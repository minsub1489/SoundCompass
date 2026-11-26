import { WEAPONS, BOT_WEAPONS } from '../config.js';

export const SURFACES = ['concrete', 'metal', 'dirt'];
export const SURFACE_COLOR = { concrete: 0x8a8a8a, metal: 0x5f6b75, dirt: 0x5b6b3a };
const DIRS = { '+x': [1, 0], '-x': [-1, 0], '+z': [0, 1], '-z': [0, -1] };
const BOUNDARY_H = 4;

const box = (pos, size, surface, kind, color, rotY = 0) =>
  ({ pos, size, rotY, surface, color: color ?? SURFACE_COLOR[surface], kind });

// Map data -> flat list of boxes (ground, boundary walls, solids, stair steps). Doors are separate.
export function expandMap(map) {
  const [w, d] = map.size;
  const out = [box([0, -0.5, 0], [w, 1, d], map.surface, 'ground', SURFACE_COLOR[map.surface])];
  const t = 0.5, hy = BOUNDARY_H / 2;
  out.push(
    box([0, hy, -d / 2 - t / 2], [w + 2 * t, BOUNDARY_H, t], 'concrete', 'boundary', 0x333333),
    box([0, hy, d / 2 + t / 2], [w + 2 * t, BOUNDARY_H, t], 'concrete', 'boundary', 0x333333),
    box([-w / 2 - t / 2, hy, 0], [t, BOUNDARY_H, d], 'concrete', 'boundary', 0x333333),
    box([w / 2 + t / 2, hy, 0], [t, BOUNDARY_H, d], 'concrete', 'boundary', 0x333333),
  );
  for (const s of map.solids ?? []) out.push(box(s.pos, s.size, s.surface ?? map.surface, 'solid', s.color, s.rotY ?? 0));
  for (const st of map.stairs ?? []) {
    const [dx, dz] = DIRS[st.dir];
    const surface = st.surface ?? map.surface;
    for (let k = 1; k <= st.steps; k++) {
      const h = k * st.rise, along = st.run * (k - 0.5);
      const size = dx ? [st.run, h, st.width] : [st.width, h, st.run];
      out.push(box([st.from[0] + dx * along, st.from[1] + h / 2, st.from[2] + dz * along], size, surface, 'step'));
    }
  }
  return out;
}

export function validateMap(map, { isPractice }) {
  const errors = [];
  const [w, d] = map.size ?? [0, 0];
  const inside = (p, what) => {
    if (!Array.isArray(p) || p.length !== 3 || Math.abs(p[0]) > w / 2 || Math.abs(p[2]) > d / 2) errors.push(`${what} out of bounds: ${p}`);
  };
  inside(map.playerSpawn, 'playerSpawn');
  (map.bots ?? []).forEach((b, i) => {
    if (!BOT_WEAPONS.includes(b.weapon) || !WEAPONS[b.weapon]) errors.push(`bot ${i} has unknown weapon ${b.weapon}`);
    inside(b.spawn, `bot ${i} spawn`);
    if (!b.patrol?.length) errors.push(`bot ${i} has no patrol`);
    (b.patrol ?? []).forEach((p, j) => inside(p, `bot ${i} patrol ${j}`));
  });
  const weapons = (map.bots ?? []).map(b => b.weapon).sort().join(',');
  if (isPractice) {
    if (!(map.bots?.length >= 1 && map.bots.length <= 2)) errors.push('practice map needs 1-2 bots');
  } else if (weapons !== [...BOT_WEAPONS].sort().join(',')) {
    errors.push(`main map needs one bot per weapon (${BOT_WEAPONS}), got ${weapons}`);
  }
  if (!((map.crateCandidates?.length ?? 0) >= map.crateCount * 2)) errors.push('crateCandidates must be at least crateCount * 2');
  (map.crateCandidates ?? []).forEach((p, i) => inside(p, `crate ${i}`));
  if (!((map.respawnPoints?.length ?? 0) >= 3)) errors.push('need at least 3 respawnPoints');
  (map.respawnPoints ?? []).forEach((p, i) => inside(p, `respawn ${i}`));
  if (!SURFACES.includes(map.surface)) errors.push(`unknown map surface ${map.surface}`);
  for (const s of [...(map.solids ?? []), ...(map.stairs ?? [])]) {
    if (s.surface && !SURFACES.includes(s.surface)) errors.push(`unknown surface ${s.surface}`);
  }
  (map.solids ?? []).forEach((s, i) => inside(s.pos, `solid ${i}`));
  (map.stairs ?? []).forEach((s, i) => inside(s.from, `stairs ${i}`));
  (map.doors ?? []).forEach((s, i) => inside(s.pos, `door ${i}`));
  if (!map.ambience || !map.bgm) errors.push('ambience and bgm are required');
  return errors;
}
