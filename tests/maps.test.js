import { describe, it, expect } from 'vitest';
import { expandMap, validateMap } from '../src/maps/expand.js';
import { MAPS } from '../src/maps/index.js';

describe('expandMap', () => {
  it('계단을 칸마다 박스로 펼친다', () => {
    const boxes = expandMap({ size: [20, 20], surface: 'concrete', solids: [], stairs: [{ from: [0, 0, 0], dir: '+x', steps: 4, rise: 0.25, run: 0.3, width: 1.5, surface: 'concrete' }] });
    const steps = boxes.filter(b => b.kind === 'step');
    expect(steps).toHaveLength(4);
    expect(Math.max(...steps.map(b => b.pos[1] + b.size[1] / 2))).toBeCloseTo(1.0);
    expect(steps[3].pos[0]).toBeGreaterThan(steps[0].pos[0]);
  });
  it('바닥과 맵 경계벽을 넣는다', () => {
    const boxes = expandMap({ size: [20, 30], surface: 'dirt', solids: [], stairs: [] });
    const ground = boxes.find(b => b.kind === 'ground');
    expect(ground.size).toEqual([20, 1, 30]);
    expect(ground.pos[1] + ground.size[1] / 2).toBe(0);
    expect(ground.surface).toBe('dirt');
    expect(boxes.filter(b => b.kind === 'boundary')).toHaveLength(4);
  });
});

describe('validateMap', () => {
  it('연습장은 오류가 없다', () => {
    expect(validateMap(MAPS.practice, { isPractice: true })).toEqual([]);
  });
  it('범위 밖 좌표와 부족한 후보를 잡아낸다', () => {
    const bad = { ...MAPS.practice, playerSpawn: [999, 0, 0], crateCandidates: [[0, 0, 0]] };
    expect(validateMap(bad, { isPractice: true }).length).toBeGreaterThanOrEqual(2);
  });
  it('본 맵은 무기별 봇이 한 명씩 있어야 한다', () => {
    const bad = { ...MAPS.practice };
    expect(validateMap(bad, { isPractice: false }).some(e => e.includes('bot'))).toBe(true);
  });
});

describe('본 맵', () => {
  for (const id of ['urban', 'warehouse', 'forest']) {
    it(`${id}는 오류가 없다`, () => {
      expect(MAPS[id], id).toBeDefined();
      expect(validateMap(MAPS[id], { isPractice: false })).toEqual([]);
    });
  }
  it('세 맵의 크기와 상자 수가 같다', () => {
    const v = ['urban', 'warehouse', 'forest'].map(id => [MAPS[id].size.join('x'), MAPS[id].crateCount]);
    expect(new Set(v.map(String)).size).toBe(1);
  });
  it('세 맵 모두 높이 2.5 m 이상인 지점이 있다', () => {
    for (const id of ['urban', 'warehouse', 'forest']) {
      const pts = [...MAPS[id].crateCandidates, ...MAPS[id].bots.flatMap(b => b.patrol)];
      expect(Math.max(...pts.map(p => p[1])), id).toBeGreaterThanOrEqual(2.5);
    }
  });
});
