// Oral exam preparation server.
// REST API (cpp-httplib + nlohmann/json), C++17, GCC 11.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using Millis = std::chrono::milliseconds;
namespace fs = std::filesystem;

struct Question {
    int id;
    std::string topic;
    std::string text;
};

static json readJsonFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
    try {
        return json::parse(in);
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Invalid JSON in " + path.string() + ": " + e.what());
    }
}

static std::vector<Question> parseQuestions(const json& arr) {
    if (!arr.is_array()) {
        throw std::runtime_error("questions.json must contain a JSON array");
    }
    std::vector<Question> result;
    int index = 0;
    for (const auto& item : arr) {
        ++index;
        if (!item.is_object() || !item.contains("text") || !item["text"].is_string()) {
            throw std::runtime_error("Question #" + std::to_string(index) +
                                     " must be an object with a string field \"text\"");
        }
        Question q;
        q.id = item.value("id", index);
        q.topic = item.value("topic", std::string("General"));
        q.text = item["text"].get<std::string>();
        result.push_back(std::move(q));
    }
    if (result.empty()) {
        throw std::runtime_error("questions.json is empty");
    }
    return result;
}

static std::vector<std::string> parseStudents(const json& arr) {
    if (!arr.is_array()) {
        throw std::runtime_error("students.json must contain a JSON array");
    }
    std::vector<std::string> result;
    for (const auto& item : arr) {
        if (!item.is_string()) {
            throw std::runtime_error("students.json must contain only strings");
        }
        result.push_back(item.get<std::string>());
    }
    if (result.empty()) {
        throw std::runtime_error("students.json is empty");
    }
    return result;
}

static json questionToJson(const Question& q) {
    return json{{"id", q.id}, {"topic", q.topic}, {"text", q.text}};
}

class ExamSession {
public:
    ExamSession(std::vector<Question> questions,
                std::vector<std::string> students,
                Millis turnDuration)
        : questions_(std::move(questions)),
          students_(std::move(students)),
          turn_(turnDuration),
          pausedRemaining_(turnDuration),
          rng_(std::random_device{}()) {
        reshuffleQuestions(std::nullopt);
    }

    json snapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshotLocked();
    }

    json start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            if (student_.empty()) {
                pickStudentLocked();
            }
            deadline_ = Clock::now() + pausedRemaining_;
            running_ = true;
        }
        return snapshotLocked();
    }

    json stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            pausedRemaining_ = remainingLocked();
            running_ = false;
        }
        return snapshotLocked();
    }

    json nextQuestion() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++pos_;
        if (pos_ >= order_.size()) {
            reshuffleQuestions(order_.back());
        }
        return snapshotLocked();
    }

    json reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        pausedRemaining_ = turn_;
        student_.clear();
        lastStudent_.reset();
        turnNumber_ = 0;
        return snapshotLocked();
    }

    void tick() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_ && Clock::now() >= deadline_) {
            pickStudentLocked();
            deadline_ = Clock::now() + turn_;
        }
    }

private:
    Millis remainingLocked() const {
        if (!running_) {
            return pausedRemaining_;
        }
        const auto left = std::chrono::duration_cast<Millis>(deadline_ - Clock::now());
        return std::max(left, Millis::zero());
    }

    void pickStudentLocked() {
        std::size_t idx = 0;
        if (students_.size() > 1) {
            std::uniform_int_distribution<std::size_t> dist(0, students_.size() - 1);
            do {
                idx = dist(rng_);
            } while (lastStudent_ && idx == *lastStudent_);
        }
        lastStudent_ = idx;
        student_ = students_[idx];
        ++turnNumber_;
    }

    void reshuffleQuestions(std::optional<std::size_t> avoidFirst) {
        order_.resize(questions_.size());
        std::iota(order_.begin(), order_.end(), std::size_t{0});
        std::shuffle(order_.begin(), order_.end(), rng_);
        if (avoidFirst && order_.size() > 1 && order_[0] == *avoidFirst) {
            std::swap(order_[0], order_[1]);
        }
        pos_ = 0;
    }

    json snapshotLocked() const {
        const Question& q = questions_[order_[pos_]];
        return json{
            {"running", running_},
            {"remainingMs", remainingLocked().count()},
            {"turnDurationMs", turn_.count()},
            {"student", student_.empty() ? json(nullptr) : json(student_)},
            {"turnNumber", turnNumber_},
            {"question", questionToJson(q)},
            {"questionNumber", pos_ + 1},
            {"questionCount", questions_.size()},
        };
    }

    std::mutex mutex_;
    const std::vector<Question> questions_;
    const std::vector<std::string> students_;
    const Millis turn_;

    bool running_ = false;
    Millis pausedRemaining_;
    Clock::time_point deadline_{};

    std::string student_;
    std::optional<std::size_t> lastStudent_;
    int turnNumber_ = 0;

    std::vector<std::size_t> order_;
    std::size_t pos_ = 0;

    std::mt19937 rng_;
};

