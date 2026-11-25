import { SOUNDS } from './sounds.js';
import { OCCLUSION } from '../config.js';
import { occlusionTarget, applyOcclusion } from './occlusion.js';
import { speakerGains71, SPEAKER_ORDER_71, canUse71, inverseDistanceGain, PANNER } from './speakers71.js';

const NO_71 = '출력 장치가 7.1(8채널)이 아닙니다. Windows 소리 설정에서 출력 장치를 7.1로 바꾸세요.';
// Occlusion is tested toward a point above the emitter so foot-level sounds do not graze the floor they stand on.
const OCC_LIFT_M = 0.5;
const SMOOTH_S = 0.02;

export function createAudio({ output }) {
  const is71 = output === '7.1';
  const ctx = new AudioContext({ latencyHint: 'interactive' });
  const master = ctx.createGain();
  master.connect(ctx.destination);
  const buffers = new Map();
  const voices = new Set();
  let merger = null;
  let isBlocked = () => false;
  const listener = { pos: [0, 0, 0], fwd: [0, 0, -1], right: [1, 0, 0] };

  async function load() {
    await Promise.all(Object.entries(SOUNDS).flatMap(([id, s]) => s.files.map(async (file, i) => {
      const res = await fetch(file);
      if (!res.ok) throw new Error(`missing sound file ${file}`);
      const list = buffers.get(id) ?? buffers.set(id, []).get(id);
      list[i] = await ctx.decodeAudioData(await res.arrayBuffer());
    })));
  }

  // Call from a user gesture. Refuses 7.1 on a device without 8 channels instead of silently down-mixing.
  async function resume() {
    if (is71) {
      if (!canUse71(ctx.destination.maxChannelCount)) throw new Error(NO_71);
      Object.assign(ctx.destination, { channelCount: 8, channelCountMode: 'explicit', channelInterpretation: 'discrete' });
      Object.assign(master, { channelCount: 8, channelCountMode: 'explicit', channelInterpretation: 'discrete' });
      merger = ctx.createChannelMerger(8);
      merger.connect(master);
    }
    await ctx.resume();
  }

  function setListener(camera) {
    const p = camera.position, e = camera.matrixWorld.elements;
    const fwd = [-e[8], -e[9], -e[10]], up = [e[4], e[5], e[6]];
    listener.pos = [p.x, p.y, p.z];
    const hl = Math.hypot(fwd[0], fwd[2]) || 1;
    listener.fwd = [fwd[0] / hl, 0, fwd[2] / hl];
    listener.right = [-listener.fwd[2], 0, listener.fwd[0]];
    if (is71) return;
    const l = ctx.listener;
    l.positionX.value = p.x; l.positionY.value = p.y; l.positionZ.value = p.z;
    l.forwardX.value = fwd[0]; l.forwardY.value = fwd[1]; l.forwardZ.value = fwd[2];
    l.upX.value = up[0]; l.upY.value = up[1]; l.upZ.value = up[2];
  }

  const occPoint = p => [p[0], p[1] + OCC_LIFT_M, p[2]];

  // immediate: first placement sets values outright so a transient (gunshot) is not faded in from 0.
  function placeVoice(v, now, immediate = false) {
    const p = typeof v.pos === 'function' ? v.pos() : v.pos;
    if (!is71) {
      v.panner.positionX.value = p[0]; v.panner.positionY.value = p[1]; v.panner.positionZ.value = p[2];
      return p;
    }
    const rel = [p[0] - listener.pos[0], p[1] - listener.pos[1], p[2] - listener.pos[2]];
    const az = (Math.atan2(rel[0] * listener.right[0] + rel[2] * listener.right[2], rel[0] * listener.fwd[0] + rel[2] * listener.fwd[2]) * 180) / Math.PI;
    const gains = speakerGains71(az);
    const set = (param, value) => (immediate ? (param.value = value) : param.setTargetAtTime(value, now, SMOOTH_S));
    v.speakers.forEach(([ch, node]) => set(node.gain, gains[ch]));
    set(v.dist.gain, inverseDistanceGain(Math.hypot(...rel)));
    return p;
  }

  function playAt(id, pos, opts = {}) {
    const list = buffers.get(id);
    if (!list?.length || ctx.state !== 'running') return;
    const now = ctx.currentTime;
    const src = new AudioBufferSourceNode(ctx, { buffer: list[Math.floor(Math.random() * list.length)] });
    const gain = new GainNode(ctx, { gain: SOUNDS[id].gain * (opts.gain ?? 1) });
    const p0 = typeof pos === 'function' ? pos() : pos;
    const blocked = isBlocked(listener.pos, occPoint(p0));
    const start = occlusionTarget(blocked);
    const occGain = new GainNode(ctx, { gain: start.gain });
    const filter = new BiquadFilterNode(ctx, { type: 'lowpass', frequency: start.cutoffHz });
    src.connect(gain).connect(occGain).connect(filter);
    const v = { src, occGain, filter, pos, blocked, nextCheck: now + OCCLUSION.updateSeconds };
    if (is71) {
      v.dist = new GainNode(ctx, { gain: 0 });
      filter.connect(v.dist);
      v.speakers = SPEAKER_ORDER_71.map((name, ch) => [ch, name]).filter(([, name]) => name !== 'LFE').map(([ch]) => {
        const node = new GainNode(ctx, { gain: 0 });
        v.dist.connect(node).connect(merger, 0, ch);
        return [ch, node];
      });
    } else {
      v.panner = new PannerNode(ctx, { panningModel: 'HRTF', distanceModel: 'inverse', ...PANNER });
      filter.connect(v.panner).connect(master);
    }
    placeVoice(v, now, true);
    voices.add(v);
    src.onended = () => { voices.delete(v); src.disconnect(); };
    src.start();
  }

  function startBed(id) {
    const list = buffers.get(id);
    if (!list?.length) return;
    const src = new AudioBufferSourceNode(ctx, { buffer: list[0], loop: true });
    const gain = new GainNode(ctx, { gain: SOUNDS[id].gain });
    src.connect(gain);
    if (is71) {
      const split = ctx.createChannelSplitter(2);
      gain.connect(split);
      split.connect(merger, 0, 0);
      split.connect(merger, 1, 1);
    } else gain.connect(master);
    src.start();
  }

  // Once per frame after setListener: moves voices and re-tests occlusion every OCCLUSION.updateSeconds.
  function update(blockedFn) {
    isBlocked = blockedFn;
    const now = ctx.currentTime;
    for (const v of voices) {
      const p = placeVoice(v, now);
      if (now < v.nextCheck) continue;
      v.nextCheck = now + OCCLUSION.updateSeconds;
      const blocked = isBlocked(listener.pos, occPoint(p));
      if (blocked !== v.blocked) { v.blocked = blocked; applyOcclusion(v, occlusionTarget(blocked), now); }
    }
  }

  return { ctx, load, resume, setListener, playAt, startBed, update, get voiceCount() { return voices.size; } };
}
