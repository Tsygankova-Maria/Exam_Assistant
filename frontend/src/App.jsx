import { useExamState } from './hooks/useExamState.js';
import Timer from './components/Timer.jsx';
import StudentCard from './components/StudentCard.jsx';
import QuestionCard from './components/QuestionCard.jsx';
import Controls from './components/Controls.jsx';
import './App.css';

export default function App() {
  const { state, remainingMs, error, busy, actions } = useExamState();

  if (!state) {
    return (
      <main className="app">
        <h1>Oral Exam Prep</h1>
        <p className="status">
          {error ? `Cannot reach the backend: ${error}` : 'Connecting to the backend…'}
        </p>
      </main>
    );
  }

  return (
    <main className="app">

      {error && <div className="banner">Connection problem: {error}. Retrying…</div>}

      <QuestionCard
        question={state.question}
        number={state.questionNumber}
        count={state.questionCount}
      />

      <div className="row">
        <StudentCard student={state.student} turnNumber={state.turnNumber} />
        <Timer
          remainingMs={remainingMs}
          totalMs={state.turnDurationMs}
          running={state.running}
        />
      </div>

      <Controls
        running={state.running}
        busy={busy}
        onStart={actions.start}
        onStop={actions.stop}
        onNext={actions.nextQuestion}
        onReset={actions.reset}
      />
    </main>
  );
}
