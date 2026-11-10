import { useCallback, useEffect, useRef, useState } from 'react';
import { api } from '../api.js';

const POLL_MS = 500; // how often we sync with the backend
const TICK_MS = 100; // how often the countdown is redrawn

export function useExamState() {
  const [state, setState] = useState(null);
  const [error, setError] = useState(null);
  const [busy, setBusy] = useState(false);
  const [, forceRender] = useState(0);
  const receivedAt = useRef(performance.now());

  const apply = useCallback((snapshot) => {
    receivedAt.current = performance.now();
    setState(snapshot);
    setError(null);
  }, []);

  useEffect(() => {
    let cancelled = false;
    const poll = async () => {
      try {
        const snapshot = await api.getState();
        if (!cancelled) apply(snapshot);
      } catch (e) {
        if (!cancelled) setError(e.message);
      }
    };
    poll();
    const id = setInterval(poll, POLL_MS);
    return () => {
      cancelled = true;
      clearInterval(id);
    };
  }, [apply]);

  useEffect(() => {
    const id = setInterval(() => forceRender((n) => n + 1), TICK_MS);
    return () => clearInterval(id);
  }, []);

  const run = useCallback(
    async (fn) => {
      setBusy(true);
      try {
        apply(await fn());
      } catch (e) {
        setError(e.message);
      } finally {
        setBusy(false);
      }
    },
    [apply]
  );

  let remainingMs = 0;
  if (state) {
    remainingMs = state.running
      ? Math.max(0, state.remainingMs - (performance.now() - receivedAt.current))
      : state.remainingMs;
  }

  return {
    state,
    remainingMs,
    error,
    busy,
    actions: {
      start: () => run(api.start),
      stop: () => run(api.stop),
      nextQuestion: () => run(api.nextQuestion),
      reset: () => run(api.reset),
    },
  };
}
