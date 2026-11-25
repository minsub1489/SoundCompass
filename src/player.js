import * as THREE from 'three';
import { WEAPONS, PLAYER_MAX_HEALTH } from './config.js';
import { createGun } from './weapons.js';
import { isSurpriseHit } from './round.js';
import { forwardOf, rightOf, PITCH_LIMIT } from './camera.js';

const WALK = 4.5, RUN = 7, JUMP = 5, GRAVITY = 18, MOUSE_SENS = 0.0022;
const STEP_WALK_S = 0.5, STEP_RUN_S = 0.33, CHEST = 1.2;

export function createPlayer({ scene, physics, map, round, camera, surfaceAt, onSound = () => {}, onHit = () => {}, onShot = () => {} }) {
  const character = physics.createCharacter(map.playerSpawn);
  const mesh = new THREE.Mesh(new THREE.CapsuleGeometry(0.35, 1.1, 4, 8), new THREE.MeshLambertMaterial({ color: 0x2f6fd6 }));
  scene.add(mesh);
  const gun = createGun(WEAPONS.playerRifle);
  const toCentre = [-map.playerSpawn[0], 0, -map.playerSpawn[2]];
  let yaw = Math.atan2(-toCentre[0], -toCentre[2]), pitch = 0;
  let vy = 0, grounded = true, health = PLAYER_MAX_HEALTH, time = 0, stepTimer = 0, wasReloading = false;
  const frustum = new THREE.Frustum(), projView = new THREE.Matrix4();

  const pos = () => character.position;
  const surfaceUnderFoot = () => {
    const p = pos();
    return surfaceAt(physics.raycast([p[0], p[1] + 0.2, p[2]], [p[0], p[1] - 0.5, p[2]], { staticOnly: true }).collider);
  };

  function look({ dx, dy }) {
    yaw -= dx * MOUSE_SENS;
    pitch = Math.max(-PITCH_LIMIT, Math.min(PITCH_LIMIT, pitch - dy * MOUSE_SENS));
  }

  function shoot() {
    const p = pos();
    onSound(WEAPONS.playerRifle.fireSound, [p[0], p[1] + 1.4, p[2]]);
    onShot(p);
    const from = camera.position.toArray();
    const dir = new THREE.Vector3();
    camera.getWorldDirection(dir);
    const to = [from[0] + dir.x * WEAPONS.playerRifle.rangeM, from[1] + dir.y * WEAPONS.playerRifle.rangeM, from[2] + dir.z * WEAPONS.playerRifle.rangeM];
    const hit = physics.raycast(from, to, { exclude: character.collider });
    if (hit.hit) onHit(hit.collider, WEAPONS.playerRifle.damage);
  }

  function fixedUpdate(dt, input) {
    time += dt;
    const k = input.keys;
    const f = forwardOf(yaw), r = rightOf(yaw);
    let mx = 0, mz = 0;
    if (k.has('KeyW')) { mx += f[0]; mz += f[2]; }
    if (k.has('KeyS')) { mx -= f[0]; mz -= f[2]; }
    if (k.has('KeyD')) { mx += r[0]; mz += r[2]; }
    if (k.has('KeyA')) { mx -= r[0]; mz -= r[2]; }
    const len = Math.hypot(mx, mz);
    const running = k.has('ShiftLeft') || k.has('ShiftRight');
    const speed = len > 0 ? (running ? RUN : WALK) : 0;
    if (len > 0) { mx = (mx / len) * speed; mz = (mz / len) * speed; }

    if (grounded && k.has('Space')) vy = JUMP;
    vy -= GRAVITY * dt;
    const fallSpeed = vy;
    const res = character.move([mx, vy, mz], dt);
    if (res.grounded) {
      if (!grounded && fallSpeed < -6) onSound('land', pos());
      if (vy < 0) vy = 0;
    }
    grounded = res.grounded;

    if (grounded && speed > 0) {
      stepTimer += dt;
      if (stepTimer >= (running ? STEP_RUN_S : STEP_WALK_S)) { stepTimer = 0; onSound(`step_${surfaceUnderFoot()}`, pos()); }
    } else stepTimer = 0;

    if (k.has('KeyR')) gun.startReload(time);
    if (input.firing && gun.tryFire(time)) shoot();
    gun.update(time);
    if (gun.reloading && !wasReloading) onSound(WEAPONS.playerRifle.reloadSound, pos());
    wasReloading = gun.reloading;
  }

  function takeDamage(amount, attackerPos) {
    const target = new THREE.Vector3(attackerPos[0], attackerPos[1] + CHEST, attackerPos[2]);
    projView.multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse);
    frustum.setFromProjectionMatrix(projView);
    const inView = frustum.containsPoint(target);
    const blocked = physics.raycast(camera.position.toArray(), target.toArray(), { staticOnly: true }).hit;
    round.addDamage(amount, isSurpriseHit(inView, blocked));
    health -= amount;
    onSound('hit_taken', pos());
    if (health <= 0) {
      onSound('death', pos());
      round.addDeath();
      character.teleport(map.playerSpawn);
      health = PLAYER_MAX_HEALTH;
      vy = 0;
    }
  }

  function syncMesh() {
    const p = pos();
    mesh.position.set(p[0], p[1] + 0.9, p[2]);
    mesh.rotation.y = yaw;
  }

  return {
    collider: character.collider,
    fixedUpdate, look, takeDamage, syncMesh,
    get position() { return pos(); },
    get yaw() { return yaw; },
    get pitch() { return pitch; },
    get health() { return health; },
    get ammo() { return gun.ammo; },
    get reloading() { return gun.reloading; },
  };
}
