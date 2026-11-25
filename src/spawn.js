const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);

const farthest = (indices, points, from) =>
  indices.reduce((best, i) => (best < 0 || dist(points[i], from) > dist(points[best], from) ? i : best), -1);

export function nextCrateIndex(candidates, occupied, playerPos, rng, minDistM) {
  const free = candidates.map((_, i) => i).filter(i => !occupied.has(i));
  const far = free.filter(i => dist(candidates[i], playerPos) >= minDistM);
  if (far.length) return far[Math.floor(rng() * far.length)];
  return farthest(free, candidates, playerPos);
}

export function initialCrateIndices(candidates, count, rng) {
  const pool = candidates.map((_, i) => i);
  for (let i = pool.length - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    [pool[i], pool[j]] = [pool[j], pool[i]];
  }
  return pool.slice(0, count);
}

export function chooseRespawnIndex(points, playerPos, isVisible, minDistM) {
  const all = points.map((_, i) => i);
  const hidden = all.filter(i => !isVisible(points[i]));
  const good = hidden.filter(i => dist(points[i], playerPos) >= minDistM);
  if (good.length) return farthest(good, points, playerPos);
  if (hidden.length) return farthest(hidden, points, playerPos);
  return farthest(all, points, playerPos);
}
