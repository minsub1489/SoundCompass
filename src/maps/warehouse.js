import { building, platform, freeStairs, block, merge } from './kit.js';

const S = 'metal';
const shelves = [];
for (const x of [-30, -24, -18, -12]) shelves.push(block(x, 0, -22, 1.2, 4, 14, { color: 0x6d5a3a }));
for (const x of [12, 18, 24, 30]) shelves.push(block(x, 0, 18, 1.2, 4, 10, { color: 0x6d5a3a }));

const parts = merge(
  // Outer shell: 10 m walls and a roof, so the whole map is indoors.
  { solids: [
    block(0, 0, -39.8, 80, 10, 0.4, { color: 0x4a4f55 }), block(0, 0, 39.8, 80, 10, 0.4, { color: 0x4a4f55 }),
    block(-39.8, 0, 0, 0.4, 10, 79.2, { color: 0x4a4f55 }), block(39.8, 0, 0, 0.4, 10, 79.2, { color: 0x4a4f55 }),
    block(0, 10, 0, 80, 0.4, 80, { color: 0x3a3f44 }),
  ] },
  platform({ x: 3, z: 30, w: 66, d: 8, y: 4, surface: S }), // north mezzanine
  platform({ x: 23, z: -30, w: 26, d: 8, y: 4, surface: S }), // south mezzanine
  platform({ x: 34.5, z: 0, w: 3, d: 52, y: 4, surface: S }), // east catwalk joining both
  freeStairs({ from: [-20, 0, 21.2], dir: '+z', height: 4, surface: S }),
  freeStairs({ from: [20, 0, -21.2], dir: '-z', height: 4, surface: S }),
  building({ x: -5, z: 0, w: 12, d: 10, floors: 2, doorSide: 'south', surface: S, color: 0x7d858c }),
  { solids: [
    ...shelves,
    block(10, 0, 0, 2.5, 2.6, 6, { color: 0x3d6b8a }),
    block(-25, 0, 5, 6, 2.6, 2.5, { color: 0x8a3d3d }),
    block(25, 0, -8, 2.5, 2.6, 6, { color: 0x3d8a5a }),
  ] },
);

export default {
  id: 'warehouse', name: '창고 실내', size: [80, 80], seed: 202,
  surface: S, ambience: 'amb_warehouse', bgm: 'bgm_warehouse',
  ...parts,
  playerSpawn: [0, 0, -36],
  bots: [
    { weapon: 'rifle', spawn: [-28, 0, 0], patrol: [[-28, 0, 0], [-6, 0, -12], [6, 0, 12], [-28, 0, 14]] },
    { weapon: 'shotgun', spawn: [-5, 0, 2], patrol: [[-5, 0, 2], [-5, 3, 2], [-5, 0, -8], [8, 0, -8]] },
    { weapon: 'sniper', spawn: [0, 4, 30], patrol: [[0, 4, 30]] },
    { weapon: 'knife', spawn: [30, 0, 30], patrol: [[30, 0, 30], [34.5, 4, 0], [20, 4, -30], [20, 0, -10]] },
  ],
  respawnPoints: [[-36, 0, -36], [36, 0, 36], [-36, 0, 36], [36, 0, -10], [0, 0, 20], [-36, 0, 0]],
  crateCandidates: [
    [-20, 0, -10], [-34, 0, 0], [-5, 0, 0], [-3, 3, 2], [-5, 6, 0], [-20, 4, 30], [34.5, 4, 10], [25, 4, -30],
    [15, 0, 8], [30, 0, -20], [0, 0, -30], [-34, 0, 30], [20, 0, 5],
  ],
  crateCount: 6,
};
