# Halo 오버레이 분석기 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 시스템 출력 오디오(WASAPI 루프백)를 분석해 소리의 종류, 방위, 앞뒤 신뢰도, 거리 구간을 추정하고, 표시부 계획과 같은 UDP 메시지로 보내는 분석기를 만듭니다.

**Architecture:** `halo_analyzer` 패키지를 입력(audio), 창 자르기(Windower), 판정(gate), 서라운드 방향(surround), 크기 기반 거리(distance), 추적(tracker), 어댑터 불러오기(adapters), 조립(pipeline), 송신(protocol)으로 나눕니다. 모델은 `Saved/ModelEval/adapters`의 어댑터를 그대로 불러 쓰고, 클래스별 확률(`scores`)만 어댑터 결과에 추가합니다. 마지막에 비교 도구로 언리얼 정답과 모델 출력을 짝지어 정확도와 지연을 잽니다.

**Tech Stack:** Python 3.12, numpy, scipy(`signal.resample_poly`, `signal.stft`), soundfile, PyAudioWPatch(루프백), pytest. 실제 모델 실행에만 torch와 각 모델 저장소가 필요합니다.

**Spec:** `Docs/superpowers/specs/2026-11-25-external-sound-halo-overlay-design.md`

**선행 계획:** `Docs/superpowers/plans/2026-11-25-halo-overlay-display.md`. 이 계획은 그 계획의 메시지 형식, `record.py`, 언리얼 송신을 씁니다. Task 1~8은 표시부 계획과 상관없이 진행할 수 있고, Task 10과 11은 표시부 계획 Task 6, 7이 끝나야 합니다.

## Global Constraints

- 저장소 루트 `C:\Users\Home\Documents\Github\SoundCompass`, 브랜치 `PSH`, 로컬 커밋만 합니다.
- 메시지 형식은 설계 문서 6절 그대로입니다. `"v": 1`, `"src": "model"`, `mode`는 `"stereo"` 또는 `"surround"`입니다. 이벤트 키는 `id, cls, conf, az, fb, dist_m, dist_bin, dist_src, elev_m, level`이고, 모르는 값은 JSON `null`입니다.
- 방위 규약: 0° 정면, +90° 오른쪽, ±180° 뒤(`Saved/ModelEval`과 같음).
- 클래스는 모델 평가 클래스 14개(`Alarm, Dog, Door, Explosion, Fire, Footstep, GlassBreak, Gunshot, Impact, Item, Reload, Throwable, Vehicle, Voice`)만 냅니다. `Other`는 내지 않습니다.
- 분석 창 1초, 홉 100 ms입니다. 기본 포트 47800, 여러 포트로 같은 메시지를 보낼 수 있습니다.
- 스테레오 어댑터의 `fb`는 설정 파일의 어댑터별 고정값이고 기본은 0입니다. 서라운드 경로의 `fb`는 1입니다. `fb < 1`이면 `az`를 앞쪽 반원(-90~90)으로 접어 보냅니다.
- 거리 구간 경계: 기준 크기보다 6 dB 이내 near, 6~18 dB mid, 그보다 작으면 far. 기준은 세션 안 최근 180초의 onset 크기 95번째 백분위입니다. 관측 3번 미만이고 기본표에 값이 없으면 `null`입니다.
- `Saved/`는 gitignore 대상이라 `Saved/ModelEval` 수정은 커밋되지 않습니다. 해당 Task는 수정 내용을 이 계획에 남기는 것으로 대신합니다.
- 테스트는 `HaloOverlay/analyzer/.venv`(py -3.12)의 pytest로 돌립니다. torch가 필요한 테스트는 만들지 않습니다.

## Review Focus

1. 완전한 무음(디지털 0)이나 아주 작은 배경음: 이벤트가 나오지 않고 NaN이나 0 나누기가 생기지 않습니다. (Task 8 `test_silence_emits_nothing`)
2. 실행 중 출력 장치 변경으로 채널 수가 2에서 8로 바뀜: 창 버퍼를 비우고 다음 창부터 `mode`가 `"surround"`로 바뀝니다. (Task 2 `test_channel_change_resets`, Task 8 `test_mode_follows_channels`)
3. 44.1 kHz, 96 kHz 장치: 서라운드 방위와 모델 입력이 48 kHz와 같은 결과를 냅니다. (Task 3 `test_sample_rate_independent`)
4. 모노(1채널) 장치: 죽지 않고 `az = 0`, `fb = 0`으로 보냅니다(오버레이는 앞뒤 두 곳에 흐리게 그립니다). (Task 8 `test_mono_device`)
5. `scores`가 없는 예전 어댑터: 이긴 클래스 하나를 확률 1로 보고 동작합니다. (Task 7 `test_missing_scores_fallback`)

