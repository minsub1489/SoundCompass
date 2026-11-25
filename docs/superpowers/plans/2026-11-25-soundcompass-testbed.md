# SoundCompass 테스트 게임 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SoundCompass가 실제 게임 플레이에 도움이 되는지 설문으로 평가하기 위한 3인칭 교전 웹 게임(연습장 1개 + 본 맵 3개, 라운드당 3분, 바이노럴 또는 7.1 출력)을 만듭니다.

**Architecture:** Vite로 빌드하는 정적 웹 앱입니다. three.js가 렌더링을, Rapier가 충돌과 캐릭터 이동을, recast-navigation이 여러 층에 걸친 봇 길찾기를, Web Audio(HRTF)가 소리를 맡습니다. 라운드마다 페이지를 새로 불러와서 씬과 물리 상태를 정리하는 코드를 따로 두지 않습니다. 판정, 타이머, CSV, 봇 판단 같은 로직은 DOM과 WebGL 없이 테스트할 수 있는 순수 모듈로 분리합니다.

**Tech Stack:** Node 22, Vite 8.3.8, three 0.187.0, @dimforge/rapier3d-compat 0.21.0, @recast-navigation/core·three·generators 0.44.0, Vitest 5.1.1, ffmpeg 7.1, ComfyUI Stable Audio 3 Medium.

**Spec:** `docs/superpowers/specs/2026-11-25-soundcompass-testbed-design.md`

## Global Constraints

- 프로젝트 루트: `D:\Epic Games\Unreal Project\SoundCompass\soundcompass-testbed` (아래 경로는 모두 이 기준)
- 대상 브라우저: 최신 Chrome. 실험실 PC에서 인터넷 없이 실행되어야 하므로 CDN을 쓰지 않습니다.
- 빌드 결과(`dist/`)는 정적 폴더이며 `vite.config.js`의 `base: './'`로 상대 경로를 씁니다.
- 출력: 시작 화면에서 `binaural` 또는 `7.1`을 고릅니다. `binaural`은 `PannerNode`(`panningModel = "HRTF"`), `7.1`은 직접 계산한 스피커별 음량(전방 ±30°, 중앙 0°, 측면 ±90°, 후방 ±150°, LFE 미사용)입니다. 청취 위치는 카메라입니다. 7.1을 골랐는데 장치가 8채널을 지원하지 않으면 시작하지 않습니다.
- 가림: 음량 35%, 저역 통과 1,500 Hz, 0.15초 동안 전환합니다.
- 라운드 3분(180초), 연습 라운드 90초. 연습 라운드는 CSV를 저장하지 않습니다.
- 봇 부활 10초, 부활 위치는 플레이어 화면 밖.
- 게임에는 실험용 음소거, 피격 방향 표시, 미니맵, 레이더, 적 위치 마커, 발소리 시각 효과를 넣지 않습니다. HUD는 조준점, 체력, 남은 시간, 모은 상자 수, 탄약만 표시합니다.
- 모든 음량은 고정값입니다. 조건이나 라운드에 따라 바꾸지 않습니다.
- 커밋 메시지 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`를 붙입니다.

## Review Focus

1. 라운드 중 Esc로 포인터 잠금이 풀린 경우: 남은 시간이 줄지 않고, 다시 클릭하면 이어서 진행해야 합니다. (Task 2 `Round` 테스트)
2. 참가자 ID나 조건 이름에 쉼표, 큰따옴표, 줄바꿈, 한글이 들어간 경우: CSV 열이 밀리지 않고, Excel에서 한글이 깨지지 않아야 합니다. (Task 2 `toCsv` 테스트)
3. 부활 지점이 모두 화면 안에 있거나 플레이어와 가까운 경우: 봇이 눈앞에 갑자기 나타나지 않도록, 그중 가장 먼 지점을 골라야 합니다. (Task 3 `chooseRespawnIndex` 테스트)
4. 상자 후보 위치가 모두 점유되었거나 플레이어 근처뿐인 경우: 같은 자리에 상자가 겹치지 않고, 비어 있는 가장 먼 후보를 골라야 합니다. (Task 3 `nextCrateIndex` 테스트)
5. 탭 전환이나 렉 뒤에 프레임 간격이 크게 벌어진 경우: 물리 스텝이 한꺼번에 몰려 멈추지 않도록, 프레임당 스텝 수에 상한을 둬야 합니다. (Task 1 `stepCount` 테스트)
6. 7.1을 골랐는데 Windows 출력 장치가 스테레오인 경우: 스테레오로 몰래 바꿔 출력하지 않고 시작 게이트에서 막아야 합니다. 그렇지 않으면 외부 시스템이 받는 입력이 기록과 달라집니다. (Task 7 `canUse71` 테스트)

---

## 파일 구조

```
package.json, vite.config.js, index.html, .gitignore
src/
  main.js              부팅, URL 파라미터, 시작 게이트, 메인 루프
  config.js            모든 상수(시간, 가림 수치, 무기 표, 거리 기준)
  rng.js               고정 시드 난수
  fixedStep.js         고정 시간 간격 누적기
  round.js             라운드 타이머·일시정지·성적, 기습 판정
  results.js           CSV 생성·파일 이름·다운로드
  spawn.js             상자 재배치, 봇 부활 지점 선택
  weapons.js           총 발사 간격·탄창·재장전 상태
  input.js             키보드·마우스·포인터 잠금
  camera.js            3인칭 어깨너머 카메라(벽 충돌 시 당김)
  player.js            플레이어 이동·사격·피격·부활
  botBrain.js          봇 상태 판단(순수 함수)
  bots.js              봇 엔티티(이동, 인지, 사격, 소리, 부활)
  physics.js           Rapier 월드, 정적 충돌체, 레이캐스트, 캐릭터 컨트롤러
  nav.js               recast 내비메시, 경로 질의
  ui.js                시작 화면, 시작 게이트, HUD, 결과 화면
  maps/
    expand.js          맵 데이터 → 박스 목록(계단 펼치기), validateMap
    build.js           박스 목록 → three 메시 + Rapier 충돌체 + 문
    index.js           맵 id → 맵 데이터
    practice.js, urban.js, warehouse.js, forest.js
  audio/
    sounds.js          소리 id → 파일·기본 음량
    occlusion.js       가림 목표값과 적용
    speakers71.js      7.1 스피커별 음량 계산
    engine.js          AudioContext, 리스너, 3D 재생, 루프, 배경음악, 가림 갱신
tools/
  audio_sources.json   소리 id별 출처(CC0 경로 또는 Stable Audio 3 프롬프트)
  audio_build.py       CC0 복사·생성·정규화 → public/audio/<id>.wav
