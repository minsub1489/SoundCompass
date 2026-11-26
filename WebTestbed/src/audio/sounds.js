// Sound id -> files + fixed gain. Files are produced by tools/audio_build.py into public/audio.
const one = (id, kind, gain) => ({ files: [`audio/${id}.wav`], gain, kind });
const many = (id, n, kind, gain) => ({ files: Array.from({ length: n }, (_, i) => `audio/${id}_${i + 1}.wav`), gain, kind });

export const SOUNDS = {
  player_rifle_shot: one('player_rifle_shot', 'sfx', 0.9),
  rifle_shot: one('rifle_shot', 'sfx', 1),
  shotgun_shot: one('shotgun_shot', 'sfx', 1),
  sniper_shot: one('sniper_shot', 'sfx', 1),
  rifle_reload: one('rifle_reload', 'sfx', 0.7),
  shotgun_pump: one('shotgun_pump', 'sfx', 0.8),
  sniper_bolt: one('sniper_bolt', 'sfx', 0.8),
  bullet_whiz: one('bullet_whiz', 'sfx', 0.9),
  bullet_impact: one('bullet_impact', 'sfx', 0.8),
  knife_swing: one('knife_swing', 'sfx', 0.8),
  knife_hit: one('knife_hit', 'sfx', 0.9),
  step_concrete: many('step_concrete', 6, 'sfx', 0.6),
  step_dirt: many('step_dirt', 6, 'sfx', 0.6),
  step_metal: many('step_metal', 6, 'sfx', 0.6),
  land: one('land', 'sfx', 0.7),
  door_open: one('door_open', 'sfx', 0.8),
  door_close: one('door_close', 'sfx', 0.8),
  crate_pickup: one('crate_pickup', 'sfx', 0.7),
  hit_taken: one('hit_taken', 'sfx', 0.8),
  death: one('death', 'sfx', 0.9),
  amb_urban: one('amb_urban', 'amb', 1),
  amb_warehouse: one('amb_warehouse', 'amb', 1),
  amb_forest: one('amb_forest', 'amb', 1),
  bgm_urban: one('bgm_urban', 'bgm', 1),
  bgm_warehouse: one('bgm_warehouse', 'bgm', 1),
  bgm_forest: one('bgm_forest', 'bgm', 1),
};
