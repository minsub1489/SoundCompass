import { describe, it, expect } from 'vitest';
import * as THREE from 'three';
import { MAPS } from '../src/maps/index.js';
import { expandMap } from '../src/maps/expand.js';
import { buildNav } from '../src/nav.js';

const meshesOf = map => expandMap(map).map(b => {
  const m = new THREE.Mesh(new THREE.BoxGeometry(...b.size));
  m.position.set(...b.pos); m.rotation.y = b.rotY; m.updateMatrixWorld(); return m;
});

// A path "reaches" a target when its last point is within 1 m horizontally and 1 m vertically.
const reaches = (path, t) => {
  const e = path.at(-1);
  return !!e && Math.hypot(e[0] - t[0], e[2] - t[2]) < 1 && Math.abs(e[1] - t[1]) < 1;
};

describe('내비메시', () => {
  for (const id of Object.keys(MAPS)) {
    it(`${id}: 플레이어 출발점에서 봇 지점, 상자 후보, 부활 지점으로 갈 수 있다`, async () => {
      const map = MAPS[id];
      const nav = await buildNav(meshesOf(map));
      const targets = [...map.bots.flatMap(b => [b.spawn, ...b.patrol]), ...map.crateCandidates, ...map.respawnPoints];
      const unreachable = targets.filter(t => !reaches(nav.path(map.playerSpawn, t), t));
      expect(unreachable).toEqual([]);
    });
  }
});