public/audio/          최종 음원, CREDITS.md
tests/                 Vitest 테스트
```

## 맵 데이터 형식 (Task 4에서 정의, Task 9에서 사용)

```js
export default {
  id: 'urban', name: '도심 골목', size: [80, 80], seed: 101,
  surface: 'concrete',              // 기본 바닥 재질: 'concrete' | 'metal' | 'dirt'
  ambience: 'amb_urban', bgm: 'bgm_urban',
  solids: [{ pos: [x, y, z], size: [w, h, d], rotY: 0, surface: 'metal', color: 0x888888 }], // pos는 박스 중심
  stairs: [{ from: [x, y, z], dir: '+x', steps: 12, rise: 0.25, run: 0.3, width: 1.5, surface: 'concrete' }],
  doors:  [{ pos: [x, y, z], size: [w, h, d], rotY: 0 }],
  playerSpawn: [x, y, z],
  bots: [{ weapon: 'rifle', spawn: [x, y, z], patrol: [[x, y, z], [x, y, z]] }],
  respawnPoints: [[x, y, z]],
  crateCandidates: [[x, y, z]], crateCount: 6,
};
```

---

### Task 1: 프로젝트 골격과 기본 유틸

**Files:**
- Create: `package.json`, `vite.config.js`, `index.html`, `.gitignore`, `src/config.js`, `src/rng.js`, `src/fixedStep.js`
- Test: `tests/core.test.js`

**Interfaces:**
- Produces:
  - `createRng(seed: number): () => number` — mulberry32, [0, 1) 범위
  - `stepCount(accumulator: number, frameDt: number, fixedDt: number, maxSteps: number): { steps: number, accumulator: number }` — 상한을 넘는 누적 시간은 버립니다.
  - `config.js` 상수: `ROUND_SECONDS = 180`, `PRACTICE_SECONDS = 90`, `FIXED_DT = 1/60`, `MAX_STEPS_PER_FRAME = 5`, `OCCLUSION = { gain: 0.35, cutoffHz: 1500, openCutoffHz: 20000, rampSeconds: 0.15, updateSeconds: 0.1 }`, `BOT_RESPAWN_SECONDS = 10`, `PLAYER_MAX_HEALTH = 100`, `MIN_RESPAWN_DIST_M = 15`, `MIN_CRATE_DIST_M = 10`, `SHOT_HEARING_M = 40`, `BOT_FOV_DEG = 110`, `BOT_REACTION_S = 0.4`, `BOT_LOST_SIGHT_S = 8`, `WEAPONS` (아래 표)

`WEAPONS` 시작값입니다. Task 10에서 조정합니다.

| id | damage | intervalS | magSize | reloadS | rangeM | spreadDeg | fireSound | reloadSound |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| playerRifle | 25 | 0.12 | 30 | 2.0 | 120 | 0.5 | player_rifle_shot | rifle_reload |
| rifle | 10 | 0.12 | 30 | 2.2 | 45 | 3 | rifle_shot | rifle_reload |
| shotgun | 55 | 1.0 | 6 | 3.0 | 12 | 6 | shotgun_shot | shotgun_pump |
| sniper | 70 | 2.2 | 5 | 3.5 | 90 | 0.8 | sniper_shot | sniper_bolt |
| knife | 30 | 0.8 | 0 | 0 | 1.8 | 0 | knife_swing | null |

`magSize = 0`은 탄창이 없는 무기입니다. 산탄총은 매 발사 뒤 `shotgun_pump`, 저격총은 매 발사 뒤 `sniper_bolt`를 재생합니다(`afterFireSound` 필드).

- [ ] **Step 1: 저장소를 만들고 의존성을 고정 버전으로 설치합니다**

```powershell
cd "D:\Epic Games\Unreal Project\SoundCompass\soundcompass-testbed"
git init
npm init -y
npm install --save-exact three@0.187.0 @dimforge/rapier3d-compat@0.21.0 @recast-navigation/core@0.44.0 @recast-navigation/three@0.44.0 @recast-navigation/generators@0.44.0
npm install --save-exact --save-dev vite@8.3.8 vitest@5.1.1
```

`package.json`에 `"type": "module"`, 스크립트 `dev: vite`, `build: vite build`, `preview: vite preview`, `test: vitest run`을 넣습니다. `vite.config.js`는 `base: './'`만 지정합니다. `.gitignore`는 `node_modules/`, `dist/`입니다. `index.html`은 전체 화면 `<canvas id="game">`과 `<div id="ui">`, `<script type="module" src="/src/main.js">`만 둡니다.

- [ ] **Step 2: 실패하는 테스트를 씁니다**

```js
import { describe, it, expect } from 'vitest';
import { createRng } from '../src/rng.js';
import { stepCount } from '../src/fixedStep.js';

describe('createRng', () => {
  it('같은 시드는 같은 수열을 낸다', () => {
    const a = createRng(42), b = createRng(42);
    const sa = [a(), a(), a()], sb = [b(), b(), b()];
    expect(sa).toEqual(sb);
    sa.forEach(v => { expect(v).toBeGreaterThanOrEqual(0); expect(v).toBeLessThan(1); });
  });
  it('다른 시드는 다른 수열을 낸다', () => {
    expect(createRng(1)()).not.toBe(createRng(2)());
  });
});

describe('stepCount', () => {
  it('누적 시간을 고정 간격 단위로 나눈다', () => {
    const r = stepCount(0, 0.05, 1 / 60, 5);
    expect(r.steps).toBe(3);
    expect(r.accumulator).toBeCloseTo(0.05 - 3 / 60, 6);
  });
  it('큰 프레임 간격은 상한에서 자르고 남은 시간을 버린다', () => {
    const r = stepCount(0, 2.0, 1 / 60, 5);
    expect(r.steps).toBe(5);
    expect(r.accumulator).toBe(0);
  });
});
```

- [ ] **Step 3: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/core.test.js`
Expected: FAIL (모듈 없음)

- [ ] **Step 4: `src/rng.js`, `src/fixedStep.js`, `src/config.js`를 구현합니다**

- [ ] **Step 5: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/core.test.js`
Expected: PASS (4 tests)

- [ ] **Step 6: 커밋합니다**

```powershell
git add -A
git commit -m "chore: scaffold testbed with pinned deps, rng and fixed step"
```

---

### Task 2: 라운드 상태와 결과 CSV

**Files:**
- Create: `src/round.js`, `src/results.js`
- Test: `tests/round.test.js`

**Interfaces:**
- Consumes: `ROUND_SECONDS`, `PRACTICE_SECONDS` (Task 1)
- Produces:
  - `class Round` — `constructor({ participantId: string, condition: string, mapId: string, audioOutput: 'binaural' | '7.1', isPractice: boolean, startedAt: Date })`. 길이는 `isPractice`에 따라 `PRACTICE_SECONDS` 또는 `ROUND_SECONDS`.
    - `tick(dt: number): void` — 일시정지 중에는 무시합니다.
    - `pause(): void`, `resume(): void`, `get paused: boolean`
    - `get remainingS: number` (0 미만으로 내려가지 않음), `get finished: boolean`
    - `stats: { crates, kills, deaths, damageTaken, surpriseHits }` (모두 0으로 시작)
    - `addCrate()`, `addKill()`, `addDeath()`, `addDamage(amount: number, surprise: boolean)` — `finished` 뒤에는 무시합니다.
  - `isSurpriseHit(inView: boolean, lineOfSightBlocked: boolean): boolean` — `!inView || lineOfSightBlocked`
  - `CSV_HEADER = 'participant_id,condition,map,audio_output,started_at,crates,kills,deaths,damage_taken,surprise_hits'`
  - `csvField(value: string | number): string` — 쉼표, 큰따옴표, 줄바꿈이 있으면 큰따옴표로 감싸고 안의 큰따옴표는 두 번 씁니다.
  - `toCsv(round: Round): string` — `'\uFEFF' + CSV_HEADER + '\r\n' + 데이터 한 줄 + '\r\n'`. `started_at`은 ISO 8601. 스펙의 "한 줄짜리 CSV"는 데이터 한 줄로 해석하고, 읽기 쉽도록 헤더 줄을 함께 씁니다.
  - `csvFilename(round: Round): string` — `${participantId}_${mapId}_${YYYYMMDD-HHmmss}.csv`. 파일 이름에 쓸 수 없는 문자(`\/:*?"<>|`와 공백)는 `_`로 바꿉니다.
  - `downloadCsv(round: Round): void` — `Blob` + `<a download>`. 연습 라운드면 아무것도 하지 않습니다.

- [ ] **Step 1: 실패하는 테스트를 씁니다**

