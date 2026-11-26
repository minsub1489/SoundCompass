export const PITCH_LIMIT = (70 * Math.PI) / 180;
const RIGHT = 0.5, UP = 1.7, BACK = 3.0, WALL_PAD = 0.2;

// yaw 0 looks toward -z (three.js default); positive yaw turns left.
export const forwardOf = (yaw, pitch = 0) => [-Math.sin(yaw) * Math.cos(pitch), Math.sin(pitch), -Math.cos(yaw) * Math.cos(pitch)];
export const rightOf = yaw => [Math.cos(yaw), 0, -Math.sin(yaw)];

const add = (a, b, s = 1) => [a[0] + b[0] * s, a[1] + b[1] * s, a[2] + b[2] * s];

// Pull a point back toward `from` when a wall is in the way.
function clearPoint(physics, from, to) {
  const hit = physics.raycast(from, to, { staticOnly: true });
  if (!hit.hit) return to;
  const len = Math.hypot(to[0] - from[0], to[1] - from[1], to[2] - from[2]);
  const dir = [(to[0] - from[0]) / len, (to[1] - from[1]) / len, (to[2] - from[2]) / len];
  return add(hit.point, dir, -WALL_PAD);
}

export function updateThirdPersonCamera(camera, target, yaw, pitch, physics) {
  const head = [target[0], target[1] + UP, target[2]];
  const pivot = clearPoint(physics, head, add(head, rightOf(yaw), RIGHT));
  const fwd = forwardOf(yaw, pitch);
  const eye = clearPoint(physics, pivot, add(pivot, fwd, -BACK));
  camera.position.set(...eye);
  const look = add(pivot, fwd, 20);
  camera.lookAt(look[0], look[1], look[2]);
  camera.updateMatrixWorld();
}
