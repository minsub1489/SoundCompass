import * as THREE from 'three';
import RAPIER from '@dimforge/rapier3d-compat';
import { FIXED_DT, MAX_STEPS_PER_FRAME, MIN_CRATE_DIST_M } from './config.js';
import { stepCount } from './fixedStep.js';
import { createRng } from './rng.js';
import { Round } from './round.js';
import { downloadCsv } from './results.js';
import { initialCrateIndices, nextCrateIndex } from './spawn.js';
import { createPhysics } from './physics.js';
import { buildMap } from './maps/build.js';
import { MAPS } from './maps/index.js';
import { createInput } from './input.js';
import { createPlayer } from './player.js';
import { updateThirdPersonCamera } from './camera.js';
import { createAudio } from './audio/engine.js';
import { buildNav } from './nav.js';
import { createBots } from './bots.js';
import * as ui from './ui.js';

const CRATE_PICKUP_M = 1.2;
const params = new URLSearchParams(location.search);
const map = MAPS[params.get('map')];

if (!map) {
  ui.showStartForm(({ participantId, condition, mapId, audioOutput }) => {
    location.search = new URLSearchParams({ pid: participantId, cond: condition, map: mapId, out: audioOutput }).toString();
  });
} else {
  await run(map);
}

function createCrates(scene, map, rng) {
  const geo = new THREE.BoxGeometry(0.6, 0.6, 0.6), mat = new THREE.MeshLambertMaterial({ color: 0xe0b020 });
  const meshes = new Map();
  const place = i => {
    const m = new THREE.Mesh(geo, mat);
    const c = map.crateCandidates[i];
    m.position.set(c[0], c[1] + 0.3, c[2]);
    scene.add(m);
    meshes.set(i, m);
  };
  initialCrateIndices(map.crateCandidates, map.crateCount, rng).forEach(place);
  // Returns the candidate position picked up this step, or null.
  return function pickUp(playerPos) {
    for (const [i, m] of meshes) {
      const c = map.crateCandidates[i];
      if (Math.hypot(c[0] - playerPos[0], c[2] - playerPos[2]) > CRATE_PICKUP_M || Math.abs(c[1] - playerPos[1]) > 1.5) continue;
      const next = nextCrateIndex(map.crateCandidates, new Set(meshes.keys()), playerPos, rng, MIN_CRATE_DIST_M);
      scene.remove(m);
      meshes.delete(i);
      if (next >= 0) place(next);
      return c;
    }
    return null;
  };
}

async function run(map) {
  await RAPIER.init();
  const audioOutput = params.get('out') === '7.1' ? '7.1' : 'binaural';
  const audio = createAudio({ output: audioOutput });
  const loaded = audio.load();
  const canvas = document.getElementById('game');
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(70, 1, 0.1, 300);
  const physics = createPhysics(RAPIER);
  const onSound = (id, pos) => audio.playAt(id, pos);
  const built = buildMap(map, { scene, physics, onSound });
  physics.step(); // ray queries only see colliders after the first step
  const navReady = buildNav(built.navMeshes);
  const pickUpCrate = createCrates(scene, map, createRng(map.seed));
  const input = createInput(canvas);
  const isBlocked = (from, to) => physics.raycast(from, to, { staticOnly: true }).hit;
  let round = null, player = null, bots = null, starting = false;
  let accumulator = 0, finished = false;

  const resize = () => {
    renderer.setSize(innerWidth, innerHeight, false);
    camera.aspect = innerWidth / innerHeight;
    camera.updateProjectionMatrix();
  };
  addEventListener('resize', resize);
  resize();

  // requestPointerLock can throw synchronously or reject; either way leave the round paused with a retry card.
  const requestLock = async () => {
    try { await input.requestLock(); } catch { ui.showPaused(requestLock); }
  };

  ui.showStartGate(map.name, async () => {
    if (round || starting) return;
    starting = true;
    let nav;
    try {
      await audio.resume();
      await loaded;
      nav = await navReady;
    } catch (e) {
      starting = false;
      ui.showStartError(e.message);
      return;
    }
    round = new Round({
      participantId: params.get('pid') ?? '', condition: params.get('cond') ?? '', mapId: map.id,
      audioOutput, isPractice: map.id === 'practice', startedAt: new Date(),
    });
    // Time only runs while the pointer is locked, so a failed lock leaves the round paused, not running blind.
    round.pause();
    player = createPlayer({
      scene, physics, map, round, camera, surfaceAt: built.surfaceAt, onSound,
      onHit: (collider, damage) => bots.handleHit(collider, damage),
      onShot: at => bots.notifyShot(at),
    });
    bots = createBots({ scene, physics, nav, map, player, round, camera, surfaceAt: built.surfaceAt, rng: createRng(map.seed + 1), onSound });
    if (import.meta.env.DEV) window.__testbed = { round, player, bots, physics, built, step, audio, scene, camera, pickUpCrate, isBlocked };
    audio.startBed(map.ambience);
    audio.startBed(map.bgm);
    ui.showHud();
    requestLock();
  });

  function step(inputState) {
    player.fixedUpdate(FIXED_DT, inputState);
    bots.update(FIXED_DT);
    const characters = [player.position, ...bots.positions];
    built.doors.forEach(d => d.update(FIXED_DT, characters));
    const crate = pickUpCrate(player.position);
    if (crate) { round.addCrate(); onSound('crate_pickup', crate); }
    physics.step();
    round.tick(FIXED_DT);
  }

  input.onLockChange(locked => {
    if (!round || round.finished) return;
    if (locked) { round.resume(); ui.hidePaused(); } else { round.pause(); ui.showPaused(requestLock); }
  });

  const clock = new THREE.Clock();
  renderer.setAnimationLoop(() => {
    const frameDt = clock.getDelta();
    if (round && player && !finished) {
      player.look(input.consumeMouse());
      if (!round.paused) {
        const r = stepCount(accumulator, frameDt, FIXED_DT, MAX_STEPS_PER_FRAME);
        accumulator = r.accumulator;
        for (let i = 0; i < r.steps && !round.finished; i++) step(input);
      }
      player.syncMesh();
      bots.syncMeshes(frameDt);
      updateThirdPersonCamera(camera, player.position, player.yaw, player.pitch, physics);
      audio.setListener(camera);
      audio.update(isBlocked);
      ui.updateHud({ health: player.health, remainingS: round.remainingS, crates: round.stats.crates, ammo: player.ammo, reloading: player.reloading });
      if (round.finished) {
        finished = true;
        document.exitPointerLock();
        downloadCsv(round);
        ui.showResults(round, { onNext: () => { location.search = ''; }, onSave: () => downloadCsv(round) });
      }
    } else if (!round) {
      camera.position.set(map.playerSpawn[0], 12, map.playerSpawn[2] - 10);
      camera.lookAt(0, 0, 0);
    }
    renderer.render(scene, camera);
  });
}
