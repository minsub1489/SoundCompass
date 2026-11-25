import { describe, it, expect } from 'vitest';
import { isMouseSpike } from '../src/input.js';

describe('isMouseSpike', () => {
  it('보통의 빠른 움직임은 통과시킨다', () => {
    expect(isMouseSpike(40, -12)).toBe(false);
    expect(isMouseSpike(-150, 80)).toBe(false);
  });
  it('한 이벤트에서 비정상적으로 큰 이동은 버린다', () => {
    expect(isMouseSpike(900, 3)).toBe(true);
    expect(isMouseSpike(2, -600)).toBe(true);
  });
});
