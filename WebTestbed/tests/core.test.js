import { describe, it, expect } from 'vitest';
import { createRng } from '../src/rng.js';
import { stepCount } from '../src/fixedStep.js';

describe('createRng', () => {
  it('같은 시드는 같은 수열을 낸다', () => {
    const a = createRng(42), b = createRng(42);
    const sa = [a(), a(), a()], sb = [b(), b(), b()];
    expect(sa).toEqual(sb);
    sa.forEach(v => { expect(v).toBeGreaterThanOrEqual(0); expect(v).toBeLessThan(1); });
  });
  it('다른 시드는 다른 수열을 낸다', () => {
    expect(createRng(1)()).not.toBe(createRng(2)());
  });
});

describe('stepCount', () => {
  it('누적 시간을 고정 간격 단위로 나눈다', () => {
    const r = stepCount(0, 0.06, 1 / 60, 5);
    expect(r.steps).toBe(3);
    expect(r.accumulator).toBeCloseTo(0.06 - 3 / 60, 6);
  });
  it('큰 프레임 간격은 상한에서 자르고 남은 시간을 버린다', () => {
    const r = stepCount(0, 2.0, 1 / 60, 5);
    expect(r.steps).toBe(5);
    expect(r.accumulator).toBe(0);
  });
});
