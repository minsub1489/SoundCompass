import { describe, it, expect } from 'vitest';
import { isMouseSpike } from '../src/input.js';

// isMouseSpike(dx, dy, prevMag): prevMag = size of the previous accepted mouse event.
describe('isMouseSpike', () => {
  it('보통의 움직임은 통과시킨다', () => {
    expect(isMouseSpike(40, -12, 30)).toBe(false);
    expect(isMouseSpike(-150, 80, 120)).toBe(false);
  });
  it('점점 빨라지는 빠른 회전은 값이 커도 통과시킨다', () => {
    expect(isMouseSpike(600, 0, 400)).toBe(false);
    expect(isMouseSpike(-1200, 40, 700)).toBe(false);
  });
  it('거의 멈춘 상태에서 갑자기 튀는 값은 버린다', () => {
    expect(isMouseSpike(900, 3, 5)).toBe(true);
    expect(isMouseSpike(2, -600, 10)).toBe(true);
  });
});