---

### Task 1: 패키지 뼈대와 메시지 송신

**Files:**
- Create: `HaloOverlay/analyzer/requirements.txt` (`numpy`, `scipy`, `soundfile`, `PyAudioWPatch`, `pytest`, 버전은 설치 시점 최신으로 고정)
- Create: `HaloOverlay/analyzer/halo_analyzer/__init__.py`, `HaloOverlay/analyzer/halo_analyzer/protocol.py`
- Create: `HaloOverlay/analyzer/tests/test_protocol.py`

**Interfaces:**
- Produces:
  ```python
  @dataclass
  class Event:
      id: int; cls: str; conf: float; az: float; fb: float; level: float
      dist_m: float | None = None; dist_bin: str | None = None; dist_src: str | None = None; elev_m: float | None = None
  def encode(events: list[Event], mode: str, t: float) -> bytes   # UTF-8 JSON, src="model", v=1
  class Sender:
      def __init__(self, ports: list[int]): ...
      def send(self, payload: bytes) -> None                       # 포트마다 127.0.0.1로 sendto, 실패는 무시
  ```

- [ ] **Step 1: venv 만들기** — Run: `py -3.12 -m venv HaloOverlay/analyzer/.venv` 다음 `HaloOverlay/analyzer/.venv/Scripts/python -m pip install -r HaloOverlay/analyzer/requirements.txt`. Expected: 설치 성공.

- [ ] **Step 2: 실패하는 테스트 작성**

  - `test_encode_matches_spec`: `Event(17, "Gunshot", 0.82, 35.0, 0.2, 0.7, dist_bin="mid", dist_src="level")`를 `encode(..., "stereo", 1234.5)`하면 `json.loads` 결과가 설계 문서 6절 예시와 같습니다(`dist_m`과 `elev_m`은 `None`, `v == 1`, `src == "model"`).
  - `test_encode_rejects_unknown_class`: `cls = "Other"`면 `ValueError`.
  - `test_sender_reaches_all_ports`: 47861, 47862에 소켓을 열고 `Sender([47861, 47862]).send(b"x")`하면 둘 다 `b"x"`를 받습니다.

- [ ] **Step 3: 실패 확인** — Run: `HaloOverlay/analyzer/.venv/Scripts/python -m pytest HaloOverlay/analyzer/tests -v`. Expected: ImportError.

- [ ] **Step 4: `protocol.py` 구현**

- [ ] **Step 5: 통과 확인** — Step 3 명령. Expected: 3개 통과.

- [ ] **Step 6: 커밋** — `git add HaloOverlay/analyzer` 후 `git commit -m "Analyzer: package skeleton and message encoder"`

### Task 2: 오디오 입력과 창 자르기

**Files:**
- Create: `HaloOverlay/analyzer/halo_analyzer/audio.py`
- Create: `HaloOverlay/analyzer/tests/test_audio.py`

**Interfaces:**
- Produces:
  ```python
  class FileSource:      # WAV를 블록으로 흘림
      def __init__(self, path: Path, block_s: float = 0.01, realtime: bool = True): ...
      def __iter__(self) -> Iterator[tuple[np.ndarray, int]]   # (samples, channels) float32, sr
  class LoopbackSource:  # 기본 출력 장치의 WASAPI 루프백, PyAudioWPatch
      def __init__(self, block_s: float = 0.01): ...
      def __iter__(self) -> Iterator[tuple[np.ndarray, int]]   # 1초마다 기본 장치를 확인하고 바뀌면 다시 엽니다
  class Windower:
      def __init__(self, window_s: float = 1.0, hop_s: float = 0.1): ...
      def push(self, block: np.ndarray, sr: int) -> list[np.ndarray]   # 완성된 창들, 각 (round(window_s*sr), ch)
  ```
  `Windower`는 `sr`이나 채널 수가 바뀌면 버퍼를 비우고 새로 채웁니다.

- [ ] **Step 1: 실패하는 테스트 작성**

  - `test_file_source_blocks`: 48 kHz 2채널 0.5초 WAV(임시 파일)를 `realtime=False`로 읽으면 블록을 이어 붙인 길이가 24000이고 값이 원본과 같습니다.
  - `test_windower_hops`: 48 kHz 2채널 블록 480개짜리를 250번(2.5초) 넣으면 창 16개(1.0, 1.1, ..., 2.5초에 끝나는 창), 각 `(48000, 2)`, 연속한 두 창은 4800샘플 어긋나 겹칩니다.
  - `test_channel_change_resets`: 2채널 0.9초를 넣은 뒤 8채널 블록을 넣으면, 8채널로 1초가 찰 때까지 창이 나오지 않고 이후 창은 `(48000, 8)`입니다.

