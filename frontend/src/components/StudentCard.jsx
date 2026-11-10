export default function StudentCard({ student, turnNumber }) {
  return (
    <section className="card student-card">
      <h2 className="card-label">Current student</h2>
      {student ? (
        <>
          <div key={turnNumber} className="student-name flash">{student}</div>
          <div className="muted">Turn #{turnNumber}</div>
        </>
      ) : (
        <div className="student-name placeholder">Press Start to begin</div>
      )}
    </section>
  );
}
