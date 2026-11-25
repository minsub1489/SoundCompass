import { describe, it, expect } from 'vitest';
import { nextCrateIndex, initialCrateIndices, chooseRespawnIndex } from '../src/spawn.js';
import { createRng } from '../src/rng.js';

const pts = [[0, 0, 0], [5, 0, 0], [20, 0, 0], [40, 0, 0]];

describe('nextCrateIndex', () => {
  it('점유되지 않고 충분히 먼 후보만 고른다', () => {
    const rng = createRng(1);
    for (let i = 0; i < 20; i++) expect([2]).toContain(nextCrateIndex(pts, new Set([3]), [0, 0, 0], rng, 10));
  });
  it('먼 후보가 없으면 비어 있는 가장 먼 후보', () => {
    expect(nextCrateIndex(pts, new Set([2, 3]), [0, 0, 0], createRng(1), 10)).toBe(1);
  });
  it('모두 점유면 -1', () => {
    expect(nextCrateIndex(pts, new Set([0, 1, 2, 3]), [0, 0, 0], createRng(1), 10)).toBe(-1);
  });
});

describe('initialCrateIndices', () => {
  it('서로 다른 인덱스를 count개 고른다', () => {
    const idx = initialCrateIndices(pts, 3, createRng(7));
    expect(idx).toHaveLength(3);
    expect(new Set(idx).size).toBe(3);
  });
});

describe('chooseRespawnIndex', () => {
  it('보이지 않고 먼 지점 중 가장 먼 곳', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], p => p[0] === 40, 15)).toBe(2);
  });
  it('모두 보이면 가장 먼 곳', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], () => true, 15)).toBe(3);
  });
  it('보이지 않는 곳이 모두 가까우면 그중 가장 먼 곳', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], p => p[0] >= 20, 15)).toBe(1);
  });
  it('다른 봇이 있는 지점은 피한다', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], () => false, 15, [[40, 0, 1]])).toBe(2);
  });
  it('모든 지점에 봇이 있으면 피하지 않고 고른다', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], () => false, 15, pts)).toBe(3);
  });
});