- [ ] **Step 2: 실패 확인** — Task 1 Step 3 명령. Expected: ImportError.

- [ ] **Step 3: 구현.** `LoopbackSource`는 `pyaudiowpatch`의 `get_default_wasapi_loopback()`으로 장치를 찾고, 장치의 기본 샘플레이트와 채널 수로 엽니다. 테스트가 없으므로 Step 5에서 손으로 확인합니다.

- [ ] **Step 4: 통과 확인** — Expected: 3개 통과.

- [ ] **Step 5: 루프백 손 확인**

  Run: `HaloOverlay/analyzer/.venv/Scripts/python -c "from halo_analyzer.audio import LoopbackSource; import itertools,numpy as np; [print(b.shape, sr, float(np.abs(b).max())) for b, sr in itertools.islice(LoopbackSource(), 0, 300, 50)]"` (작업 폴더 `HaloOverlay/analyzer`, 아무 소리나 재생 중일 때)
  Expected: 채널 수와 샘플레이트가 Windows 사운드 설정과 같고, 최대값이 0보다 큽니다.

- [ ] **Step 6: 커밋** — `git commit -m "Analyzer: file and loopback input, windowing"`

### Task 3: 서라운드 방위

**Files:**
- Create: `HaloOverlay/analyzer/halo_analyzer/surround.py`
- Create: `HaloOverlay/analyzer/tests/test_surround.py`
- Create: `HaloOverlay/analyzer/tests/synth.py` (테스트용 패너, Task 9도 씀)

**Interfaces:**
- Produces:
  ```python
  LAYOUT_71: dict[int, float | None]   # 채널 번호 → 방위. 0 FL -30, 1 FR 30, 2 FC 0, 3 LFE None, 4 BL -150, 5 BR 150, 6 SL -90, 7 SR 90
  LAYOUT_51: dict[int, float | None]   # 0 FL -30, 1 FR 30, 2 FC 0, 3 LFE None, 4 -110, 5 110
  def layout_for(channels: int) -> dict[int, float | None] | None   # 6, 8만 지원, 그 외 None
  def surround_peaks(window: np.ndarray, sr: int, n_bands: int = 24, max_peaks: int = 3) -> list[tuple[float, float]]
      # (방위, 에너지 비율) 강한 순. 비율 합은 1 이하. 무음이면 []
  ```
  `synth.py`: `pan_71(mono: np.ndarray, az: float, layout: dict) -> np.ndarray` — 방위를 감싸는 인접 스피커 두 개에 일정 전력(sin/cos) 패닝, 다른 채널은 0.

- [ ] **Step 1: 실패하는 테스트 작성**

  - `test_single_source_angles`: 백색잡음 1초를 `pan_71`로 0, 45, 90, 120, -135, 180에 놓으면 `surround_peaks(...)[0][0]`이 각 방위의 ±10° 이내입니다.
  - `test_51_rear`: 6채널 `LAYOUT_51`로 110에 놓으면 ±10° 이내입니다.
  - `test_two_sources_two_peaks`: 1 kHz 이하 대역 잡음을 -90, 4 kHz 이상 대역 잡음을 60에 같은 에너지로 놓으면 봉우리 두 개가 각각 ±15° 이내입니다.
  - `test_silence`: 0 배열이면 `[]`.
  - `test_sample_rate_independent`: 44100, 96000 Hz에서 `test_single_source_angles`의 90° 경우가 ±10° 이내입니다.
  - `test_lfe_ignored`: LFE 채널에만 신호가 있으면 `[]`.

- [ ] **Step 2: 실패 확인** — Expected: ImportError.

- [ ] **Step 3: `surround_peaks` 구현**

  ```text
  1. scipy.signal.stft(창, nperseg=1024 상당(약 21 ms), 채널별)로 |X|^2를 구합니다.
  2. 100 Hz~16 kHz를 로그 간격 n_bands개로 묶어, 시간 프레임 × 대역마다 채널 에너지 e_c를 구합니다.
  3. 각 칸의 벡터 v = Σ_c e_c * (cos az_c, sin az_c) (LFE 제외), 방위 = atan2(v_y, v_x), 가중치 = Σ_c e_c.
  4. 5° 간격 72칸 원형 히스토그램에 가중치를 더하고, 이웃 2칸으로 원형 평활합니다.
  5. 전체 가중치의 10% 이상인 극대값을 강한 순으로 max_peaks개 고르고, 각 봉우리 방위는 ±10° 안 칸들의 가중 평균(원형)입니다.
  6. 전체 가중치가 1e-10 이하이면 []를 반환합니다.
  ```

