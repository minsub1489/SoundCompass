import { describe, it, expect } from 'vitest';
import { existsSync } from 'node:fs';
import { SOUNDS } from '../src/audio/sounds.js';
import { WEAPONS } from '../src/config.js';
import { MAPS } from '../src/maps/index.js';

describe('SOUNDS', () => {
  it('참조하는 파일이 모두 public/audio에 있다', () => {
    const missing = Object.values(SOUNDS).flatMap(s => s.files).filter(f => !existsSync(`public/${f}`));
    expect(missing).toEqual([]);
  });
  it('필수 id가 모두 있다', () => {
    for (const id of ['player_rifle_shot', 'rifle_shot', 'shotgun_shot', 'sniper_shot', 'rifle_reload', 'shotgun_pump', 'sniper_bolt', 'knife_swing', 'knife_hit', 'step_concrete', 'step_dirt', 'step_metal', 'land', 'door_open', 'door_close', 'crate_pickup', 'hit_taken', 'death', 'amb_urban', 'amb_warehouse', 'amb_forest', 'bgm_urban', 'bgm_warehouse', 'bgm_forest'])
      expect(SOUNDS[id], id).toBeDefined();
  });
  it('무기와 맵이 쓰는 소리 id가 모두 있다', () => {
    const ids = Object.values(WEAPONS).flatMap(w => [w.fireSound, w.reloadSound, w.afterFireSound]).filter(Boolean);
    for (const m of Object.values(MAPS)) ids.push(m.ambience, m.bgm);
    for (const id of ids) expect(SOUNDS[id], id).toBeDefined();
  });
});
