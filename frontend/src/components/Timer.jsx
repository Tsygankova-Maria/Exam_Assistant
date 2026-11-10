function formatTime(ms) {
  const totalSeconds = Math.ceil(ms / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return `${minutes}:${String(seconds).padStart(2, '0')}`;
}

export default function Timer({ remainingMs, totalMs, running }) {
  const fraction = totalMs > 0 ? Math.min(1, Math.max(0, remainingMs / totalMs)) : 0;
  const urgent = running && remainingMs <= 10000;

  return (
    <section className="card timer-card">
      <h2 className="card-label">Time left</h2>
      <div className={`timer-value${urgent ? ' urgent' : ''}`}>{formatTime(remainingMs)}</div>
      <div className="progress" role="progressbar" aria-valuemin={0} aria-valuemax={100}
           aria-valuenow={Math.round(fraction * 100)}>
        <div className={`progress-bar${urgent ? ' urgent' : ''}`}
             style={{ width: `${fraction * 100}%` }} />
      </div>
      <span className={`pill ${running ? 'pill-running' : 'pill-paused'}`}>
        {running ? 'Running' : 'Paused'}
      </span>
    </section>
  );
}