```js
import { describe, it, expect } from 'vitest';
import { Round, isSurpriseHit } from '../src/round.js';
import { toCsv, csvField, csvFilename, CSV_HEADER } from '../src/results.js';

const mk = (o = {}) => new Round({ participantId: 'P01', condition: 'A', mapId: 'urban', audioOutput: 'binaural', isPractice: false, startedAt: new Date('2026-12-01T10:20:30Z'), ...o });

describe('Round', () => {
  it('본 라운드는 180초, 연습은 90초', () => {
    expect(mk().remainingS).toBe(180);
    expect(mk({ isPractice: true }).remainingS).toBe(90);
  });
  it('일시정지 중에는 시간이 줄지 않는다', () => {
    const r = mk(); r.tick(10); r.pause(); r.tick(50); r.resume(); r.tick(5);
    expect(r.remainingS).toBe(165);
  });
  it('시간이 다 되면 끝나고 이후 기록은 무시한다', () => {
    const r = mk(); r.tick(200);
    expect(r.finished).toBe(true); expect(r.remainingS).toBe(0);
    r.addKill(); expect(r.stats.kills).toBe(0);
  });
  it('피해와 기습 횟수를 함께 센다', () => {
    const r = mk(); r.addDamage(30, true); r.addDamage(10, false);
    expect(r.stats.damageTaken).toBe(40); expect(r.stats.surpriseHits).toBe(1);
  });
});

describe('isSurpriseHit', () => {
  it('화면 밖이거나 가려지면 기습', () => {
    expect(isSurpriseHit(false, false)).toBe(true);
    expect(isSurpriseHit(true, true)).toBe(true);
    expect(isSurpriseHit(true, false)).toBe(false);
  });
});

describe('CSV', () => {
  it('쉼표·따옴표·줄바꿈을 이스케이프한다', () => {
    expect(csvField('a,b')).toBe('"a,b"');
    expect(csvField('say "hi"')).toBe('"say ""hi"""');
    expect(csvField('줄\n바꿈')).toBe('"줄\n바꿈"');
    expect(csvField('시스템켬')).toBe('시스템켬');
  });
  it('BOM, 헤더, 데이터 한 줄을 쓴다', () => {
    const r = mk({ condition: '소리 끔, 시스템' }); r.addCrate(); r.addKill(); r.addDeath(); r.addDamage(25, true);
    expect(toCsv(r)).toBe('\uFEFF' + CSV_HEADER + '\r\n' + 'P01,"소리 끔, 시스템",urban,binaural,2026-12-01T10:20:30.000Z,1,1,1,25,1\r\n');
  });
  it('파일 이름의 금지 문자를 바꾼다', () => {
    expect(csvFilename(mk({ participantId: 'P 01/a' }))).toMatch(/^P_01_a_urban_\d{8}-\d{6}\.csv$/);
  });
});
```

- [ ] **Step 2: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/round.test.js`
Expected: FAIL (모듈 없음)

- [ ] **Step 3: `src/round.js`, `src/results.js`를 구현합니다**

파일 이름의 시각은 실험 진행자가 보기 쉽도록 로컬 시각을 씁니다.

- [ ] **Step 4: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/round.test.js`
Expected: PASS

- [ ] **Step 5: 커밋합니다**

```powershell
git add -A
git commit -m "feat: round timer with pause, surprise-hit rule and CSV results"
```

---

### Task 3: 상자 재배치와 봇 부활 지점

**Files:**
- Create: `src/spawn.js`
- Test: `tests/spawn.test.js`

**Interfaces:**
- Consumes: `createRng` (Task 1)
- Produces (좌표는 `{x, y, z}` 또는 `[x, y, z]` 중 `[x, y, z]` 배열로 통일):
  - `nextCrateIndex(candidates: number[][], occupied: Set<number>, playerPos: number[], rng: () => number, minDistM: number): number` — 비어 있고 플레이어와 `minDistM` 이상 떨어진 후보 중 하나를 `rng`로 고릅니다. 그런 후보가 없으면 비어 있는 후보 중 가장 먼 것을 고릅니다. 비어 있는 후보가 없으면 `-1`.
  - `initialCrateIndices(candidates: number[][], count: number, rng): number[]` — 서로 다른 인덱스 `count`개.
  - `chooseRespawnIndex(points: number[][], playerPos: number[], isVisible: (p: number[]) => boolean, minDistM: number): number` — 보이지 않고 `minDistM` 이상 떨어진 지점 중 가장 먼 것을 고릅니다. 없으면 보이지 않는 지점 중 가장 먼 것, 그것도 없으면 전체 중 가장 먼 것입니다.

- [ ] **Step 1: 실패하는 테스트를 씁니다**

```js
import { describe, it, expect } from 'vitest';
import { nextCrateIndex, initialCrateIndices, chooseRespawnIndex } from '../src/spawn.js';
import { createRng } from '../src/rng.js';

const pts = [[0, 0, 0], [5, 0, 0], [20, 0, 0], [40, 0, 0]];

describe('nextCrateIndex', () => {
  it('점유되지 않고 충분히 먼 후보만 고른다', () => {
    const rng = createRng(1);
    for (let i = 0; i < 20; i++) expect([2]).toContain(nextCrateIndex(pts, new Set([3]), [0, 0, 0], rng, 10));
  });
  it('먼 후보가 없으면 비어 있는 가장 먼 후보', () => {
    expect(nextCrateIndex(pts, new Set([2, 3]), [0, 0, 0], createRng(1), 10)).toBe(1);
  });
  it('모두 점유면 -1', () => {
    expect(nextCrateIndex(pts, new Set([0, 1, 2, 3]), [0, 0, 0], createRng(1), 10)).toBe(-1);
  });
});

describe('initialCrateIndices', () => {
  it('서로 다른 인덱스를 count개 고른다', () => {
    const idx = initialCrateIndices(pts, 3, createRng(7));
    expect(new Set(idx).size).toBe(3);
  });
});

describe('chooseRespawnIndex', () => {
  it('보이지 않고 먼 지점 중 가장 먼 곳', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], p => p[0] === 40, 15)).toBe(2);
  });
  it('모두 보이면 가장 먼 곳', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], () => true, 15)).toBe(3);
  });
  it('보이지 않는 곳이 모두 가까우면 그중 가장 먼 곳', () => {
    expect(chooseRespawnIndex(pts, [0, 0, 0], p => p[0] >= 20, 15)).toBe(1);
  });
});
```

- [ ] **Step 2: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/spawn.test.js`
Expected: FAIL

- [ ] **Step 3: `src/spawn.js`를 구현합니다**

- [ ] **Step 4: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/spawn.test.js`
Expected: PASS

- [ ] **Step 5: 커밋합니다**

```powershell
git add -A
git commit -m "feat: crate relocation and off-screen bot respawn choice"
```

---

### Task 4: 맵 데이터, 물리 월드, 연습장 렌더링

**Files:**
- Create: `src/maps/expand.js`, `src/maps/build.js`, `src/maps/index.js`, `src/maps/practice.js`, `src/physics.js`, `src/main.js`(임시: 연습장을 자유 카메라로 보여 주는 수준)
- Test: `tests/maps.test.js`

