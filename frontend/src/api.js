async function request(path, method = 'GET') {
  const response = await fetch(`/api${path}`, {
    method,
    headers: { Accept: 'application/json' },
  });
  if (!response.ok) {
    throw new Error(`${method} /api${path} failed with status ${response.status}`);
  }
  return response.json();
}

export const api = {
  getState: () => request('/state'),
  start: () => request('/start', 'POST'),
  stop: () => request('/stop', 'POST'),
  nextQuestion: () => request('/next-question', 'POST'),
  reset: () => request('/reset', 'POST'),
};
