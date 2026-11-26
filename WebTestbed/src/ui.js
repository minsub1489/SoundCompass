import { MAPS } from './maps/index.js';

const root = document.getElementById('ui');
const style = document.createElement('style');
style.textContent = `
  .panel { position: fixed; inset: 0; display: grid; place-items: center; background: rgba(10,12,16,.82); }
  .card { background: #1d2129; padding: 28px 32px; border-radius: 10px; min-width: 340px; display: grid; gap: 12px; }
  .card h1 { margin: 0 0 4px; font-size: 20px; }
  .card label { display: grid; gap: 4px; font-size: 14px; color: #bbb; }
  .card input, .card select, .card button { font: inherit; padding: 8px 10px; border-radius: 6px; border: 1px solid #444; background: #111; color: #eee; }
  .card button { background: #2f6fd6; border: 0; cursor: pointer; }
  .error { color: #ff7a7a; min-height: 1.2em; font-size: 14px; }
  .hud { position: fixed; font: 600 18px system-ui; text-shadow: 0 1px 2px #000; }
  .crosshair { position: fixed; left: 50%; top: 50%; width: 14px; height: 14px; margin: -7px 0 0 -7px; }
  .crosshair::before, .crosshair::after { content: ''; position: absolute; background: #fff; }
  .crosshair::before { left: 6px; top: 0; width: 2px; height: 14px; }
  .crosshair::after { top: 6px; left: 0; height: 2px; width: 14px; }
  .crosshair.hit::before, .crosshair.hit::after { background: #ff3b3b; }
  table { border-collapse: collapse; } td { padding: 4px 12px; } td:last-child { text-align: right; font-weight: 600; }
`;
document.head.append(style);

const el = html => { const t = document.createElement('template'); t.innerHTML = html.trim(); return t.content.firstElementChild; };
const clear = () => root.replaceChildren();

const OUTPUT_LABEL = { binaural: '바이노럴 스테레오', '7.1': '7.1 채널' };

export function showStartForm(onSubmit) {
  clear();
  const form = el(`<form class="panel"><div class="card">
    <h1>SoundCompass 테스트</h1>
    <label>참가자 ID<input name="pid" autocomplete="off"></label>
    <label>조건 이름<input name="cond" autocomplete="off"></label>
    <label>맵<select name="map">${Object.values(MAPS).map(m => `<option value="${m.id}">${m.name}</option>`).join('')}</select></label>
    <label>출력 방식<select name="out">${Object.entries(OUTPUT_LABEL).map(([v, t]) => `<option value="${v}">${t}</option>`).join('')}</select></label>
    <div class="error"></div>
    <button type="submit">시작</button>
  </div></form>`);
  form.addEventListener('submit', e => {
    e.preventDefault();
    const data = new FormData(form);
    const participantId = String(data.get('pid')).trim();
    if (!participantId) { form.querySelector('.error').textContent = '참가자 ID를 입력하세요.'; return; }
    onSubmit({ participantId, condition: String(data.get('cond')).trim(), mapId: String(data.get('map')), audioOutput: String(data.get('out')) });
  });
  root.append(form);
}

let gate = null;
export function showStartGate(text, onClick) {
  clear();
  gate = el(`<div class="panel"><div class="card"><h1>${text}</h1><div>화면을 클릭하면 시작합니다.</div><div class="error"></div></div></div>`);
  gate.addEventListener('click', onClick);
  root.append(gate);
}

export function showStartError(message) {
  if (gate) gate.querySelector('.error').textContent = message;
}

let hud = null;
export function showHud() {
  clear();
  hud = {
    time: el('<div class="hud" style="top:16px;left:50%;transform:translateX(-50%)"></div>'),
    crates: el('<div class="hud" style="top:16px;right:20px"></div>'),
    health: el('<div class="hud" style="bottom:20px;left:20px"></div>'),
    ammo: el('<div class="hud" style="bottom:20px;right:20px"></div>'),
  };
  hud.crosshair = el('<div class="crosshair"></div>');
  root.append(...Object.values(hud));
}

// Own-shot feedback only: the crosshair turns red briefly when the player's shot hits a bot.
let hitTimer = 0;
export function flashCrosshairHit() {
  if (!hud) return;
  hud.crosshair.classList.add('hit');
  clearTimeout(hitTimer);
  hitTimer = setTimeout(() => hud?.crosshair.classList.remove('hit'), 120);
}

export function updateHud({ health, remainingS, crates, ammo, reloading }) {
  if (!hud) return;
  const s = Math.ceil(remainingS);
  hud.time.textContent = `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
  hud.crates.textContent = `상자 ${crates}`;
  hud.health.textContent = `체력 ${Math.max(0, Math.round(health))}`;
  hud.ammo.textContent = reloading ? '재장전 중' : `탄약 ${ammo}`;
}

let paused = null;
export function showPaused(onClick) {
  if (paused) return;
  paused = el('<div class="panel"><div class="card"><h1>일시정지</h1><div>클릭하면 이어서 진행합니다.</div></div></div>');
  paused.addEventListener('click', onClick);
  root.append(paused);
}
export function hidePaused() { paused?.remove(); paused = null; }

export function showResults(round, { onNext, onSave }) {
  clear();
  hud = null;
  const s = round.stats;
  const rows = [['모은 상자', s.crates], ['처치', s.kills], ['사망', s.deaths], ['받은 피해', s.damageTaken], ['기습 피해 횟수', s.surpriseHits]];
  const card = el(`<div class="panel"><div class="card">
    <h1>라운드 종료${round.isPractice ? ' (연습)' : ''}</h1>
    <table>${rows.map(([k, v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join('')}</table>
    <div>${round.isPractice ? '연습 라운드 결과는 저장하지 않습니다.' : '결과 CSV를 다운로드했습니다. 다운로드 폴더에 파일이 없으면 다시 저장하세요.'}</div>
    ${round.isPractice ? '' : '<button data-save>CSV 다시 저장</button>'}
    <button data-next>다음</button>
  </div></div>`);
  card.querySelector('[data-next]').addEventListener('click', onNext);
  card.querySelector('[data-save]')?.addEventListener('click', onSave);
  root.append(card);
}