**Interfaces:**
- Consumes: `WEAPONS` (Task 1)
- Produces:
  - `expandMap(map): Box[]` — `Box = { pos: number[], size: number[], rotY: number, surface: string, color: number, kind: 'solid' | 'step' }`. `solids`를 그대로 옮기고, `stairs`를 계단 한 칸마다 박스로 펼칩니다(칸 높이 `rise`, 깊이 `run`, 각 칸은 바닥부터 그 높이까지 채운 박스). 문은 포함하지 않습니다.
  - `validateMap(map, { isPractice: boolean }): string[]` — 오류 메시지 목록. 확인 항목: 본 맵은 봇 4명(소총, 산탄총, 저격총, 칼 각 1명), 연습장은 1~2명; `crateCandidates.length >= crateCount * 2`; `respawnPoints.length >= 3`; 모든 좌표가 `size` 범위 안(x, z가 ±size/2); `WEAPONS`에 없는 무기 없음; `surface`가 `'concrete' | 'metal' | 'dirt'` 중 하나.
  - `MAPS: Record<'practice' | 'urban' | 'warehouse' | 'forest', MapData>` (`src/maps/index.js`, 이 Task에서는 practice만)
  - `createPhysics(RAPIER): Physics` — `world`(중력 -9.81, `timestep = FIXED_DT`), `step()`, `addStaticBox(box: Box): Collider`, `raycast(from: number[], to: number[], opts?: { exclude?: Collider, staticOnly?: boolean }): { hit: boolean, point?: number[], collider?: Collider }`, `createCharacter(pos: number[], radius = 0.35, halfHeight = 0.55): Character`. 충돌 그룹은 STATIC, DOOR, CHARACTER 세 개로 나눕니다. `staticOnly: true`는 STATIC과 DOOR(닫힌 문은 활성 충돌체, 열린 문은 비활성)에만 맞으므로, 시야와 가림 판정에서 닫힌 문이 벽처럼 동작합니다.
  - `Character` — `collider`, `get position(): number[]`(캡슐 바닥 기준), `move(desired: number[], dt: number): { grounded: boolean }`. Rapier 캐릭터 컨트롤러에 `enableAutostep(0.3, 0.2, false)`, `enableSnapToGround(0.3)`, `setMaxSlopeClimbAngle(45°)`를 설정합니다.
  - `buildMap(map, { scene, physics }): { navMeshes: THREE.Mesh[], surfaceAt(collider): string, doors: Door[] }` — 박스마다 `MeshLambertMaterial` 단색 메시와 정적 충돌체를 만듭니다. 바닥 평면도 박스 하나로 넣습니다. 조명은 `HemisphereLight` 하나와 `DirectionalLight` 하나입니다.
  - `Door` — `update(dt, characterPositions: number[][])`. 캐릭터가 1.5 m 안에 들어오면 열리고(DOOR 충돌체 비활성, `door_open` 소리 이벤트), 아무도 없으면 3초 뒤 닫힙니다(`door_close`). 소리는 `onSound(id, pos)` 콜백으로 내보내며, Task 7에서 오디오에 연결합니다. 닫힌 문은 가림 레이캐스트에 걸립니다.

연습장(`practice.js`): 40×40 m, 계단이 있는 2층 건물 하나(2층 바닥 높이 3 m, 창문 구멍 2개, 문 1개), `crateCount: 3`, 후보 6개, 봇 2명(rifle, knife), 부활 지점 3개, `surface: 'concrete'`, `ambience: 'amb_urban'`, `bgm: 'bgm_urban'`.

- [ ] **Step 1: 실패하는 테스트를 씁니다**

```js
import { describe, it, expect } from 'vitest';
import { expandMap, validateMap } from '../src/maps/expand.js';
import { MAPS } from '../src/maps/index.js';

describe('expandMap', () => {
  it('계단을 칸마다 박스로 펼친다', () => {
    const boxes = expandMap({ solids: [], stairs: [{ from: [0, 0, 0], dir: '+x', steps: 4, rise: 0.25, run: 0.3, width: 1.5, surface: 'concrete' }] });
    const steps = boxes.filter(b => b.kind === 'step');
    expect(steps).toHaveLength(4);
    expect(Math.max(...steps.map(b => b.pos[1] + b.size[1] / 2))).toBeCloseTo(1.0);
    expect(steps[3].pos[0]).toBeGreaterThan(steps[0].pos[0]);
  });
});

describe('validateMap', () => {
  it('연습장은 오류가 없다', () => {
    expect(validateMap(MAPS.practice, { isPractice: true })).toEqual([]);
  });
  it('범위 밖 좌표와 부족한 후보를 잡아낸다', () => {
    const bad = { ...MAPS.practice, playerSpawn: [999, 0, 0], crateCandidates: [[0, 0, 0]] };
    expect(validateMap(bad, { isPractice: true }).length).toBeGreaterThanOrEqual(2);
  });
});
```

- [ ] **Step 2: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/maps.test.js`
Expected: FAIL

- [ ] **Step 3: `expand.js`, `practice.js`, `index.js`를 구현합니다**

- [ ] **Step 4: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/maps.test.js`
Expected: PASS

- [ ] **Step 5: `physics.js`, `build.js`, 임시 `main.js`를 구현합니다**

`main.js`는 `await RAPIER.init()` 뒤 연습장을 만들고, `OrbitControls`(three/addons)로 둘러볼 수 있게 합니다. 이 임시 코드는 Task 5에서 교체합니다.

- [ ] **Step 6: Chrome에서 연습장이 보이는지 확인합니다**

Run: `npm run dev` 후 Chrome에서 표시된 주소를 엽니다.
Expected: 2층 건물, 계단, 창문 구멍, 문이 보이고 콘솔 오류가 없습니다.

- [ ] **Step 7: 커밋합니다**

```powershell
git add -A
git commit -m "feat: map data format, physics world and practice map"
```

---

### Task 5: 플레이어, 3인칭 카메라, 라운드 흐름

**Files:**
- Create: `src/input.js`, `src/camera.js`, `src/weapons.js`, `src/player.js`, `src/ui.js`
- Modify: `src/main.js` (임시 코드 교체)
- Test: `tests/weapons.test.js`

**Interfaces:**
- Consumes: `Round`, `isSurpriseHit`, `downloadCsv` (Task 2), `createPhysics`, `buildMap`, `MAPS` (Task 4), `stepCount`, `WEAPONS`, `PLAYER_MAX_HEALTH` (Task 1)
- Produces:
  - `createGun(def): Gun` — `tryFire(now: number): boolean`, `startReload(now: number): boolean`, `update(now: number): void`, `get ammo: number`, `get reloading: boolean`. 발사 간격 전에는 `false`, 탄이 0이면 `false`를 돌려주고 자동으로 재장전을 시작합니다. 재장전 중에는 쏠 수 없습니다. `magSize = 0`이면 탄 제한이 없습니다.
  - `createInput(canvas): Input` — `keys: Set<string>`(`KeyboardEvent.code`), `consumeMouse(): { dx, dy }`, `get firing: boolean`, `get locked: boolean`, `onLockChange(cb: (locked: boolean) => void)`, `requestLock()`
  - `updateThirdPersonCamera(camera, target: number[], yaw: number, pitch: number, physics): void` — 오른쪽 0.5 m, 위 1.7 m, 뒤 3.0 m. 카메라와 목표 사이를 벽이 막으면 맞은 지점 0.2 m 앞으로 당깁니다. pitch는 ±70°로 제한합니다.
  - `createPlayer({ scene, physics, map, round, camera, onSound }): Player` — `update(dt, input)`, `get position: number[]`, `get health: number`, `takeDamage(amount: number, attackerPos: number[])`, `get collider`. 이동 속도 걷기 4.5 m/s, 달리기(Shift) 7 m/s, 점프 5 m/s. 사격은 화면 중앙 레이캐스트(사거리 `playerRifle.rangeM`)이며, 맞은 충돌체를 `onHit(collider, damage)` 콜백으로 넘깁니다(Task 8에서 봇에 연결). 소리는 `onSound(id, pos)`로 내보냅니다: 발사, 재장전, 발소리(걷기 0.5초, 달리기 0.33초 간격, 바닥 재질은 발밑 레이캐스트의 `surfaceAt`), 착지, 피격, 사망.
  - `takeDamage`는 `inView`를 카메라 `Frustum.containsPoint(공격자 가슴 높이)`로, `lineOfSightBlocked`를 카메라→공격자 정적 레이캐스트로 구하고 `round.addDamage(amount, isSurpriseHit(...))`를 부릅니다. 체력이 0이 되면 `round.addDeath()` 후 `playerSpawn`에서 체력 100으로 즉시 부활합니다.
  - `ui.js` — `showStartForm(onSubmit: ({ participantId, condition, mapId, audioOutput }) => void)`, `showStartGate(onClick)`, `showStartError(message: string)`, `updateHud({ health, remainingS, crates, ammo, reloading })`, `showPaused()`, `hidePaused()`, `showResults(round, { onNext })`
  - URL 파라미터: `?pid=&cond=&map=&out=` (`out`은 `binaural` 또는 `7.1`, 기본값 `binaural`). 시작 화면의 출력 방식 선택도 이 두 값입니다. 파라미터가 없으면 시작 화면을 보여 주고, 제출하면 이 파라미터로 페이지를 다시 엽니다. 결과 화면의 "다음"은 파라미터 없이 다시 엽니다.

