import { describe, it, expect } from 'vitest';
import { createGun } from '../src/weapons.js';
import { WEAPONS } from '../src/config.js';

describe('createGun', () => {
  it('발사 간격을 지킨다', () => {
    const g = createGun(WEAPONS.playerRifle);
    expect(g.tryFire(0)).toBe(true);
    expect(g.tryFire(0.05)).toBe(false);
    expect(g.tryFire(0.13)).toBe(true);
  });
  it('탄창이 비면 자동 재장전하고 끝나면 다시 쏜다', () => {
    const g = createGun({ ...WEAPONS.playerRifle, magSize: 2 });
    g.tryFire(0); g.tryFire(1);
    expect(g.tryFire(2)).toBe(false);
    expect(g.reloading).toBe(true);
    g.update(2 + WEAPONS.playerRifle.reloadS + 0.01);
    expect(g.ammo).toBe(2);
    expect(g.tryFire(5)).toBe(true);
  });
  it('재장전 중에는 쏠 수 없다', () => {
    const g = createGun(WEAPONS.playerRifle);
    g.tryFire(0);
    expect(g.startReload(1)).toBe(true);
    expect(g.tryFire(1.5)).toBe(false);
  });
  it('탄창 없는 무기는 탄 제한이 없다', () => {
    const k = createGun(WEAPONS.knife);
    for (let i = 0; i < 50; i++) expect(k.tryFire(i)).toBe(true);
  });
});