- [ ] **Step 4: 통과 확인** — Expected: 6개 통과. 허용 오차를 넘으면 오차 값과 함께 원인을 보고합니다(허용 오차를 늘리지 않습니다).

- [ ] **Step 5: 커밋** — `git commit -m "Analyzer: surround azimuth from channel energy vectors"`

### Task 4: 소리 있음 판정

**Files:**
- Create: `HaloOverlay/analyzer/halo_analyzer/gate.py`
- Create: `HaloOverlay/analyzer/tests/test_gate.py`

**Interfaces:**
- Produces:
  ```python
  class OnsetGate:
      def __init__(self, rise_db: float = 9.0, background_s: float = 3.0, hop_s: float = 0.1, n_bands: int = 24): ...
      def update(self, window: np.ndarray, sr: int) -> bool   # 창의 마지막 hop_s 구간이 onset이면 True
  class ClassGate:
      def __init__(self, thresholds: dict[str, float] | None = None, default: float = 0.3): ...
      def passes(self, cls: str, score: float) -> bool
  ```
  `OnsetGate`의 배경 수준은 최근 `background_s` 동안 홉별 대역 에너지(dB)의 중앙값입니다. 마지막 홉에서 어느 대역이든 배경보다 `rise_db` 이상 높으면 onset입니다. 첫 `background_s` 동안은 배경이 다 차지 않아도 지금까지의 중앙값을 씁니다. 9 dB와 0.3은 시작값이고, Task 9에서 `Saved/ModelEval` 장면으로 다시 맞춥니다.

- [ ] **Step 1: 실패하는 테스트 작성**

  - `test_steady_noise_no_onset`: -30 dBFS 백색잡음을 5초 흘리면 1초 이후 `update`가 모두 False.
  - `test_burst_is_onset`: 같은 잡음 위에 3초 지점에서 0.2초 길이 +20 dB 버스트를 더하면 3.0~3.2초에 끝나는 창 중 하나 이상이 True.
  - `test_music_then_click`: 440 Hz와 660 Hz 사인 합(음악 대용) 5초 위에 3초 지점 짧은 광대역 클릭을 더하면, 1초 이후로는 클릭이 든 홉에서만 True.
  - `test_silence`: 0 배열 5초는 모두 False이고 경고나 예외가 없습니다.
  - `test_class_gate`: `ClassGate({"Gunshot": 0.5})`에서 `passes("Gunshot", 0.4)`는 False, `passes("Door", 0.31)`은 True.

- [ ] **Step 2: 실패 확인** — Expected: ImportError.
- [ ] **Step 3: 구현**
- [ ] **Step 4: 통과 확인** — Expected: 5개 통과.
- [ ] **Step 5: 커밋** — `git commit -m "Analyzer: onset and class gates"`

### Task 5: 크기 기반 거리 구간

**Files:**
- Create: `HaloOverlay/analyzer/halo_analyzer/distance.py`
- Create: `HaloOverlay/analyzer/tests/test_distance.py`

**Interfaces:**
- Produces:
  ```python
  def peak_db(window: np.ndarray, sr: int, frame_s: float = 0.05) -> float   # 50 ms RMS 최대값(dBFS), 무음이면 -120
  class LevelDistance:
      def __init__(self, defaults: dict[str, float] | None = None, near_db: float = 6.0, mid_db: float = 18.0,
                   window_s: float = 180.0, min_obs: int = 3, percentile: float = 95.0): ...
      def observe(self, cls: str, level_db: float, t: float) -> None
      def estimate(self, cls: str, level_db: float, t: float) -> str | None   # "near" | "mid" | "far" | None
  ```

- [ ] **Step 1: 실패하는 테스트 작성**

  - `test_needs_three_observations`: 기본표 없이 관측 2번이면 `None`, 3번째부터 값이 나옵니다.
  - `test_default_reference_used`: `defaults={"Gunshot": -10.0}`이면 관측 0번이어도 `estimate("Gunshot", -12.0, 0) == "near"`, `-20.0`은 `"mid"`, `-40.0`은 `"far"`.
  - `test_bins_from_reference`: Door를 -20 dB로 20번 관측한 뒤 -22 → near, -30 → mid, -45 → far, -15(기준보다 큼) → near.
  - `test_reference_forgets_old`: t=0에 -10 dB 20번, t=200에 -30 dB 20번 관측하면 t=200에서 `-32`는 near입니다(180초 지난 관측은 버림).
  - `test_volume_shift_invariant`: 모든 관측과 질의 값에 -12 dB를 더해도 같은 구간이 나옵니다.
  - `test_peak_db_silence`: 0 배열이면 -120.

- [ ] **Step 2: 실패 확인** — Expected: ImportError.
- [ ] **Step 3: 구현**
- [ ] **Step 4: 통과 확인** — Expected: 6개 통과.
- [ ] **Step 5: 커밋** — `git commit -m "Analyzer: level-based distance bins"`

