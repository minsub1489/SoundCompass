import * as THREE from 'three';
import {
  WEAPONS, BOT_MAX_HEALTH, BOT_RESPAWN_SECONDS, MIN_RESPAWN_DIST_M, SHOT_HEARING_M, BOT_FOV_DEG,
} from './config.js';
import { createGun } from './weapons.js';
import { decideBot, initialBotState } from './botBrain.js';
import { chooseRespawnIndex } from './spawn.js';
import { forwardOf } from './camera.js';

const COLORS = { rifle: 0xd03030, shotgun: 0xe08020, sniper: 0x8040c0, knife: 0x202020 };
const SPEED = { patrol: 3.5, chase: 5, attack: 3.5 };
const KNIFE_CHASE_SPEED = 6.5;
const GRAVITY = 18, EYE = 1.6, CHEST = 1.2, MUZZLE = 1.4, SIGHT_M = 100;
const REPATH_S = 0.5, WAYPOINT_M = 0.4, AFTER_FIRE_DELAY_S = 0.35;
const STEP_WALK_S = 0.5, STEP_RUN_S = 0.33;

const add = (p, dy) => [p[0], p[1] + dy, p[2]];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const len = v => Math.hypot(v[0], v[1], v[2]);

export function createBots({ scene, physics, nav, map, player, round, camera, surfaceAt, rng, onSound }) {
  const frustum = new THREE.Frustum(), projView = new THREE.Matrix4(), tmp = new THREE.Vector3();
  const byCollider = new Map();

  // Same rule as the surprise-hit check: on screen and not hidden behind static geometry.
  function visibleToPlayer(p) {
    projView.multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse);
    frustum.setFromProjectionMatrix(projView);
    const chest = add(p, CHEST);
    return frustum.containsPoint(tmp.set(...chest)) && !physics.raycast(camera.position.toArray(), chest, { staticOnly: true }).hit;
  }

  const bots = map.bots.map(spec => {
    const def = WEAPONS[spec.weapon];
    const character = physics.createCharacter(spec.spawn);
    const material = new THREE.MeshLambertMaterial({ color: COLORS[spec.weapon] });
    const mesh = new THREE.Mesh(new THREE.CapsuleGeometry(0.35, 1.1, 4, 8), material);
    scene.add(mesh);
    const first = spec.patrol[1] ?? spec.patrol[0];
    const bot = {
      spec, def, character, mesh, material, gun: createGun(def),
      state: initialBotState(), health: BOT_MAX_HEALTH, alive: true, respawnTimer: 0,
      yaw: Math.atan2(-(first[0] - spec.spawn[0]), -(first[2] - spec.spawn[2])) || 0,
      vy: 0, grounded: true, time: 0, stepTimer: 0, wasReloading: false, flash: 0,
      path: [], pathTarget: null, repath: 0, heardShotAt: null, pending: [],
    };
    byCollider.set(character.collider.handle, bot);
    return bot;
  });

  const pos = bot => bot.character.position;

  function perceive(bot) {
    const p = pos(bot), pp = player.position;
    const toPlayer = sub(add(pp, CHEST), add(p, EYE));
    const distance = len(toPlayer);
    const fwd = forwardOf(bot.yaw);
    const h = Math.hypot(toPlayer[0], toPlayer[2]) || 1;
    const cos = (fwd[0] * toPlayer[0] + fwd[2] * toPlayer[2]) / h;
    const inFov = distance < 2 || cos >= Math.cos(((BOT_FOV_DEG / 2) * Math.PI) / 180);
    const sees = distance <= SIGHT_M && inFov && !physics.raycast(add(p, EYE), add(pp, CHEST), { staticOnly: true }).hit;
    const out = { botPos: p, seesPlayer: sees, playerPos: pp, distance, heardShotAt: bot.heardShotAt, patrol: bot.spec.patrol, isSniper: bot.def.id === 'sniper' };
    bot.heardShotAt = null;
    return out;
  }

  function fire(bot, distance) {
    const p = pos(bot), pp = player.position;
    if (bot.def.magSize === 0) {
      onSound(bot.def.fireSound, add(p, CHEST));
      if (distance <= bot.def.rangeM) { player.takeDamage(bot.def.damage, p); onSound('knife_hit', add(pp, CHEST)); }
      return;
    }
    const muzzle = add(p, MUZZLE);
    onSound(bot.def.fireSound, muzzle);
    if (bot.def.afterFireSound) bot.pending.push({ id: bot.def.afterFireSound, at: bot.time + AFTER_FIRE_DELAY_S });
    const aim = sub(add(pp, CHEST), muzzle);
    const spread = (bot.def.spreadDeg * Math.PI) / 180;
    const yaw = Math.atan2(aim[0], aim[2]) + (rng() * 2 - 1) * spread;
    const pitch = Math.atan2(aim[1], Math.hypot(aim[0], aim[2])) + (rng() * 2 - 1) * spread;
    const dir = [Math.sin(yaw) * Math.cos(pitch), Math.sin(pitch), Math.cos(yaw) * Math.cos(pitch)];
    const end = [muzzle[0] + dir[0] * bot.def.rangeM, muzzle[1] + dir[1] * bot.def.rangeM, muzzle[2] + dir[2] * bot.def.rangeM];
    const hit = physics.raycast(muzzle, end, { exclude: bot.character.collider });
    if (hit.hit && hit.collider.handle === player.collider.handle) {
      // Shotgun damage falls off linearly to 30% at the end of its range.
      const falloff = bot.def.id === 'shotgun' ? 1 - 0.7 * Math.min(1, distance / bot.def.rangeM) : 1;
      player.takeDamage(Math.round(bot.def.damage * falloff), p);
    }
  }

  function steer(bot, moveTo, dt) {
    const p = pos(bot);
    if (!moveTo) return [0, 0];
    bot.repath -= dt;
    if (bot.repath <= 0 || !bot.pathTarget || len(sub(moveTo, bot.pathTarget)) > 1) {
      bot.path = nav.path(p, moveTo);
      bot.pathTarget = moveTo;
      bot.repath = REPATH_S;
    }
    while (bot.path.length && Math.hypot(bot.path[0][0] - p[0], bot.path[0][2] - p[2]) < WAYPOINT_M) bot.path.shift();
    const next = bot.path[0];
    if (!next) return [0, 0];
    const d = [next[0] - p[0], next[2] - p[2]], h = Math.hypot(d[0], d[1]) || 1;
    return [d[0] / h, d[1] / h];
  }

  function respawn(bot) {
    const others = bots.filter(b => b !== bot && b.alive).map(pos);
    const i = chooseRespawnIndex(map.respawnPoints, player.position, visibleToPlayer, MIN_RESPAWN_DIST_M, others);
    bot.character.teleport(map.respawnPoints[i]);
    bot.character.setEnabled(true);
    Object.assign(bot, { alive: true, health: BOT_MAX_HEALTH, state: initialBotState(), vy: 0, path: [], pathTarget: null, gun: createGun(bot.def), pending: [] });
    bot.mesh.visible = true;
  }

  function updateBot(bot, dt) {
    bot.time += dt;
    if (!bot.alive) {
      if ((bot.respawnTimer -= dt) <= 0) respawn(bot);
      return;
    }
    const per = perceive(bot);
    const d = decideBot(bot.state, per, bot.def, dt);
    bot.state = d.state;
    const [dx, dz] = steer(bot, d.moveTo, dt);
    const speed = bot.def.id === 'knife' && bot.state.mode !== 'patrol' ? KNIFE_CHASE_SPEED : SPEED[bot.state.mode];
    const moving = dx !== 0 || dz !== 0;
    if (bot.state.mode === 'attack' || per.seesPlayer) {
      const to = sub(per.playerPos, per.botPos);
      bot.yaw = Math.atan2(-to[0], -to[2]);
    } else if (moving) bot.yaw = Math.atan2(-dx, -dz);

    bot.vy = bot.grounded ? -1 : bot.vy - GRAVITY * dt;
    const res = bot.character.move([dx * speed, bot.vy, dz * speed], dt);
    bot.grounded = res.grounded;

    if (bot.grounded && moving) {
      bot.stepTimer += dt;
      if (bot.stepTimer >= (bot.state.mode === 'patrol' ? STEP_WALK_S : STEP_RUN_S)) {
        bot.stepTimer = 0;
        const p = pos(bot);
        const surface = surfaceAt(physics.raycast([p[0], p[1] + 0.2, p[2]], [p[0], p[1] - 0.5, p[2]], { staticOnly: true }).collider);
        onSound(`step_${surface}`, () => pos(bot));
      }
    } else bot.stepTimer = 0;

    if (d.fire && bot.gun.tryFire(bot.time)) fire(bot, per.distance);
    bot.gun.update(bot.time);
    if (bot.gun.reloading && !bot.wasReloading && bot.def.reloadSound) onSound(bot.def.reloadSound, add(pos(bot), MUZZLE));
    bot.wasReloading = bot.gun.reloading;
    bot.pending = bot.pending.filter(s => {
      if (bot.time < s.at) return true;
      onSound(s.id, add(pos(bot), MUZZLE));
      return false;
    });
  }

  return {
    update(dt) { bots.forEach(b => updateBot(b, dt)); },
    syncMeshes(dt) {
      for (const b of bots) {
        const p = pos(b);
        b.mesh.position.set(p[0], p[1] + 0.9, p[2]);
        b.mesh.rotation.y = b.yaw;
        b.flash = Math.max(0, b.flash - dt);
        b.material.emissive.setHex(b.flash > 0 ? 0xffffff : 0x000000);
      }
    },
    // Player's shot: bots within hearing range investigate it.
    notifyShot(at) {
      for (const b of bots) if (b.alive && len(sub(pos(b), at)) <= SHOT_HEARING_M) b.heardShotAt = at;
    },
    handleHit(collider, damage) {
      const b = byCollider.get(collider.handle);
      if (!b || !b.alive) return false;
      b.health -= damage;
      b.flash = 0.08;
      if (b.health <= 0) {
        b.alive = false;
        b.respawnTimer = BOT_RESPAWN_SECONDS;
        b.character.setEnabled(false);
        b.mesh.visible = false;
        round.addKill();
        onSound('death', add(pos(b), CHEST));
      }
      return true;
    },
    get positions() { return bots.filter(b => b.alive).map(pos); },
    bots,
  };
}
