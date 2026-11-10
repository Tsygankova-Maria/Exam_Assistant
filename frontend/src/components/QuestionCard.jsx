export default function QuestionCard({ question, number, count }) {
  return (
    <section className="card question-card">
      <div className="question-header">
        <h2 className="card-label">Current question</h2>
        <span className="muted">
          {number} / {count}
        </span>
      </div>
      <span className="topic">{question.topic}</span>
      <p className="question-text">{question.text}</p>
    </section>
  );
}