struct Options {
    int port = 8080;
    fs::path dataDir = "./data";
    fs::path staticDir = "../frontend/dist";
};

static Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto needValue = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + arg);
            }
            return argv[++i];
        };
        if (arg == "--port") {
            opt.port = std::stoi(needValue());
        } else if (arg == "--data") {
            opt.dataDir = needValue();
        } else if (arg == "--static") {
            opt.staticDir = needValue();
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: exam_server [--port N] [--data DIR] [--static DIR]\n"
                         "  --port    HTTP port (default 8080)\n"
                         "  --data    folder with questions.json and students.json (default ./data)\n"
                         "  --static  built frontend to serve (default ../frontend/dist)\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    return opt;
}

static httplib::Server* g_server = nullptr;

static void onSignal(int) {
    if (g_server) {
        g_server->stop();
    }
}

static void sendJson(httplib::Response& res, const json& body, int status = 200) {
    res.status = status;
    res.set_header("Cache-Control", "no-store");
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

int main(int argc, char** argv) {
    try {
        const Options opt = parseArgs(argc, argv);

        const json questionsJson = readJsonFile(opt.dataDir / "questions.json");
        const json studentsJson = readJsonFile(opt.dataDir / "students.json");
        std::vector<Question> questions = parseQuestions(questionsJson);
        std::vector<std::string> students = parseStudents(studentsJson);

        json questionsOut = json::array();
        for (const auto& q : questions) {
            questionsOut.push_back(questionToJson(q));
        }
        const json studentsOut = students;

        const std::size_t questionCount = questions.size();
        const std::size_t studentCount = students.size();

        ExamSession session(std::move(questions), std::move(students), std::chrono::seconds(90));

        httplib::Server svr;
        g_server = &svr;
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);

        svr.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"},
        });

        svr.set_exception_handler([](const httplib::Request&, httplib::Response& res,
                                     std::exception_ptr ep) {
            std::string message = "Internal server error";
            try {
                if (ep) std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                message = e.what();
            } catch (...) {
            }
            sendJson(res, json{{"error", message}}, 500);
        });

        svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
            res.status = 204;
        });

        svr.Get("/api/questions", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, questionsOut);
        });
        svr.Get("/api/students", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, studentsOut);
        });
        svr.Get("/api/state", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, session.snapshot());
        });
        svr.Post("/api/start", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, session.start());
        });
        svr.Post("/api/stop", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, session.stop());
        });
        svr.Post("/api/next-question", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, session.nextQuestion());
        });
        svr.Post("/api/reset", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, session.reset());
        });

        if (fs::exists(opt.staticDir / "index.html")) {
            svr.set_mount_point("/", opt.staticDir.string());
            std::cout << "Serving frontend from " << fs::absolute(opt.staticDir) << "\n";
        } else {
            std::cout << "No built frontend found at " << opt.staticDir
                      << " (API only; use `npm run dev` in ./frontend)\n";
        }

        if (!svr.bind_to_port("127.0.0.1", opt.port)) {
            std::cerr << "Cannot bind to port " << opt.port
                      << " (already in use?). Try --port 8081\n";
            g_server = nullptr;
            return 1;
        }

        std::atomic<bool> alive{true};
        std::thread ticker([&] {
            while (alive) {
                session.tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        std::cout << "Loaded " << questionCount << " questions and " << studentCount
                  << " students.\n"
                  << "Listening on http://127.0.0.1:" << opt.port << "  (Ctrl+C to quit)\n";

        svr.listen_after_bind();

        alive = false;
        ticker.join();
        g_server = nullptr;
        std::cout << "Server stopped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