시작 화면 검증: 참가자 ID는 앞뒤 공백을 제거한 뒤 비어 있으면 제출할 수 없습니다. 맵은 practice, urban, warehouse, forest 중 하나를 고르고, `map=practice`이면 `isPractice = true`입니다. 조건 이름은 자유 입력입니다.

메인 루프: `requestAnimationFrame`마다 `stepCount`로 고정 스텝을 돌리고, 포인터 잠금이 풀리면 `round.pause()`와 `showPaused()`, 다시 잠기면 `resume()`합니다. `round.finished`가 되면 포인터 잠금을 풀고 `downloadCsv(round)` 후 `showResults`를 보여 줍니다.

- [ ] **Step 1: 실패하는 테스트를 씁니다**

```js
import { describe, it, expect } from 'vitest';
import { createGun } from '../src/weapons.js';
import { WEAPONS } from '../src/config.js';

describe('createGun', () => {
  it('발사 간격을 지킨다', () => {
    const g = createGun(WEAPONS.playerRifle);
    expect(g.tryFire(0)).toBe(true);
    expect(g.tryFire(0.05)).toBe(false);
    expect(g.tryFire(0.13)).toBe(true);
  });
  it('탄창이 비면 자동 재장전하고 끝나면 다시 쏜다', () => {
    const g = createGun({ ...WEAPONS.playerRifle, magSize: 2 });
    g.tryFire(0); g.tryFire(1);
    expect(g.tryFire(2)).toBe(false);
    expect(g.reloading).toBe(true);
    g.update(2 + WEAPONS.playerRifle.reloadS + 0.01);
    expect(g.ammo).toBe(2);
    expect(g.tryFire(5)).toBe(true);
  });
  it('탄창 없는 무기는 탄 제한이 없다', () => {
    const k = createGun(WEAPONS.knife);
    for (let i = 0; i < 50; i++) expect(k.tryFire(i)).toBe(true);
  });
});
```

- [ ] **Step 2: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/weapons.test.js`
Expected: FAIL

- [ ] **Step 3: `src/weapons.js`를 구현합니다**

- [ ] **Step 4: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/weapons.test.js`
Expected: PASS

- [ ] **Step 5: `input.js`, `camera.js`, `player.js`, `ui.js`, `main.js`를 구현합니다**

- [ ] **Step 6: Chrome에서 라운드 흐름을 확인합니다**

Run: `npm run dev`
Expected:
- 시작 화면에서 ID가 비어 있으면 시작되지 않습니다.
- `map=practice`로 시작하면 클릭 게이트 뒤에 조작할 수 있고, 2층으로 계단을 올라갈 수 있습니다.
- 벽에 붙어도 카메라가 벽 안으로 들어가지 않습니다.
- Esc를 누르면 일시정지 화면이 뜨고 HUD의 시간이 멈춥니다.
- 연습 라운드가 끝나면 결과 화면이 뜨고 CSV는 내려받지 않습니다. 본 라운드의 CSV 다운로드는 본 맵이 생기는 Task 9 Step 5에서 확인합니다.

- [ ] **Step 7: 커밋합니다**

```powershell
git add -A
git commit -m "feat: third-person player, gun, HUD and round flow"
```

---

### Task 6: 음원 준비 (CC0 복사, Stable Audio 3 생성, 정규화)

**Files:**
- Create: `tools/audio_sources.json`, `tools/audio_build.py`, `src/audio/sounds.js`, `public/audio/*.wav`, `public/audio/CREDITS.md`
- Test: `tests/sounds.test.js`

**Interfaces:**
- Produces:
  - `SOUNDS: Record<string, { files: string[], gain: number, kind: 'sfx' | 'amb' | 'bgm' }>` — `files`는 `audio/<id>.wav` 또는 변형이 있으면 `audio/<id>_1.wav ... _6.wav`. `kind`가 `sfx`면 모노, `amb`/`bgm`은 스테레오입니다.
  - 소리 id 목록(다른 Task는 이 이름만 씁니다):

| id | 출처 | 비고 |
| --- | --- | --- |
| player_rifle_shot, rifle_shot | CC0 `OneShots/Gunshots/SW_Gun_Rifle_AR15.wav` | 같은 파일 |
| shotgun_shot | CC0 `SW_Gun_Shotgun_Model12.wav` | |
| sniper_shot | CC0 `SW_Gun_Rifle_MosinNagant.wav` | |
| rifle_reload, shotgun_pump, sniper_bolt | Stable Audio 3 | |
| knife_swing, knife_hit | Stable Audio 3 | |
| step_concrete (6) | CC0 `Footsteps/SW_Footstep_Concrete_1..6.wav` | |
| step_dirt (6) | CC0 `Footsteps/SW_Footstep_Gravel_1..6.wav` | |
| step_metal (6) | Stable Audio 3, 시드 6개 | |
| land | Stable Audio 3 | |
| door_open, door_close | CC0 `Door/SW_DoorOpen.wav`, `SW_DoorClose.wav` | |
| crate_pickup | CC0 `Items/SW_Item_Medkit_1.wav` | |
| hit_taken, death | Stable Audio 3 | |
| amb_urban, amb_warehouse, amb_forest | Stable Audio 3, 30초 | 루프 |
| bgm_urban, bgm_warehouse, bgm_forest | Stable Audio 3, 60초(오류 나면 30초) | 루프 |

CC0 경로의 기준 폴더는 `C:\Users\Home\Documents\Github\SoundCompass\Saved\SoundTestSetup\wav`이며, 출처 정보는 같은 저장소의 `Content/SoundTest/CREDITS.md`에서 해당 항목을 `public/audio/CREDITS.md`로 옮깁니다.

`audio_build.py` 동작:
- `audio_sources.json` 항목마다 `{ id, kind, variants?, source: 'cc0', path }` 또는 `{ id, kind, variants?, source: 'sa3', prompt, seconds, seed }`를 읽습니다.
- `sa3`는 `D:\ComfyUI\workflows\stable_audio3_run.py`의 워크플로를 그대로 쓰고 `pos.text`, `lat.seconds`, `ks.seed`, `sv.filename_prefix`만 바꿔 `http://127.0.0.1:8188/prompt`에 제출합니다. 출력은 `C:\Users\Home\AppData\Local\Comfy-Desktop\ComfyUI-Shared\output\audio\`에 생깁니다. 실행은 ComfyUI venv Python(`C:\Users\Home\AppData\Local\Comfy-Desktop\ComfyUI-Installs\ComfyUI\ComfyUI\.venv\Scripts\python.exe -X utf8`)으로 합니다. 짧은 효과음은 `seconds: 3`으로 생성합니다.
- ffmpeg 후처리: `sfx`는 모노 48 kHz, 앞뒤 무음 제거(`silenceremove`), 끝 0.05초 페이드, `loudnorm=I=-18`. `amb`는 스테레오 48 kHz, `loudnorm=I=-30`. `bgm`은 스테레오 48 kHz, `loudnorm=I=-26`. `amb`/`bgm`은 끝 2초를 처음 2초에 크로스페이드해서 루프 이음새를 없앱니다.
- 이미 `public/audio/<id>.wav`가 있으면 건너뜁니다(`--force`로 다시 만듦).
- 생성 프롬프트는 빌더가 정하되, 효과음 프롬프트에는 "no music"을, 배경음악 프롬프트에는 맵 분위기(도심 긴장감, 창고 산업적, 숲 불길함)와 "loopable, no vocals"를 넣습니다.

- [ ] **Step 1: 실패하는 테스트를 씁니다**

```js
import { describe, it, expect } from 'vitest';
import { existsSync } from 'node:fs';
import { SOUNDS } from '../src/audio/sounds.js';