### Task 6: 이벤트 추적

**Files:**
- Create: `HaloOverlay/analyzer/halo_analyzer/tracker.py`
- Create: `HaloOverlay/analyzer/tests/test_tracker.py`

**Interfaces:**
- Produces:
  ```python
  @dataclass
  class Detection: cls: str; az: float
  class Tracker:
      def __init__(self, max_gap_deg: float = 20.0): ...
      def assign(self, detections: list[Detection]) -> list[tuple[int, bool]]   # (id, 새 트랙 여부), 입력 순서대로
  ```
  직전 `assign` 호출의 트랙과만 비교합니다. 같은 클래스, 감은 방위 차 `max_gap_deg` 이내 중 가장 가까운 트랙을 하나씩만 이어받습니다. id는 1부터 증가합니다.

- [ ] **Step 1: 실패하는 테스트 작성**

  - `test_continuation`: `[Gunshot 30]` 다음 `[Gunshot 40]`이면 같은 id, 두 번째는 새 트랙 아님.
  - `test_wrap`: `[Door 175]` 다음 `[Door -175]`이면 같은 id.
  - `test_gap_breaks`: `[Door 0]`, `[]`, `[Door 0]`이면 마지막은 새 id.
  - `test_class_differs`: `[Door 0]` 다음 `[Gunshot 0]`이면 새 id.
  - `test_two_same_class`: `[Footstep -60, Footstep 60]` 다음 `[Footstep 55, Footstep -65]`이면 id가 방위를 따라 이어집니다.

- [ ] **Step 2~4: 실패 확인, 구현, 통과 확인** — Expected: 5개 통과.
- [ ] **Step 5: 커밋** — `git commit -m "Analyzer: event tracker"`

### Task 7: 어댑터 불러오기와 `scores` 키

**Files:**
- Create: `HaloOverlay/analyzer/halo_analyzer/adapters.py`
- Create: `HaloOverlay/analyzer/tests/test_adapters.py`, `HaloOverlay/analyzer/tests/fake_modeleval/adapters/__init__.py`, `.../adapters/fake_classifier.py`, `.../adapters/fake_old_classifier.py`, `.../adapters/fake_spatial.py`
- Modify (커밋 안 됨): `Saved/ModelEval/adapters/_common.py`, `efficientat_mn10.py`, `efficientat_mn04.py`, `efficientat_mn40.py`, `panns_cnn14.py`, `panns_mobilenetv1.py`, `ast.py`, `eat.py`, `yamnet.py`, `clap_general.py`, `clap_htsat_unfused.py`

**Interfaces:**
- Consumes: ModelEval 어댑터 규약 `Adapter(device).predict(audio: np.ndarray (samples, ch), sr: int) -> dict | list[dict]`, 키 `class`, `azimuth`, `distance`(값이 없으면 None).
- Produces:
  ```python
  def load_adapter(name: str, modeleval_dir: Path, device: str): ...   # modeleval_dir을 sys.path에 넣고 adapters.<name>.Adapter(device)
  def class_scores(result: dict) -> dict[str, float]                  # result["scores"]가 없으면 {result["class"]: 1.0}, class도 None이면 {}
  def as_list(result: dict | list[dict]) -> list[dict]
  ```
  `_common.py`에 `class_scores(probs, index) -> dict[str, float]`(클래스마다 `float(probs[index[c]].max())`)를 추가하고, 분류 어댑터마다 반환 dict에 `"scores": class_scores(probs, self.index)`를 넣습니다. CLAP 어댑터는 클래스별 최대 유사도를 softmax한 값을 넣습니다.

- [ ] **Step 1: 실패하는 테스트 작성**

  - `test_load_fake`: `load_adapter("fake_classifier", tests/fake_modeleval, "cpu").predict(np.zeros((48000, 1)), 48000)`이 `{"class": "Door", "azimuth": None, "distance": None, "scores": {...}}`를 돌려줍니다.
  - `test_missing_scores_fallback`: `fake_old_classifier` 결과로 `class_scores`를 부르면 `{"Gunshot": 1.0}`.
  - `test_as_list`: dict 하나는 길이 1 리스트, 리스트는 그대로.

- [ ] **Step 2~4: 실패 확인, 구현, 통과 확인** — Expected: 3개 통과.

- [ ] **Step 5: ModelEval 회귀 확인**

  `Saved/ModelEval` 수정 뒤 Run: `Saved/ModelEval/.venv/Scripts/python Saved/ModelEval/selftest.py`. Expected: 수정 전과 같은 결과. torch 어댑터는 노트북 작업 폴더(`~/claude-jobs/20261125-audio-model-eval/`)에서 같은 수정을 하고 `run.sh` 순서대로 `evaluate.py efficientat_mn10 --device cuda`를 돌려 `results/` 수치가 수정 전과 같은지 확인합니다(laptop-compute 스킬 사용).

