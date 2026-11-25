import { BOT_REACTION_S, BOT_LOST_SIGHT_S } from './config.js';

const ARRIVE_M = 1;
const dist2D = (a, b) => Math.hypot(a[0] - b[0], a[2] - b[2]);

export const initialBotState = () => ({ mode: 'patrol', patrolIndex: 0, lastKnown: null, lostTimer: 0, reactTimer: 0 });

// Pure decision step: perception in, next state + where to move + whether to fire out.
export function decideBot(state, p, weapon, dt) {
  const s = { ...state };
  const melee = weapon.magSize === 0;

  if (p.seesPlayer) {
    s.lastKnown = p.playerPos;
    s.lostTimer = 0;
    s.reactTimer += dt;
    if (p.distance <= weapon.rangeM) {
      s.mode = 'attack';
      return { state: s, moveTo: melee ? p.playerPos : null, fire: s.reactTimer >= BOT_REACTION_S };
    }
    if (p.isSniper) { s.mode = 'attack'; return { state: s, moveTo: null, fire: false }; }
    s.mode = 'chase';
    return { state: s, moveTo: p.playerPos, fire: false };
  }

  s.reactTimer = 0;
  if (p.heardShotAt && !p.isSniper) { s.lastKnown = p.heardShotAt; s.mode = 'chase'; s.lostTimer = 0; }
  if (s.mode !== 'patrol') {
    s.lostTimer += dt;
    if (!p.isSniper && s.lastKnown && s.lostTimer <= BOT_LOST_SIGHT_S) {
      s.mode = 'chase';
      return { state: s, moveTo: s.lastKnown, fire: false };
    }
    Object.assign(s, { mode: 'patrol', lastKnown: null, lostTimer: 0 });
  }

  let target = p.patrol[s.patrolIndex % p.patrol.length];
  if (dist2D(p.botPos, target) < ARRIVE_M && p.patrol.length > 1) {
    s.patrolIndex = (s.patrolIndex + 1) % p.patrol.length;
    target = p.patrol[s.patrolIndex];
  }
  return { state: s, moveTo: target, fire: false };
}
