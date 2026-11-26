import { describe, it, expect, vi } from 'vitest';
import { occlusionTarget, applyOcclusion } from '../src/audio/occlusion.js';

const param = v => ({ value: v, cancelScheduledValues: vi.fn(), setValueAtTime: vi.fn(), linearRampToValueAtTime: vi.fn() });

describe('occlusion', () => {
  it('막히면 35%, 1500 Hz', () => {
    expect(occlusionTarget(true)).toEqual({ gain: 0.35, cutoffHz: 1500 });
    expect(occlusionTarget(false)).toEqual({ gain: 1, cutoffHz: 20000 });
  });
  it('0.15초 동안 선형 전환한다', () => {
    const voice = { occGain: { gain: param(1) }, filter: { frequency: param(20000) } };
    applyOcclusion(voice, occlusionTarget(true), 10);
    expect(voice.occGain.gain.setValueAtTime).toHaveBeenCalledWith(1, 10);
    expect(voice.occGain.gain.linearRampToValueAtTime).toHaveBeenCalledWith(0.35, 10.15);
    expect(voice.filter.frequency.linearRampToValueAtTime).toHaveBeenCalledWith(1500, 10.15);
  });
});