describe('SOUNDS', () => {
  it('참조하는 파일이 모두 public/audio에 있다', () => {
    const missing = Object.values(SOUNDS).flatMap(s => s.files).filter(f => !existsSync(`public/${f}`));
    expect(missing).toEqual([]);
  });
  it('필수 id가 모두 있다', () => {
    for (const id of ['player_rifle_shot', 'rifle_shot', 'shotgun_shot', 'sniper_shot', 'rifle_reload', 'shotgun_pump', 'sniper_bolt', 'knife_swing', 'knife_hit', 'step_concrete', 'step_dirt', 'step_metal', 'land', 'door_open', 'door_close', 'crate_pickup', 'hit_taken', 'death', 'amb_urban', 'amb_warehouse', 'amb_forest', 'bgm_urban', 'bgm_warehouse', 'bgm_forest'])
      expect(SOUNDS[id], id).toBeDefined();
  });
});
```

- [ ] **Step 2: `src/audio/sounds.js`만 먼저 쓰고 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/sounds.test.js`
Expected: FAIL (파일 없음 목록 출력)

- [ ] **Step 3: ComfyUI 서버가 켜져 있는지 확인합니다**

Run: `curl -s http://127.0.0.1:8188/system_stats`
Expected: JSON 응답. 응답이 없으면 사용자에게 ComfyUI Desktop 실행을 요청하고 기다립니다.

- [ ] **Step 4: `tools/audio_sources.json`, `tools/audio_build.py`를 쓰고 실행합니다**

Run: `& "C:\Users\Home\AppData\Local\Comfy-Desktop\ComfyUI-Installs\ComfyUI\ComfyUI\.venv\Scripts\python.exe" -X utf8 tools/audio_build.py`
Expected: 각 id에 대해 `ok <id>` 출력, 실패 없음.

- [ ] **Step 5: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/sounds.test.js`
Expected: PASS

- [ ] **Step 6: 생성한 소리를 들어 보고 쓸 수 없는 것을 다시 만듭니다**

생성한 음원 목록을 사용자에게 보여 주고 확인을 받습니다. 쓸 수 없는 소리는 시드나 프롬프트를 바꿔 `--force`로 다시 만듭니다.

- [ ] **Step 7: 커밋합니다**

```powershell
git add -A
git commit -m "feat: sound set from CC0 library and Stable Audio 3"
```

---

### Task 7: 오디오 엔진과 가림

**Files:**
- Create: `src/audio/occlusion.js`, `src/audio/speakers71.js`, `src/audio/engine.js`
- Modify: `src/main.js`, `src/maps/build.js`(문 소리 연결), `src/player.js`(onSound 연결)
- Test: `tests/occlusion.test.js`, `tests/speakers71.test.js`

**Interfaces:**
- Consumes: `SOUNDS` (Task 6), `OCCLUSION` (Task 1), `physics.raycast` (Task 4), `onSound` 콜백 (Task 4, 5)
- Produces:
  - `occlusionTarget(blocked: boolean): { gain: number, cutoffHz: number }` — 막히면 `{ 0.35, 1500 }`, 아니면 `{ 1, 20000 }`.
  - `applyOcclusion(voice: { occGain: GainNode, filter: BiquadFilterNode }, target, now: number): void` — 두 값 모두 `cancelScheduledValues(now)`, `setValueAtTime(현재값, now)`, `linearRampToValueAtTime(목표, now + 0.15)`.
  - `SPEAKER_ORDER_71 = ['FL', 'FR', 'FC', 'LFE', 'BL', 'BR', 'SL', 'SR']` — WAVE/Windows 채널 순서
  - `SPEAKER_AZIMUTH_71 = { FC: 0, FR: 30, SR: 90, BR: 150, BL: -150, SL: -90, FL: -30 }` — 도 단위, 양수는 오른쪽
  - `speakerGains71(azimuthDeg: number): number[]` — 길이 8, `SPEAKER_ORDER_71` 순서. 방위각을 [-180, 180)으로 정규화하고, 원을 따라 인접한 두 스피커 사이에서 일정 출력 패닝(`cos`/`sin`)을 합니다. 제곱합은 항상 1이고 LFE는 항상 0입니다.
  - `canUse71(maxChannelCount: number): boolean` — `maxChannelCount >= 8`
  - `createAudio({ output: 'binaural' | '7.1' }): AudioEngine`
    - `load(): Promise<void>` — `SOUNDS` 전체를 `decodeAudioData`
    - `resume(): Promise<void>` — 시작 게이트 클릭에서 호출. `output === '7.1'`이고 `!canUse71(ctx.destination.maxChannelCount)`이면 `Error('출력 장치가 7.1(8채널)이 아닙니다. Windows 소리 설정에서 7.1로 바꾸세요.')`를 던지고, `main.js`는 `showStartError`로 보여 준 뒤 시작하지 않습니다. 7.1이면 `destination.channelCount = 8`, `channelCountMode = 'explicit'`, `channelInterpretation = 'discrete'`로 설정합니다.
    - `setListener(camera): void` — 매 프레임 위치와 앞/위 방향을 `AudioListener`에 반영
    - `playAt(id: string, pos: number[] | (() => number[]), opts?: { gain?: number }): void` — 변형이 있으면 무작위로 하나 고릅니다. 체인: `BufferSource → gain → occGain → filter(lowpass) → PannerNode(HRTF, inverse, refDistance 2, rolloffFactor 1, maxDistance 120) → master`. `pos`가 함수면 매 프레임 위치를 갱신합니다(움직이는 봇 발소리).
      7.1에서는 `PannerNode` 대신 `filter → distGain → 스피커별 GainNode 7개 → ChannelMergerNode(8)의 해당 입력 → master(8채널)`입니다. `distGain`은 inverse 거리 모델(refDistance 2, rolloffFactor 1, maxDistance 120)을 직접 계산하고, 스피커 음량은 매 프레임 카메라 기준 수평 방위각으로 `speakerGains71`을 구해 `setTargetAtTime(값, now, 0.02)`로 반영합니다.
    - `startBed(id: string): void` — 위치 없는 스테레오 루프(환경음, 배경음악). 가림을 적용하지 않습니다. 7.1에서는 스테레오의 L, R을 FL, FR 채널에 보냅니다.
    - `update(isBlocked: (from: number[], to: number[]) => boolean): void` — 재생 중인 3D 소리마다 `OCCLUSION.updateSeconds` 간격으로 리스너→음원 정적 레이캐스트(닫힌 문 포함)를 하고, 결과가 바뀌면 `applyOcclusion`을 부릅니다. 재생이 끝난 소리는 목록에서 뺍니다.
  - 음량: `master` 1.0 고정. 실험용 음소거나 음량 조절 UI는 만들지 않습니다.

- [ ] **Step 1: 실패하는 테스트를 씁니다**

```js
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
    expect(voice.occGain.gain.linearRampToValueAtTime).toHaveBeenCalledWith(0.35, 10.15);
    expect(voice.filter.frequency.linearRampToValueAtTime).toHaveBeenCalledWith(1500, 10.15);
  });
});
```

`tests/speakers71.test.js`:

```js
import { describe, it, expect } from 'vitest';
import { speakerGains71, SPEAKER_ORDER_71, canUse71 } from '../src/audio/speakers71.js';

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
```

- [ ] **Step 2: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/occlusion.test.js tests/speakers71.test.js`
Expected: FAIL

