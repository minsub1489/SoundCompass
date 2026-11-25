import { init, NavMeshQuery } from '@recast-navigation/core';
import { threeToSoloNavMesh } from '@recast-navigation/three';

// Recast takes heights/radius in voxels (metres / cell size).
const CS = 0.2, CH = 0.1;
const CONFIG = {
  cs: CS, ch: CH, walkableSlopeAngle: 45,
  // Recast's ledge filter drops a span whose neighbours differ by more than walkableClimb in total; on 0.25 m stairs
  // that range is 0.5 m, so climb must be >= 0.5 m. The character autostep (physics.js) is raised to match.
  walkableClimb: Math.round(0.6 / CH), walkableHeight: Math.round(1.8 / CH),
  // 1 voxel (0.2 m), below the 0.35 m capsule; the character controller absorbs the corner scrape.
  walkableRadius: 1,
};
const HALF_EXTENTS = { x: 1, y: 2, z: 1 };

const v = p => ({ x: p[0], y: p[1], z: p[2] });
const a = q => [q.x, q.y, q.z];

export async function buildNav(meshes) {
  await init();
  const result = threeToSoloNavMesh(meshes, CONFIG);
  if (!result.success) throw new Error(`navmesh generation failed: ${result.error}`);
  const query = new NavMeshQuery(result.navMesh);
  return {
    path(from, to) {
      const r = query.computePath(v(from), v(to), { halfExtents: HALF_EXTENTS });
      return r.success ? r.path.map(a) : [];
    },
    closest(p) {
      const r = query.findClosestPoint(v(p), { halfExtents: HALF_EXTENTS });
      return r.success ? a(r.point) : p;
    },
  };
}
