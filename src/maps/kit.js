// Helpers that turn a few numbers into solids/stairs/doors for map data files.
export const FLOOR_HEIGHT = 3;
const WALL_T = 0.3, SLAB_T = 0.2;
const DOOR_W = 1.2, DOOR_H = 2.2, WIN_W = 1.2, WIN_Y0 = 1.0, WIN_Y1 = 2.0;
// 1.6 m wide: recast drops the open-side ledge and erodes by the agent radius, so narrower stairs lose their navmesh.
const STAIR = { steps: 12, rise: 0.25, run: 0.3, width: 1.6 };

export const box = (x, y, z, w, h, d, extra = {}) => ({ pos: [x, y, z], size: [w, h, d], ...extra });
// Box resting on height y0 (pos is the box centre).
export const block = (x, y0, z, w, h, d, extra = {}) => box(x, y0 + h / 2, z, w, h, d, extra);

// One axis-aligned wall of length `len` centred at (cx, cz), along x or z, with an optional rectangular opening.
function wallSegments(cx, cz, along, len, y0, h, opening, extra) {
  const mk = (offset, segLen, sy0, sh) => along === 'x'
    ? block(cx + offset, sy0, cz, segLen, sh, WALL_T, extra)
    : block(cx, sy0, cz + offset, WALL_T, sh, segLen, extra);
  if (!opening) return [mk(0, len, y0, h)];
  const { w, y: oy0, top: oy1 } = opening;
  const sideLen = (len - w) / 2;
  const out = [mk(-(w / 2 + sideLen / 2), sideLen, y0, h), mk(w / 2 + sideLen / 2, sideLen, y0, h)];
  if (oy0 > 0) out.push(mk(0, w, y0, oy0));
  if (oy1 < h) out.push(mk(0, w, y0 + oy1, h - oy1));
  return out;
}

// Raised floor (top at y) on four corner pillars: mezzanines, catwalks, tower tops.
export function platform({ x, z, w, d, y, surface, color = 0x707a80 }) {
  const extra = { surface, color };
  const solids = [block(x, y - SLAB_T, z, w, SLAB_T, d, extra)];
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    solids.push(block(x + sx * (w / 2 - 0.2), 0, z + sz * (d / 2 - 0.2), 0.3, y - SLAB_T, 0.3, extra));
  }
  return { solids, stairs: [], doors: [] };
}

// Free-standing stairs climbing `height` from `from` in `dir`, walled on both sides so they are entered from the front.
export function freeStairs({ from, dir, height, width = STAIR.width, surface, color = 0x666666 }) {
  const steps = Math.round(height / STAIR.rise), length = steps * STAIR.run;
  const along = { '+x': [1, 0], '-x': [-1, 0], '+z': [0, 1], '-z': [0, -1] }[dir];
  const cx = from[0] + along[0] * length / 2, cz = from[2] + along[1] * length / 2;
  const off = width / 2 + 0.075, wallH = height + 1;
  const solids = along[0]
    ? [block(cx, from[1], cz - off, length, wallH, 0.15, { surface, color }), block(cx, from[1], cz + off, length, wallH, 0.15, { surface, color })]
    : [block(cx - off, from[1], cz, 0.15, wallH, length, { surface, color }), block(cx + off, from[1], cz, 0.15, wallH, length, { surface, color })];
  return { solids, stairs: [{ from, dir, steps, rise: STAIR.rise, run: STAIR.run, width, surface }], doors: [] };
}

// Stack of shrinking 0.5 m terraces (within the 0.55 m autostep), e.g. a hill.
export function terracedHill({ x, z, size, levels, surface, color = 0x4f5f33 }) {
  const solids = [];
  for (let k = 0; k < levels; k++) {
    const s = size - (size / levels) * k;
    solids.push(block(x, 0.5 * k, z, s, 0.5, s, { surface, color }));
  }
  return { solids, stairs: [], doors: [] };
}

// Merge several kit results into one { solids, stairs, doors }.
export const merge = (...parts) => ({
  solids: parts.flatMap(p => p.solids ?? []),
  stairs: parts.flatMap(p => p.stairs ?? []),
  doors: parts.flatMap(p => p.doors ?? []),
});

/**
 * Rectangular building: `floors` storeys, a door on `doorSide` (ground floor), a window centred on every other
 * wall of every floor, interior stairs alternating west/east per floor, and a flat roof with a low parapet.
 */
