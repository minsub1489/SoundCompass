# SoundCompass 테스트 게임

SoundCompass가 실제 게임 플레이에 도움이 되는지 확인하기 위한 3인칭 교전 웹 게임입니다. 게임은 소리만 내며, 시각화는 외부의 SoundCompass 시스템이 컴퓨터 소리 출력을 받아 그립니다.

설계는 `docs/superpowers/specs/2026-11-25-soundcompass-testbed-design.md`, 구현 계획은 `docs/superpowers/plans/2026-11-25-soundcompass-testbed.md`에 있습니다.

## 실행

최신 Chrome을 사용합니다. 인터넷 연결은 필요하지 않습니다. `index.html`을 파일로 직접 열면 음원을 불러올 수 없으므로 반드시 로컬 서버로 엽니다.

Node가 설치된 PC (이 폴더에서):

```powershell
npm install
npm run build
npm run preview
```

빌드된 `dist` 폴더만 옮긴 PC (Python이 있는 경우, `dist` 폴더에서):

```powershell
python -m http.server 8080
```

그다음 Chrome에서 표시된 주소(예: `http://localhost:8080`)를 엽니다.

## 진행 순서

1. 시작 화면에서 참가자 ID, 조건 이름, 맵, 출력 방식을 입력하고 시작합니다.
2. 연습장(90초)을 한 번 플레이합니다. 연습 결과는 저장하지 않습니다.
3. 본 맵 3개(도심 골목, 창고 실내, 야외 숲)를 3분씩 플레이합니다. 맵과 조건의 짝은 참가자마다 라틴 방격으로 바꿉니다. 배정표는 실험 진행자가 관리합니다.
4. 라운드가 끝날 때마다 결과 화면이 뜨고 CSV가 저장됩니다. "다음"을 누르면 시작 화면으로 돌아갑니다.

조작: WASD 이동, Shift 달리기, Space 점프, 마우스 조준, 왼쪽 클릭 사격, R 재장전, Esc 일시정지(남은 시간이 멈춥니다).

## 출력 방식

- **바이노럴 스테레오**: HRTF로 3D 위치를 표현합니다. 헤드폰 기준입니다.
- **7.1 채널**: Windows 소리 설정에서 출력 장치를 7.1로 바꾼 뒤 게임 페이지를 열거나 새로 고칩니다. 장치가 8채널을 지원하지 않으면 게임이 시작되지 않고 안내 문구가 뜹니다. 7.1에는 높이 스피커가 없으므로 위아래 정보는 가림과 거리로만 전달됩니다.

## 주의: "소리 끔" 조건

게임 음량을 끄거나 Windows 출력을 음소거하면 SoundCompass가 받을 입력도 사라집니다. "소리 끔" 조건은 게임 소리를 그대로 출력한 채로, 참가자가 소리를 듣지 못하게 하는 방식(예: 헤드폰을 쓰지 않음)으로 만듭니다. 게임에는 실험용 음소거 기능이 없습니다.

## 결과 CSV

Chrome 다운로드 폴더에 `<참가자ID>_<맵>_<날짜-시각>.csv`로 저장됩니다. UTF-8(BOM 포함)이라 Excel에서 한글이 깨지지 않습니다. 다운로드가 막혔거나 파일이 보이지 않으면 결과 화면의 "CSV 다시 저장"을 누릅니다.

CSV에는 입력한 값이 그대로 기록되지만, Excel은 일부 값을 바꿔 보여 줍니다. 조건 이름을 `=`, `+`, `-`, `@`로 시작하지 마세요(수식으로 읽혀 `#NAME?`이 됩니다). 참가자 ID `01`은 Excel에서 `1`로 보이므로 `P01`처럼 문자로 시작하는 ID를 권합니다.

| 열 | 내용 |
| --- | --- |
| participant_id | 참가자 ID |
| condition | 시작 화면에 입력한 조건 이름 |
| map | urban, warehouse, forest |
| audio_output | binaural 또는 7.1 |
| started_at | 라운드 시작 시각(ISO 8601, 현지 시각과 UTC 오프셋, 예: `2026-12-01T19:20:30+09:00`) |
| crates | 모은 보급 상자 수 |
| kills | 처치 수 |
| deaths | 사망 수 |
| damage_taken | 받은 총 피해 |
| surprise_hits | 기습 피해 횟수: 피격 순간 공격한 적이 화면 밖에 있었거나 벽·바닥에 가려 보이지 않았던 횟수 |

## 음원

`public/audio`의 음원은 `tools/audio_build.py`가 CC0 원본과 로컬 ComfyUI(Stable Audio 3)로 만듭니다. 출처는 `public/audio/CREDITS.md`에 있습니다. 소리를 다시 만들려면 ComfyUI를 켜고 다음을 실행합니다(`--only`로 일부만 지정 가능).

```powershell
& "C:\Users\Home\AppData\Local\Comfy-Desktop\ComfyUI-Installs\ComfyUI\ComfyUI\.venv\Scripts\python.exe" -X utf8 tools/audio_build.py --force --only knife_swing
```

## 개발

```powershell
npm run dev
```

```powershell
npm test
```