- [ ] **Step 6: 커밋** — `git add HaloOverlay/analyzer` 후 `git commit -m "Analyzer: adapter loading with class scores"`. ModelEval 수정 내용은 커밋 메시지 본문에 한 줄로 적습니다.

### Task 8: 파이프라인과 실행 명령

**Files:**
- Create: `HaloOverlay/analyzer/halo_analyzer/pipeline.py`, `HaloOverlay/analyzer/halo_analyzer/__main__.py`
- Create: `HaloOverlay/analyzer/analyzer.json` (기본 설정)
- Create: `HaloOverlay/analyzer/tests/test_pipeline.py`

**Interfaces:**
- Consumes: Task 1~7의 모든 공개 이름.
- Produces:
  ```python
  @dataclass
  class AnalyzerConfig:
      fb: dict[str, float] = field(default_factory=dict)            # 공간 어댑터 이름 → fb, 없으면 0
      class_thresholds: dict[str, float] = field(default_factory=dict)
      default_threshold: float = 0.3
      max_events: int = 3
      reference_db: dict[str, float] = field(default_factory=dict)  # LevelDistance 기본표
  class Analyzer:
      def __init__(self, classifier, spatial, spatial_name: str | None, config: AnalyzerConfig): ...
      def process(self, window: np.ndarray, sr: int, t: float) -> tuple[str, list[Event]]   # (mode, events)
  ```
  `analyzer.json` 키는 `AnalyzerConfig` 필드와 같습니다.
  실행: `python -m halo_analyzer --input loopback|<wav> --classifier efficientat_mn10 --spatial gcc_phat|none --modeleval <경로, 기본 ../../Saved/ModelEval> --device auto|cuda|cpu --ports 47800[,47801] --config analyzer.json`. `auto`는 torch가 있고 CUDA가 되면 `cuda`, 아니면 `cpu`입니다.

  `process` 규칙(설계 문서 7절):
  ```text
  1. 채널 수 >= 6이고 layout_for가 있으면 mode="surround", 아니면 "stereo". 1채널은 stereo로 보고 공간 어댑터를 부르지 않습니다(az=0, fb=0).
  2. OnsetGate.update가 False이고 직전 홉에 이벤트가 없으면 빈 목록을 반환합니다(분류기를 부르지 않음).
  3. 모노 다운믹스로 분류기를 부르고 class_scores에서 ClassGate를 통과한 클래스를 확률 순으로 max_events개 고릅니다.
  4. 방향: surround는 surround_peaks의 k번째 봉우리를 k번째 클래스에(봉우리가 모자라면 가장 강한 봉우리), fb=1.
     stereo는 공간 어댑터 결과 중 class가 같은 것의 azimuth, 없으면 class 없는 결과를 강한 순(리스트 순서)으로 배정, fb=config.fb.get(spatial_name, 0).
     azimuth가 None이면 그 클래스는 버립니다. fb < 1이면 |az| > 90일 때 az = sign(az)*180 - az로 접습니다.
  5. Tracker.assign으로 id를 붙입니다. 새 트랙이면 peak_db로 LevelDistance.observe와 estimate를 한 번 하고, 그 구간을 트랙이 이어지는 동안 유지합니다.
     공간 어댑터가 distance를 내면 dist_m=distance, dist_src="model"이고 크기 추정은 하지 않습니다.
  6. level = clip((창 RMS dBFS + 60) / 60, 0, 1), conf = 그 클래스 점수.
  ```

- [ ] **Step 1: 실패하는 테스트 작성** (가짜 어댑터 사용)

  - `test_silence_emits_nothing`: 0 배열 창 30개는 모두 빈 목록이고 분류기 호출 수가 0입니다.
  - `test_surround_event`: `pan_71`로 120°에 둔 버스트 창이면 `mode == "surround"`, 이벤트 1개, `cls == "Door"`(가짜 분류기), `|az - 120| <= 10`, `fb == 1`.
  - `test_stereo_folds_back`: 가짜 공간 어댑터가 `azimuth=150`을 내고 `fb` 설정이 없으면 `az == 30`, `fb == 0`.
  - `test_stereo_trusted_fb`: `fb={"fake_spatial": 1.0}`이면 `az == 150`.
  - `test_track_keeps_distance_bin`: 같은 소리 창 5개에서 이벤트 id와 `dist_bin`이 바뀌지 않습니다.
  - `test_mode_follows_channels`: 2채널 창 다음 8채널 창이면 mode가 `"stereo"`에서 `"surround"`로 바뀝니다.
  - `test_mono_device`: 1채널 버스트 창이면 이벤트의 `az == 0`, `fb == 0`, 공간 어댑터 호출 수 0.
  - `test_level_clipped`: ±1.0 사각파 창이면 `level == 1.0`.
  - `test_cli_file_input`: `python -m halo_analyzer --input <8채널 버스트 WAV> --classifier fake_classifier --spatial none --modeleval tests/fake_modeleval --ports 47871`을 subprocess로 돌리고 47871에서 받으면 `"mode": "surround"` 메시지가 하나 이상 옵니다. WAV 끝에서 프로세스가 종료 코드 0으로 끝납니다.