export function building({ x, z, w, d, floors, doorSide = 'south', surface, color = 0x9a8f80, y0 = 0 }) {
  const solids = [], stairs = [], doors = [];
  const extra = { surface, color };
  const sides = {
    south: { cx: x, cz: z - d / 2 + WALL_T / 2, along: 'x', len: w },
    north: { cx: x, cz: z + d / 2 - WALL_T / 2, along: 'x', len: w },
    west: { cx: x - w / 2 + WALL_T / 2, cz: z, along: 'z', len: d - 2 * WALL_T },
    east: { cx: x + w / 2 - WALL_T / 2, cz: z, along: 'z', len: d - 2 * WALL_T },
  };
  const stairX = i => (i % 2 === 0 ? x - w / 2 + WALL_T + STAIR.width / 2 : x + w / 2 - WALL_T - STAIR.width / 2);
  // 1.2 m landing in front of the stairs so a character can line up and walk straight onto them.
  const stairZ0 = z - d / 2 + WALL_T + 1.2;
  const stairLen = STAIR.steps * STAIR.run;

  for (let i = 0; i < floors; i++) {
    const fy = y0 + i * FLOOR_HEIGHT;
    for (const [name, s] of Object.entries(sides)) {
      const isDoor = i === 0 && name === doorSide;
      const opening = isDoor ? { w: DOOR_W, y: 0, top: DOOR_H } : { w: WIN_W, y: WIN_Y0, top: WIN_Y1 };
      solids.push(...wallSegments(s.cx, s.cz, s.along, s.len, fy, FLOOR_HEIGHT, opening, extra));
      if (isDoor) {
        const size = s.along === 'x' ? [DOOR_W, DOOR_H, 0.08] : [0.08, DOOR_H, DOOR_W];
        doors.push({ pos: [s.cx, fy + DOOR_H / 2, s.cz], size, rotY: 0 });
      }
    }
    stairs.push({ from: [stairX(i), fy, stairZ0], dir: '+z', ...STAIR, surface });
    // Stairwell wall on the open side: stairs can only be entered from the front, for players and bots alike.
    const openSide = i % 2 === 0 ? 1 : -1;
    solids.push(block(stairX(i) + openSide * (STAIR.width / 2 + 0.075), fy, stairZ0 + stairLen / 2, 0.15, FLOOR_HEIGHT - SLAB_T, stairLen, extra));
  }
  // Slabs for floors 1..floors (the last one is the roof), each with a hole above the stairs below it.
  const ix0 = x - w / 2 + WALL_T, ix1 = x + w / 2 - WALL_T, iz0 = z - d / 2 + WALL_T, iz1 = z + d / 2 - WALL_T;
  // Hole ends where the top step ends; it is wider than the stairs so a character's head clears the slab edge.
  const holeZ0 = stairZ0 - 0.2, holeZ1 = stairZ0 + stairLen;
  for (let k = 1; k <= floors; k++) {
    const sy = y0 + k * FLOOR_HEIGHT - SLAB_T;
    const hx0 = Math.max(ix0, stairX(k - 1) - STAIR.width / 2 - 0.5), hx1 = Math.min(ix1, stairX(k - 1) + STAIR.width / 2 + 0.5);
    const hx = (hx0 + hx1) / 2, hw = hx1 - hx0;
    if (hx0 > ix0) solids.push(block((ix0 + hx0) / 2, sy, z, hx0 - ix0, SLAB_T, iz1 - iz0, extra));
    if (ix1 > hx1) solids.push(block((hx1 + ix1) / 2, sy, z, ix1 - hx1, SLAB_T, iz1 - iz0, extra));
    if (holeZ0 > iz0) solids.push(block(hx, sy, (iz0 + holeZ0) / 2, hw, SLAB_T, holeZ0 - iz0, extra));
    if (iz1 > holeZ1) solids.push(block(hx, sy, (holeZ1 + iz1) / 2, hw, SLAB_T, iz1 - holeZ1, extra));
  }
  const ry = y0 + floors * FLOOR_HEIGHT;
  for (const s of Object.values(sides)) {
    solids.push(s.along === 'x' ? block(s.cx, ry, s.cz, s.len, 0.9, WALL_T, extra) : block(s.cx, ry, s.cz, WALL_T, 0.9, s.len, extra));
  }
  return { solids, stairs, doors };
}