- [ ] **Step 3: `occlusion.js`, `speakers71.js`를 구현합니다**

- [ ] **Step 4: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/occlusion.test.js tests/speakers71.test.js`
Expected: PASS

- [ ] **Step 5: `engine.js`를 구현하고 플레이어, 문, 상자, 환경음, 배경음악을 연결합니다**

상자는 이 Task에서 함께 넣습니다. `main.js`가 `initialCrateIndices`로 상자를 놓고(노란 박스 0.6 m), 플레이어가 1.2 m 안에 들어오면 `round.addCrate()`, `playAt('crate_pickup', ...)`, `nextCrateIndex`로 다른 후보에 새 상자를 놓습니다. 환경음과 배경음악은 시작 게이트 클릭 뒤 `startBed(map.ambience)`, `startBed(map.bgm)`로 재생합니다.

- [ ] **Step 6: Chrome에서 헤드폰으로 확인합니다**

Run: `npm run dev`, `map=practice`
Expected:
- 시작 게이트를 누르기 전에는 소리가 나지 않고, 누른 뒤 환경음과 배경음악이 들립니다.
- 자기 발소리와 총소리가 들리고, 계단과 2층에서는 발소리 재질이 맞습니다.
- 문이 열리고 닫히는 소리가 문 위치에서 들립니다.
- 건물 안에서 문이 닫히면 바깥 소리가 둔해지고, 문이 열리면 0.15초 안에 또렷해집니다.
- 상자를 주우면 획득음이 나고 다른 곳에 새 상자가 생깁니다.
- Windows 출력 장치가 스테레오일 때 `out=7.1`로 시작하면 시작 게이트에서 오류 문구가 뜨고 게임이 시작되지 않습니다.
- 7.1 장치(또는 7.1로 설정한 가상 장치)가 있으면 `out=7.1`로 시작해서, 카메라를 돌릴 때 소리가 해당 스피커 채널로 옮겨 가는지 확인합니다. 장치가 없으면 이 항목은 확인하지 못했다고 보고합니다.

- [ ] **Step 7: 커밋합니다**

```powershell
git add -A
git commit -m "feat: HRTF audio engine with occlusion, beds and crates"
```

---

### Task 8: 내비메시와 봇

**Files:**
- Create: `src/nav.js`, `src/botBrain.js`, `src/bots.js`
- Modify: `src/main.js`, `src/player.js`(onHit 연결)
- Test: `tests/botBrain.test.js`

**Interfaces:**
- Consumes: `buildMap().navMeshes` (Task 4), `physics.createCharacter`, `physics.raycast` (Task 4), `createGun`, `Player.takeDamage` (Task 5), `audio.playAt` (Task 7), `chooseRespawnIndex` (Task 3), `round.addKill` (Task 2), 상수 (Task 1)
- Produces:
  - `buildNav(meshes: THREE.Mesh[]): Promise<Nav>` — `@recast-navigation/core`의 `init()` 뒤 `threeToSoloNavMesh(meshes, { cs: 0.2, ch: 0.2, walkableSlopeAngle: 45, walkableClimb: 0.3, walkableHeight: 1.8, walkableRadius: 0.4 })`. recast 설정 단위(복셀 수 또는 미터)는 설치된 0.44.0 타입 정의를 확인해서 맞춥니다. `Nav.path(from: number[], to: number[]): number[][]`(실패 시 `[]`), `Nav.closest(p: number[]): number[]`
  - `decideBot(state: BotState, p: Perception, weapon: WeaponDef, dt: number): { state: BotState, moveTo: number[] | null, fire: boolean }`
    - `BotState = { mode: 'patrol' | 'chase' | 'attack', patrolIndex: number, lastKnown: number[] | null, lostTimer: number, reactTimer: number }`
    - `Perception = { botPos: number[], seesPlayer: boolean, playerPos: number[], distance: number, heardShotAt: number[] | null, patrol: number[][], isSniper: boolean }`
    - 규칙: 플레이어가 보이면 `lastKnown`을 갱신하고 `reactTimer += dt`, 안 보이면 `reactTimer = 0`. 보이고 `distance <= weapon.rangeM`이면 `attack`이 되고, `reactTimer >= BOT_REACTION_S`일 때 `fire: true`. 보이지만 사거리 밖이면 `chase`(저격수는 제자리에서 `attack` 대기, `moveTo: null`). 안 보이게 되면 `chase`로 `lastKnown`까지 가고, `BOT_LOST_SIGHT_S` 동안 못 찾으면 `patrol`로 돌아갑니다. `heardShotAt`이 있으면 `lastKnown`을 그 위치로 두고 `chase`합니다(저격수는 제외). `patrol`은 `patrol[patrolIndex]`로 가고, 1 m 안에 도착하면 다음 인덱스로 넘어갑니다.
  - `createBots({ scene, physics, nav, audio, map, player, round, camera }): Bots` — `update(dt)`, `handleHit(collider, damage): boolean`, `get positions: number[][]`(문 열림 판정용)
    - 봇 외형: 캡슐, 무기별 단색(rifle 빨강, shotgun 주황, sniper 보라, knife 검정).
    - 이동: `Nav.path`를 0.5초마다 다시 구해 다음 지점 쪽으로 `Character.move`. 속도는 걷기 3.5 m/s, 추적 5 m/s, 칼 봇 추적 6.5 m/s.
    - 인지: 시야각 `BOT_FOV_DEG`, 봇 눈높이(1.6 m)→플레이어 가슴(1.2 m) 레이캐스트가 정적 충돌체와 닫힌 문에 막히지 않으면 `seesPlayer`. 플레이어가 쏘면 `SHOT_HEARING_M` 안의 봇에게 `heardShotAt`을 넘깁니다.
    - 사격: `fire`일 때 `createGun`의 `tryFire`가 성공하면 `weapon.fireSound`를 봇 위치에서 재생하고, `spreadDeg` 안에서 흔든 방향으로 레이캐스트해서 플레이어 충돌체에 맞으면 `player.takeDamage(damage, botPos)`. 산탄총은 거리 비례 감소(사거리 끝에서 30%). `afterFireSound`와 `reloadSound`도 재생합니다. 칼은 `distance <= 1.8`일 때만 휘두르고 `knife_swing`, 맞으면 `knife_hit`을 재생합니다.
    - 발소리: 이동 중 걷기 0.5초, 추적 0.33초 간격으로 발밑 재질에 맞는 `step_*`을 `playAt(id, () => bot.position)`으로 재생합니다.
    - 사망: `handleHit`로 체력(100)이 0이 되면 `round.addKill()`, `death`를 재생하고 숨깁니다. `BOT_RESPAWN_SECONDS` 뒤 `chooseRespawnIndex(map.respawnPoints, player.position, p => 화면 안이고 가리지 않음, MIN_RESPAWN_DIST_M)` 지점에서 `patrol` 상태로 다시 나타납니다.
    - 봇 난수(조준 흔들림 등)는 `createRng(map.seed)`를 씁니다.

- [ ] **Step 1: 실패하는 테스트를 씁니다**

```js
import { describe, it, expect } from 'vitest';
import { decideBot } from '../src/botBrain.js';
import { WEAPONS, BOT_REACTION_S, BOT_LOST_SIGHT_S } from '../src/config.js';

const s0 = () => ({ mode: 'patrol', patrolIndex: 0, lastKnown: null, lostTimer: 0, reactTimer: 0 });
const per = o => ({ botPos: [100, 0, 100], seesPlayer: false, playerPos: [10, 0, 0], distance: 10, heardShotAt: null, patrol: [[0, 0, 0], [5, 0, 0]], isSniper: false, ...o });

