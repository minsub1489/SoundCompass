import { building } from './kit.js';

const house = building({ x: 0, z: 0, w: 12, d: 10, floors: 2, doorSide: 'south', surface: 'concrete' });

export default {
  id: 'practice', name: '연습장', size: [40, 40], seed: 7,
  surface: 'concrete', ambience: 'amb_urban', bgm: 'bgm_urban',
  solids: [...house.solids, { pos: [10, 0.6, -10], size: [3, 1.2, 1.2] }, { pos: [-12, 1, 8], size: [2, 2, 6] }],
  stairs: house.stairs,
  doors: house.doors,
  playerSpawn: [0, 0, -16],
  bots: [
    { weapon: 'rifle', spawn: [12, 0, 14], patrol: [[12, 0, 14], [-12, 0, 14], [-14, 0, -2], [14, 0, -2]] },
    { weapon: 'knife', spawn: [-15, 0, -15], patrol: [[-15, 0, -15], [0, 0, -9], [15, 0, -15]] },
  ],
  respawnPoints: [[17, 0, 17], [-17, 0, 17], [17, 0, -17], [-17, 0, 0]],
  crateCandidates: [[3, 0, 2], [-3, 3, 3], [3, 3, -2], [2, 6, 2], [-14, 0, 12], [14, 0, -12]],
  crateCount: 3,
};
