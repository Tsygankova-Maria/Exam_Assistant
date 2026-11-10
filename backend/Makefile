CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Ithird_party
LDFLAGS  := -pthread

TARGET   := exam_server
SRC      := src/main.cpp

HTTPLIB_VERSION := v0.18.3
JSON_VERSION    := v3.11.3

all: $(TARGET)

$(TARGET): $(SRC) third_party/httplib.h third_party/json.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $@ $(LDFLAGS)

third_party/httplib.h:
	mkdir -p third_party
	curl -fsSL -o $@ https://raw.githubusercontent.com/yhirose/cpp-httplib/$(HTTPLIB_VERSION)/httplib.h

third_party/json.hpp:
	mkdir -p third_party
	curl -fsSL -o $@ https://github.com/nlohmann/json/releases/download/$(JSON_VERSION)/json.hpp

run: $(TARGET)
	./$(TARGET) --port 8080 --data ./data --static ../frontend/dist

clean:
	rm -f $(TARGET)

.PHONY: all run clean