describe('decideBot', () => {
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
```

- [ ] **Step 2: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/botBrain.test.js`
Expected: FAIL

- [ ] **Step 3: `src/botBrain.js`를 구현합니다**

- [ ] **Step 4: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/botBrain.test.js`
Expected: PASS

- [ ] **Step 5: `nav.js`, `bots.js`를 구현하고 `main.js`, `player.js`에 연결합니다**

- [ ] **Step 6: Chrome에서 연습장으로 확인합니다**

Run: `npm run dev`, `map=practice`
Expected:
- 봇이 순찰 경로를 따라 계단을 오르내립니다.
- 봇이 다가올 때 등 뒤에서도 발소리가 들리고, 2층에 있는 봇의 발소리는 둔하게 들립니다.
- 플레이어가 쏘면 근처 봇이 그쪽으로 옵니다.
- 봇을 처치하면 처치 수가 오르고, 10초 뒤 화면 밖에서 다시 나타납니다.
- 화면 밖 봇에게 맞으면 결과의 기습 횟수가 오릅니다.

- [ ] **Step 7: 커밋합니다**

```powershell
git add -A
git commit -m "feat: navmesh bots with weapon-specific behaviour and sounds"
```

---

### Task 9: 본 맵 3개

**Files:**
- Create: `src/maps/urban.js`, `src/maps/warehouse.js`, `src/maps/forest.js`
- Modify: `src/maps/index.js`
- Test: `tests/maps.test.js`(추가), `tests/nav.test.js`

**Interfaces:**
- Consumes: 맵 데이터 형식, `validateMap`, `expandMap` (Task 4), `buildNav` (Task 8)
- Produces: `MAPS.urban`, `MAPS.warehouse`, `MAPS.forest`

세 맵 공통: 80×80 m, 봇 4명(rifle, shotgun, sniper, knife 각 1명), `crateCount: 6`, 후보 12개 이상, 부활 지점 6개 이상, 저격수 순찰 경로는 높은 곳 한 지점. 적이 시야 밖이나 벽 뒤, 다른 층에서 접근하도록 배치합니다.

| 맵 | seed | surface | 높이 요소 | ambience / bgm |
| --- | --- | --- | --- | --- |
| urban | 101 | concrete | 2~3층 건물 4채 이상, 계단, 창문 구멍, 옥상, 문 | amb_urban / bgm_urban |
| warehouse | 202 | metal | 외벽으로 둘러싼 실내, 중이층(메자닌), 철제 계단, 캣워크, 선반 줄, 문 | amb_warehouse / bgm_warehouse |
| forest | 303 | dirt | 언덕(계단식 박스나 경사 박스), 감시탑, 절벽, 2층 오두막 | amb_forest / bgm_forest |

창고 맵의 외벽과 천장은 정적 충돌체이므로 바깥 하늘 조명 대신 실내 조명 색으로 구분합니다(빌더 재량).

- [ ] **Step 1: 실패하는 테스트를 씁니다**

`tests/maps.test.js`에 추가:

```js
describe('본 맵', () => {
  for (const id of ['urban', 'warehouse', 'forest']) {
    it(`${id}는 오류가 없다`, () => {
      expect(validateMap(MAPS[id], { isPractice: false })).toEqual([]);
    });
  }
  it('세 맵의 크기와 상자 수가 같다', () => {
    const v = ['urban', 'warehouse', 'forest'].map(id => [MAPS[id].size.join('x'), MAPS[id].crateCount]);
    expect(new Set(v.map(String)).size).toBe(1);
  });
});
```

`tests/nav.test.js`:

```js
import { describe, it, expect } from 'vitest';
import * as THREE from 'three';
import { MAPS } from '../src/maps/index.js';
import { expandMap } from '../src/maps/expand.js';
import { buildNav } from '../src/nav.js';

const meshesOf = map => expandMap(map).map(b => {
  const m = new THREE.Mesh(new THREE.BoxGeometry(...b.size));
  m.position.set(...b.pos); m.rotation.y = b.rotY; m.updateMatrixWorld(); return m;
});

describe('내비메시', () => {
  for (const id of ['practice', 'urban', 'warehouse', 'forest']) {
    it(`${id}: 플레이어 출발점에서 모든 봇 순찰 지점과 상자 후보로 갈 수 있다`, async () => {
      const map = MAPS[id];
      const nav = await buildNav(meshesOf(map));
      const targets = [...map.bots.flatMap(b => [b.spawn, ...b.patrol]), ...map.crateCandidates];
      for (const t of targets) expect(nav.path(map.playerSpawn, t).length, String(t)).toBeGreaterThan(0);
    });
  }
});
```

- [ ] **Step 2: 테스트가 실패하는지 확인합니다**

Run: `npx vitest run tests/maps.test.js tests/nav.test.js`
Expected: FAIL (urban, warehouse, forest 없음)

- [ ] **Step 3: 세 맵 데이터를 씁니다**

- [ ] **Step 4: 테스트가 통과하는지 확인합니다**

Run: `npx vitest run tests/maps.test.js tests/nav.test.js`
Expected: PASS. 경로가 없는 지점이 나오면 그 좌표를 고치거나 계단을 추가합니다.

- [ ] **Step 5: Chrome에서 세 맵을 각각 한 라운드씩 플레이합니다**

Expected: 각 맵에서 계단과 높은 곳에 오를 수 있고, 봇이 층을 오가며, 맵별 바닥 발소리와 환경음, 배경음악이 맞습니다. 라운드가 끝나면 CSV 파일이 Chrome 다운로드 폴더에 저장되고, Excel로 열었을 때 한글 조건 이름이 깨지지 않습니다.

- [ ] **Step 6: 커밋합니다**

```powershell
git add -A
git commit -m "feat: urban, warehouse and forest maps with multi-level layouts"
```

---

### Task 10: 난이도 조정, 오프라인 빌드, 최종 점검

**Files:**
- Modify: `src/config.js`(수치 조정), 필요하면 맵 데이터
- Create: `README.md`(실험 진행자용 실행 방법)

- [ ] **Step 1: 난이도를 조정합니다**

세 맵을 소리 켬 상태로 3분씩 플레이하면서 기습 피해가 라운드당 여러 번(목표 3회 이상) 생기도록 `WEAPONS`, 봇 속도, `BOT_REACTION_S`를 조정합니다. 조정한 값과 이유를 커밋 메시지에 적습니다.

- [ ] **Step 2: 전체 테스트를 돌립니다**

Run: `npm test`
Expected: 전부 PASS

- [ ] **Step 3: 오프라인 빌드를 확인합니다**

Run: `npm run build` 후 `npx vite preview`
Expected: 네트워크를 끈 상태에서도 Chrome에서 시작 화면, 연습장, 본 맵 하나가 정상 동작하고 콘솔 오류가 없습니다. 개발자 도구 Network 탭에 외부 도메인 요청이 없습니다.

- [ ] **Step 4: README를 씁니다**

내용: 실행 방법(`dist` 폴더를 정적 서버로 여는 명령), 라운드 진행 순서(연습 → 본 라운드 3개), 시작 화면 입력 항목, CSV 저장 위치(Chrome 다운로드 폴더)와 열 설명, 7.1 출력을 쓰려면 Windows 소리 설정에서 출력 장치를 7.1로 바꿔야 한다는 안내, "소리 끔" 조건은 게임 음량이 아니라 참가자가 소리를 듣지 못하게 하는 방식으로 만든다는 주의 사항.

- [ ] **Step 5: 커밋합니다**

```powershell
git add -A
git commit -m "chore: tune difficulty, offline build check and operator README"
```
