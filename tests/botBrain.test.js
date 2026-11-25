import { describe, it, expect } from 'vitest';
import { decideBot, initialBotState } from '../src/botBrain.js';
import { WEAPONS, BOT_REACTION_S, BOT_LOST_SIGHT_S } from '../src/config.js';

const s0 = () => initialBotState();
const per = o => ({ botPos: [100, 0, 100], seesPlayer: false, playerPos: [10, 0, 0], distance: 10, heardShotAt: null, patrol: [[0, 0, 0], [5, 0, 0]], isSniper: false, ...o });

describe('decideBot', () => {
  it('초기 상태는 순찰', () => {
    expect(s0()).toEqual({ mode: 'patrol', patrolIndex: 0, lastKnown: null, lostTimer: 0, reactTimer: 0 });
  });
  it('사거리 안에서 보이면 반응 시간 뒤에 쏜다', () => {
    let r = decideBot(s0(), per({ seesPlayer: true }), WEAPONS.rifle, 0.1);
    expect(r.state.mode).toBe('attack'); expect(r.fire).toBe(false);
    r = decideBot(r.state, per({ seesPlayer: true }), WEAPONS.rifle, BOT_REACTION_S);
    expect(r.fire).toBe(true);
  });
  it('놓치면 마지막 위치로 추적하고 시간이 지나면 순찰로 돌아간다', () => {
    let r = decideBot(s0(), per({ seesPlayer: true }), WEAPONS.rifle, 0.1);
    r = decideBot(r.state, per(), WEAPONS.rifle, 0.1);
    expect(r.state.mode).toBe('chase'); expect(r.moveTo).toEqual([10, 0, 0]);
    r = decideBot(r.state, per(), WEAPONS.rifle, BOT_LOST_SIGHT_S + 0.1);
    expect(r.state.mode).toBe('patrol');
  });
  it('총소리를 들으면 그 위치로 추적한다', () => {
    const r = decideBot(s0(), per({ heardShotAt: [30, 0, 5] }), WEAPONS.rifle, 0.1);
    expect(r.state.mode).toBe('chase'); expect(r.moveTo).toEqual([30, 0, 5]);
  });
  it('저격수는 쫓아가지 않는다', () => {
    const r = decideBot(s0(), per({ heardShotAt: [30, 0, 5], isSniper: true }), WEAPONS.sniper, 0.1);
    expect(r.state.mode).toBe('patrol');
  });
  it('저격수는 사거리 밖의 적이 보여도 제자리에서 기다린다', () => {
    const r = decideBot(s0(), per({ seesPlayer: true, distance: 200, isSniper: true }), WEAPONS.sniper, 0.1);
    expect(r.moveTo).toBeNull(); expect(r.fire).toBe(false);
  });
  it('칼은 1.8 m 안에서만 공격한다', () => {
    let r = decideBot(s0(), per({ seesPlayer: true, distance: 5 }), WEAPONS.knife, 1);
    expect(r.fire).toBe(false); expect(r.state.mode).toBe('chase');
    r = decideBot(r.state, per({ seesPlayer: true, distance: 1.5 }), WEAPONS.knife, 1);
    expect(r.fire).toBe(true);
  });
  it('순찰 지점에 도착하면 다음 지점으로 간다', () => {
    const r = decideBot(s0(), per({ botPos: [0, 0, 0.5] }), WEAPONS.rifle, 0.1);
    expect(r.state.patrolIndex).toBe(1);
    expect(r.moveTo).toEqual([5, 0, 0]);
  });
});
