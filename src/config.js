export const ROUND_SECONDS = 180;
export const PRACTICE_SECONDS = 90;
export const FIXED_DT = 1 / 60;
export const MAX_STEPS_PER_FRAME = 5;

// Same values as the Unreal SoundCompass occlusion settings.
export const OCCLUSION = { gain: 0.35, cutoffHz: 1500, openCutoffHz: 20000, rampSeconds: 0.15, updateSeconds: 0.1 };

export const BOT_RESPAWN_SECONDS = 10;
export const PLAYER_MAX_HEALTH = 100;
export const BOT_MAX_HEALTH = 100;
export const MIN_RESPAWN_DIST_M = 15;
export const MIN_CRATE_DIST_M = 10;
export const SHOT_HEARING_M = 40;
export const BOT_FOV_DEG = 110;
export const BOT_REACTION_S = 0.4;
export const BOT_LOST_SIGHT_S = 8;

// Starting values; tuned in Task 10.
export const WEAPONS = {
  playerRifle: { id: 'playerRifle', damage: 25, intervalS: 0.12, magSize: 30, reloadS: 2.0, rangeM: 120, spreadDeg: 0.5, fireSound: 'player_rifle_shot', reloadSound: 'rifle_reload', afterFireSound: null },
  rifle: { id: 'rifle', damage: 10, intervalS: 0.12, magSize: 30, reloadS: 2.2, rangeM: 45, spreadDeg: 3, fireSound: 'rifle_shot', reloadSound: 'rifle_reload', afterFireSound: null },
  shotgun: { id: 'shotgun', damage: 55, intervalS: 1.0, magSize: 6, reloadS: 3.0, rangeM: 12, spreadDeg: 6, fireSound: 'shotgun_shot', reloadSound: 'shotgun_pump', afterFireSound: 'shotgun_pump' },
  sniper: { id: 'sniper', damage: 70, intervalS: 2.2, magSize: 5, reloadS: 3.5, rangeM: 90, spreadDeg: 0.8, fireSound: 'sniper_shot', reloadSound: 'sniper_bolt', afterFireSound: 'sniper_bolt' },
  knife: { id: 'knife', damage: 30, intervalS: 0.8, magSize: 0, reloadS: 0, rangeM: 1.8, spreadDeg: 0, fireSound: 'knife_swing', reloadSound: null, afterFireSound: null },
};

export const BOT_WEAPONS = ['rifle', 'shotgun', 'sniper', 'knife'];