- [ ] **Step 2~4: 실패 확인, 구현, 통과 확인** — Expected: 9개 통과, 앞 Task 테스트도 모두 통과.
- [ ] **Step 5: 커밋** — `git commit -m "Analyzer: pipeline and command line"`

### Task 9: 7.1 장면 생성과 임계값 맞추기

**Files:**
- Modify (커밋 안 됨): `Saved/ModelEval/gen_scenes.py` (`--layout binaural|7.1`, 기본 binaural)
- Create (커밋 안 됨): `Saved/ModelEval/adapters/halo_surround.py`, `Saved/ModelEval/adapters/halo_pipeline.py`
- Modify: `HaloOverlay/analyzer/analyzer.json` (맞춘 임계값과 기준 크기 기록)

**Interfaces:**
- Consumes: `halo_analyzer.surround.surround_peaks`, `tests/synth.py`의 패닝 방식, `halo_analyzer.pipeline.Analyzer`.
- Produces:
  - `gen_scenes.py --layout 7.1 --out data71`: `spatialize` 대신 거리 감쇠(바이노럴 경로와 같은 거리 이득)와 `pan_71` 방식 패닝으로 8채널 장면을 씁니다. 배경은 앰비언스·음악을 8채널에 고르게 펴고, `labels.csv` 형식은 같습니다. `room`, `occluded` 음향 조건은 바이노럴 경로와 같은 잔향과 필터를 채널마다 적용합니다.
  - `halo_surround.py`: `Adapter.predict`가 `surround_peaks` 결과를 `[{"class": None, "azimuth": az, "distance": None}, ...]`로 돌려줍니다.
  - `halo_pipeline.py`: 분석기 전체(가짜가 아닌 실제 분류기 + 지정 공간 어댑터)를 평가 하네스 규약으로 감쌉니다. `dist_bin`을 거리 구간 가운데 값(2.5/10/30 m)으로 바꿔 `distance`에 넣습니다.

- [ ] **Step 1: 7.1 장면 생성** — Run: `Saved/ModelEval/.venv/Scripts/python Saved/ModelEval/gen_scenes.py --n 600 --seed 1234 --layout 7.1 --out Saved/ModelEval/data71`. Expected: `data71/labels.csv` 904이벤트(바이노럴 세트와 같은 시드라 같은 이벤트).

- [ ] **Step 2: 서라운드 경로 평가** — Run: `Saved/ModelEval/.venv/Scripts/python Saved/ModelEval/evaluate.py halo_surround --data Saved/ModelEval/data71` (평가 하네스에 `--data` 옵션이 없으면 추가합니다). Expected: `results/halo_surround_cpu/summary.md` 생성. `az_mae`와 `front_back_rate`를 기록하고, 단일 음원 dry 조건 `az_mae`가 10°를 넘으면 원인을 보고합니다.

- [ ] **Step 3: 임계값과 거리 경계 맞추기**

  노트북에서 `halo_pipeline`을 실제 분류기(EfficientAT mn10)로 바이노럴 세트와 7.1 세트에 돌립니다. `OnsetGate.rise_db`(6, 9, 12), `default_threshold`(0.2, 0.3, 0.4)를 바꿔 `event_recall`과 `pred_precision`이 함께 가장 좋은 조합을 고르고, `dist_bin_acc`로 `near_db`, `mid_db`를 확인합니다. 고른 값과 근거 수치를 `analyzer.json`과 커밋 메시지에 남깁니다.

- [ ] **Step 4: 커밋** — `git add HaloOverlay/analyzer/analyzer.json` 후 `git commit -m "Analyzer: tune gates and distance bins on ModelEval scenes"`

### Task 10: 정답과 모델 비교 도구

**Files:**
- Create: `HaloOverlay/tools/compare_streams.py`
- Create: `HaloOverlay/tools/test_compare.py`

