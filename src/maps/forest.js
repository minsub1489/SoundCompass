import { building, platform, freeStairs, terracedHill, block, merge } from './kit.js';

const S = 'dirt';
const trunk = (x, z) => block(x, 0, z, 0.6, 6, 0.6, { color: 0x5b4127 });
const rock = (x, z, s = 2) => block(x, 0, z, s, 1.5, s, { color: 0x6f6f6f });

const parts = merge(
  terracedHill({ x: -20, z: -15, size: 16, levels: 4, surface: S }),
  terracedHill({ x: 22, z: 20, size: 16, levels: 4, surface: S }),
  platform({ x: 25, z: -25, w: 4, d: 4, y: 5, surface: S, color: 0x6b5236 }), // watchtower
  freeStairs({ from: [25, 0, -33], dir: '+z', height: 5, surface: S, color: 0x6b5236 }),
  { solids: [block(-25, 0, 25, 20, 4, 10, { color: 0x5e5a52 })] }, // cliff
  freeStairs({ from: [-25, 0, 15.2], dir: '+z', height: 4, surface: S, color: 0x5e5a52 }),
  building({ x: 5, z: 5, w: 10, d: 8, floors: 2, doorSide: 'south', surface: 'concrete', color: 0x7a5c3c }), // cabin
  { solids: [
    trunk(-8, -28), trunk(-30, -32), trunk(12, -18), trunk(-12, 8), trunk(-34, 12), trunk(15, 32),
    trunk(32, 8), trunk(-4, 30), trunk(8, -32), trunk(-26, 0), trunk(34, -12), trunk(18, 0),
    rock(-14, -30), rock(28, -2, 2.5), rock(-6, 18), rock(12, 26), rock(-32, -6),
  ] },
);

export default {
  id: 'forest', name: '야외 숲', size: [80, 80], seed: 303,
  surface: S, ambience: 'amb_forest', bgm: 'bgm_forest',
  ...parts,
  playerSpawn: [0, 0, -36],
  bots: [
    { weapon: 'rifle', spawn: [-30, 0, -30], patrol: [[-30, 0, -30], [-20, 2, -15], [0, 0, -20], [-35, 0, 0]] },
    { weapon: 'shotgun', spawn: [5, 0, 5], patrol: [[5, 0, 5], [5, 3, 5], [15, 0, -5], [-5, 0, 15]] },
    { weapon: 'sniper', spawn: [25, 5, -25], patrol: [[25, 5, -25]] },
    { weapon: 'knife', spawn: [30, 0, 30], patrol: [[30, 0, 30], [22, 2, 20], [0, 0, 30], [-25, 4, 25], [10, 0, 20]] },
  ],
  respawnPoints: [[-36, 0, 36], [36, 0, 36], [36, 0, 0], [-36, 0, -36], [0, 0, 36], [36, 0, -36]],
  crateCandidates: [
    [-20, 2, -15], [22, 2, 20], [-25, 4, 27], [-30, 4, 23], [5, 0, 7], [7, 3, 4], [5, 6, 5], [-10, 0, 0],
    [15, 0, -15], [30, 0, 10], [-30, 0, 10], [0, 0, 25], [10, 0, -30],
  ],
  crateCount: 6,
};
