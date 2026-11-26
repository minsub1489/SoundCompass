import { FIXED_DT } from './config.js';

export const GROUP = { STATIC: 1, DOOR: 2, CHARACTER: 4 };
// Rapier interaction groups: upper 16 bits = membership, lower 16 bits = what it collides with.
const groups = (member, filter = 0xffff) => (member << 16) | filter;
const STATIC_QUERY = groups(0xffff, GROUP.STATIC | GROUP.DOOR);

const quatY = r => ({ x: 0, y: Math.sin(r / 2), z: 0, w: Math.cos(r / 2) });

export function createPhysics(RAPIER) {
  const world = new RAPIER.World({ x: 0, y: -9.81, z: 0 });
  world.timestep = FIXED_DT;

  function addStaticBox(box, group = GROUP.STATIC) {
    const [w, h, d] = box.size;
    const desc = RAPIER.ColliderDesc.cuboid(w / 2, h / 2, d / 2)
      .setTranslation(box.pos[0], box.pos[1], box.pos[2])
      .setRotation(quatY(box.rotY ?? 0))
      .setCollisionGroups(groups(group));
    return world.createCollider(desc);
  }

  function raycast(from, to, { exclude, staticOnly = false } = {}) {
    const dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
    const len = Math.hypot(dx, dy, dz);
    if (len < 1e-6) return { hit: false };
    const dir = { x: dx / len, y: dy / len, z: dz / len };
    const ray = new RAPIER.Ray({ x: from[0], y: from[1], z: from[2] }, dir);
    const hit = world.castRay(ray, len, true, undefined, staticOnly ? STATIC_QUERY : undefined, exclude);
    if (!hit) return { hit: false };
    const t = hit.timeOfImpact;
    return { hit: true, point: [from[0] + dir.x * t, from[1] + dir.y * t, from[2] + dir.z * t], collider: hit.collider };
  }

  function createCharacter(pos, radius = 0.35, halfHeight = 0.55) {
    const footOffset = halfHeight + radius;
    const body = world.createRigidBody(RAPIER.RigidBodyDesc.kinematicPositionBased().setTranslation(pos[0], pos[1] + footOffset, pos[2]));
    const collider = world.createCollider(RAPIER.ColliderDesc.capsule(halfHeight, radius).setCollisionGroups(groups(GROUP.CHARACTER)), body);
    const controller = world.createCharacterController(0.02);
    // Matches the navmesh walkableClimb (0.6 m) so bots can follow paths that cut onto stairs from the side.
    controller.enableAutostep(0.55, 0.2, false);
    controller.enableSnapToGround(0.3);
    controller.setMaxSlopeClimbAngle((45 * Math.PI) / 180);
    controller.setApplyImpulsesToDynamicBodies(false);
    return {
      collider,
      height: 2 * footOffset,
      get position() { const t = body.translation(); return [t.x, t.y - footOffset, t.z]; },
      // velocity in m/s; the caller integrates gravity into velocity[1].
      move(velocity, dt) {
        controller.computeColliderMovement(collider, { x: velocity[0] * dt, y: velocity[1] * dt, z: velocity[2] * dt });
        const m = controller.computedMovement(), t = body.translation();
        body.setNextKinematicTranslation({ x: t.x + m.x, y: t.y + m.y, z: t.z + m.z });
        return { grounded: controller.computedGrounded() };
      },
      teleport(p) { body.setTranslation({ x: p[0], y: p[1] + footOffset, z: p[2] }, true); body.setNextKinematicTranslation({ x: p[0], y: p[1] + footOffset, z: p[2] }); },
      setEnabled(on) { collider.setEnabled(on); },
    };
  }

  return { world, RAPIER, step: () => world.step(), addStaticBox, raycast, createCharacter };
}
