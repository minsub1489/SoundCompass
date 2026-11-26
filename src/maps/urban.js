import { building, block, merge } from './kit.js';

const S = 'concrete';
const parts = merge(
  building({ x: -22, z: -20, w: 14, d: 10, floors: 3, doorSide: 'north', surface: S }),
  building({ x: 20, z: -22, w: 12, d: 10, floors: 2, doorSide: 'north', surface: S, color: 0x8f8678 }),
  building({ x: -20, z: 20, w: 12, d: 12, floors: 2, doorSide: 'south', surface: S, color: 0x8a7f72 }),
  building({ x: 22, z: 18, w: 14, d: 10, floors: 3, doorSide: 'south', surface: S }),
  building({ x: 0, z: 0, w: 10, d: 8, floors: 2, doorSide: 'east', surface: S, color: 0xa09684 }),
  {
    solids: [
      block(8, 0, 12, 2.5, 2.6, 6, { color: 0x3d6b8a }), // container
      block(-8, 0, -10, 6, 2.6, 2.5, { color: 0x8a3d3d }), // container
      block(-5, 0, -15, 6, 1.2, 0.4), // low wall
      block(12, 0, -8, 0.4, 1.2, 6), // low wall
      block(-32, 0, 0, 2, 1.5, 4.5, { color: 0x2c2c2c }), // car
      block(32, 0, -5, 2, 1.5, 4.5, { color: 0x5a5a20 }), // car
      block(-8, 0, 25, 0.4, 3, 12), // alley wall
      block(10, 0, -32, 0.4, 3, 10), // alley wall
    ],
  },
);

export default {
  id: 'urban', name: '도심 골목', size: [80, 80], seed: 101,
  surface: S, ambience: 'amb_urban', bgm: 'bgm_urban',
  ...parts,
  playerSpawn: [0, 0, -36],
  bots: [
    { weapon: 'rifle', spawn: [-30, 0, 32], patrol: [[-30, 0, 32], [0, 0, 30], [30, 0, 32], [30, 0, 5], [0, 0, 8], [-30, 0, 8]] },
    { weapon: 'shotgun', spawn: [-22, 0, -18], patrol: [[-22, 0, -18], [-22, 3, -18], [-10, 0, -12], [-30, 0, -8]] },
    { weapon: 'sniper', spawn: [22, 9, 20], patrol: [[22, 9, 20]] },
    { weapon: 'knife', spawn: [25, 0, -35], patrol: [[25, 0, -35], [35, 0, -10], [6, 0, -12], [-5, 0, -25]] },
  ],
  respawnPoints: [[-36, 0, 36], [36, 0, 36], [-36, 0, -8], [36, 0, -30], [0, 0, 36], [-36, 0, -36]],
  crateCandidates: [
    [-20, 0, -21], [-24, 3, -20], [-20, 6, -20], [-20, 9, -21], [20, 0, -22], [18, 3, -24], [-20, 0, 22], [-18, 3, 20],
    [22, 0, 18], [20, 6, 17], [0, 0, 0], [2, 3, 1], [-30, 0, 4], [30, 0, 2], [0, 0, -25], [0, 0, 28],
  ],
  crateCount: 6,
};
