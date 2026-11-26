import { describe, it, expect, beforeAll } from 'vitest';
import { inverseDistanceGain, SPEAKER_ORDER_71 } from '../src/audio/speakers71.js';

// Minimal Web Audio fakes: enough to see which gain values a voice starts with and how it is wired.
const nodes = [];
class Param {
  constructor(v) { this.value = v; }
  setTargetAtTime() {} setValueAtTime() {} linearRampToValueAtTime() {} cancelScheduledValues() {}
}
class FakeNode {
  constructor(ctx, o = {}) {
    this.links = [];
    for (const k of ['gain', 'frequency', 'positionX', 'positionY', 'positionZ']) this[k] = new Param(o[k] ?? (k === 'gain' ? 1 : 0));
    nodes.push(this);
  }
  connect(node, out = 0, input = 0) { this.links.push({ node, input }); return node; }
  disconnect() {}
  start() {}
}
const params = names => Object.fromEntries(names.map(n => [n, new Param(0)]));

beforeAll(() => {
  for (const n of ['GainNode', 'BiquadFilterNode', 'PannerNode', 'AudioBufferSourceNode']) globalThis[n] = class extends FakeNode {};
  globalThis.AudioContext = class {
    constructor() {
      this.state = 'suspended'; this.currentTime = 0; this.destination = { maxChannelCount: 8 };
      this.listener = params(['positionX', 'positionY', 'positionZ', 'forwardX', 'forwardY', 'forwardZ', 'upX', 'upY', 'upZ']);
    }
    createGain() { return new FakeNode(this); }
    createChannelMerger() { return (this.merger = new FakeNode(this)); }
    createChannelSplitter() { return new FakeNode(this); }
    async resume() { this.state = 'running'; }
    async decodeAudioData() { return {}; }
  };
  globalThis.fetch = async () => ({ ok: true, arrayBuffer: async () => new ArrayBuffer(0) });
});

describe('7.1 voice start', () => {
  it('첫 샘플부터 목표 스피커 음량과 거리 감쇠로 시작한다', async () => {
    const { createAudio } = await import('../src/audio/engine.js');
    const audio = createAudio({ output: '7.1' });
    await audio.load();
    await audio.resume();
    // Camera at the origin looking down -z (identity world matrix); the shot is 5 m straight ahead.
    audio.setListener({ position: { x: 0, y: 0, z: 0 }, matrixWorld: { elements: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1] } });
    const before = nodes.length;
    audio.playAt('rifle_shot', [0, 0, -5]);
    const voiceNodes = nodes.slice(before);
    const merger = audio.ctx.merger;
    const speakers = voiceNodes.flatMap(n => n.links.filter(l => l.node === merger).map(l => ({ node: n, ch: l.input })));
    const dist = voiceNodes.find(n => n.links.some(l => speakers.some(s => s.node === l.node)));
    expect(speakers.find(s => s.ch === SPEAKER_ORDER_71.indexOf('FC')).node.gain.value).toBeCloseTo(1);
    expect(dist.gain.value).toBeCloseTo(inverseDistanceGain(5));
  });
});
