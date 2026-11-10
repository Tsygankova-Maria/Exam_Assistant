export default function Controls({ running, busy, onStart, onStop, onNext, onReset }) {
  return (
    <div className="controls">
      <button className="btn btn-start" onClick={onStart} disabled={busy || running}>
        ▶ Start
      </button>
      <button className="btn btn-stop" onClick={onStop} disabled={busy || !running}>
        ⏸ Stop
      </button>
      <button className="btn btn-next" onClick={onNext} disabled={busy}>
        Next Question →
      </button>
      <button className="btn btn-reset" onClick={onReset} disabled={busy}>
        Reset
      </button>
    </div>
  );
}