**Interfaces:**
- Consumes: 표시부 계획 Task 6의 `record.py` 기록 형식(`{"recv": ..., "msg": ...}` 줄).
- Produces: `compare(records: list[dict], window_s: float = 1.0, max_az_err: float = 30.0) -> dict`, 명령 `py -3.12 HaloOverlay/tools/compare_streams.py <file.jsonl>`이 결과를 표로 출력합니다.
  ```text
  - 정답 onset: src="ue" 메시지에 처음 나온 id. 모델 onset: src="model" 메시지에 처음 나온 id.
  - 정답 onset마다 recv가 0~window_s초 뒤이고 방위 차(감아서)가 max_az_err 이내인 첫 모델 onset을 짝짓습니다(한 모델 onset은 한 번만).
  - 결과 키: n_truth, n_matched, recall, class_acc(짝 중 cls 같음), az_mae, front_back_rate(evaluate.py와 같은 정의),
    latency_ms_median, latency_ms_p90(cls가 같은 짝의 recv 차이).
  ```

- [ ] **Step 1: 실패하는 테스트 작성**

  - `test_perfect_pairing`: 정답 3개와 각각 0.15초 뒤, 같은 cls, 방위 5° 차인 모델 이벤트 3개면 `recall == 1`, `class_acc == 1`, `az_mae == 5`, `latency_ms_median == 150`.
  - `test_late_is_missed`: 1.2초 뒤 모델 이벤트는 짝이 안 되어 `recall == 0`.
  - `test_wrong_class_counted`: 방위는 맞고 cls가 다르면 짝은 되고 `class_acc == 0`, 지연 통계에서는 빠집니다.
  - `test_front_back`: 정답 150°, 모델 30°이고 `max_az_err`가 180이면 `front_back_rate == 1`.

- [ ] **Step 2~4: 실패 확인, 구현, 통과 확인** — Run: `py -3.12 -m unittest HaloOverlay/tools/test_compare.py -v`. Expected: 4개 통과.
- [ ] **Step 5: 커밋** — `git commit -m "Tools: compare ground-truth and model event streams"`

### Task 11: 실제 모델 통합과 성능 확인

이 Task는 코드 변경이 거의 없고, 사용자와 함께 하는 확인 절차입니다.

**Files:**
- Modify: `Docs/VERIFICATION.md` (결과 기록)

- [ ] **Step 1: 이 PC에 모델 준비**

  노트북 `~/claude-jobs/20261125-audio-model-eval/`의 `repos/EfficientAT`과 `weights/`(mn10 관련만)를 `Saved/ModelEval/`로 복사합니다(laptop-compute 스킬). `HaloOverlay/analyzer/.venv`에 CUDA판 torch와 torchaudio를 설치합니다. 설치 전에 다운로드 크기(약 2.5~3 GB)를 사용자에게 알리고 동의를 받습니다.

- [ ] **Step 2: 테스트 맵으로 비교**

  사용자에게 표시부 계획 Task 7 Step 5처럼 PIE에서 송신을 켜 달라고 요청합니다(포트 `47800,47801`). 분석기를 `--input loopback --classifier efficientat_mn10 --spatial gcc_phat --ports 47800,47801`로, `record.py --port 47801`을 함께 돌리고, 사용자가 테스트 맵 방 1~7을 한 번씩 돌고 나면 `compare_streams.py`로 결과를 냅니다. 출력 장치를 스테레오로 한 번, 7.1로 한 번 돕니다(7.1 장치가 없으면 그 사실을 기록하고 건너뜁니다). 오버레이 화면은 사용자가 봅니다.
  Expected: recall, class_acc, az_mae, front_back_rate, 지연 중앙값과 90번째 백분위가 나옵니다. 지연 목표는 200 ms 이하입니다.

- [ ] **Step 3: 게임 옆 성능 확인**

  사용자가 고른 게임 하나를 테두리 없는 창 모드로 켜고, 오버레이와 분석기를 끈 상태와 켠 상태에서 각각 2분씩 평균 프레임, CPU 사용률(작업 관리자 또는 `typeperf`), GPU 사용률과 VRAM을 기록합니다. 오버레이 그리기 시간은 `HaloOverlay.exe --stats`(그리기 시간 평균을 10초마다 `OutputDebugString`으로 출력하도록 이 Step에서 추가)로 봅니다.
  Expected: 오버레이 그리기 프레임당 1 ms 이하, 분석기와 오버레이 CPU 합 5% 이하, 게임 평균 프레임 하락 5% 이하. 넘으면 설계 문서 7절 10번대로 분류 홉을 250 ms로 늘려 다시 잽니다.

- [ ] **Step 4: 기록과 커밋**

  `Docs/VERIFICATION.md` 맨 위에 날짜, 장치 설정, 위 수치, 건너뛴 항목을 기존 형식대로 적습니다. `git commit -m "Record external halo overlay verification"`
