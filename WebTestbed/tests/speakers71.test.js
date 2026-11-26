import { describe, it, expect } from 'vitest';
import { speakerGains71, SPEAKER_ORDER_71, canUse71, inverseDistanceGain } from '../src/audio/speakers71.js';

const g = (az, name) => speakerGains71(az)[SPEAKER_ORDER_71.indexOf(name)];
const power = az => speakerGains71(az).reduce((s, v) => s + v * v, 0);

describe('speakerGains71', () => {
  it('스피커 방향에서는 그 스피커만 울린다', () => {
    expect(g(0, 'FC')).toBeCloseTo(1); expect(g(30, 'FR')).toBeCloseTo(1);
    expect(g(-90, 'SL')).toBeCloseTo(1); expect(g(150, 'BR')).toBeCloseTo(1);
  });
  it('두 스피커 사이 가운데는 같은 음량', () => {
    expect(g(60, 'FR')).toBeCloseTo(Math.SQRT1_2); expect(g(60, 'SR')).toBeCloseTo(Math.SQRT1_2);
    expect(g(180, 'BL')).toBeCloseTo(Math.SQRT1_2); expect(g(-180, 'BR')).toBeCloseTo(Math.SQRT1_2);
  });
  it('모든 방향에서 출력 합이 일정하고 LFE는 0', () => {
    for (let az = -180; az < 180; az += 7) { expect(power(az)).toBeCloseTo(1); expect(g(az, 'LFE')).toBe(0); }
  });
  it('360°를 넘는 각도도 정규화한다', () => {
    expect(speakerGains71(390)).toEqual(speakerGains71(30));
  });
});

describe('canUse71', () => {
  it('8채널 이상만 허용한다', () => {
    expect(canUse71(2)).toBe(false); expect(canUse71(6)).toBe(false); expect(canUse71(8)).toBe(true);
  });
});

describe('inverseDistanceGain', () => {
  it('PannerNode inverse 모델과 같은 값을 낸다', () => {
    expect(inverseDistanceGain(1)).toBe(1);
    expect(inverseDistanceGain(2)).toBe(1);
    expect(inverseDistanceGain(4)).toBeCloseTo(0.5);
    expect(inverseDistanceGain(500)).toBeCloseTo(2 / 120);
  });
});
