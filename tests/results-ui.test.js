// @vitest-environment happy-dom
import { describe, it, expect, vi, beforeAll } from 'vitest';
import { Round } from '../src/round.js';

const mk = isPractice => new Round({ participantId: 'P01', condition: 'A', mapId: isPractice ? 'practice' : 'urban', audioOutput: 'binaural', isPractice, startedAt: new Date() });
const button = text => [...document.querySelectorAll('#ui button')].find(b => b.textContent.includes(text));

// ui.js grabs #ui once at import, so the element must exist before the first import and stay in place.
beforeAll(() => { document.body.innerHTML = '<div id="ui"></div>'; });

describe('showResults', () => {
  it('본 라운드는 CSV를 다시 저장할 수 있다', async () => {
    const { showResults } = await import('../src/ui.js');
    const onSave = vi.fn();
    showResults(mk(false), { onNext: () => {}, onSave });
    button('CSV 다시 저장').click();
    expect(onSave).toHaveBeenCalledTimes(1);
  });
  it('연습 라운드에는 저장 버튼이 없다', async () => {
    const { showResults } = await import('../src/ui.js');
    showResults(mk(true), { onNext: () => {}, onSave: () => {} });
    expect(button('CSV 다시 저장')).toBeUndefined();
  });
});
