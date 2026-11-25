# Halo 오버레이 표시부 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 게임 밖에서 UDP로 받은 소리 이벤트를 게임 안 Sound Halo와 같은 규칙으로 그리는 투명 오버레이를 만들고, 언리얼 테스트 맵이 정답 이벤트를 같은 형식으로 보내게 합니다.

**Architecture:** C++ 오버레이는 프로토콜 해석(Protocol), 기하(Geometry), 시간에 따른 표시 상태(Scene), 그리기(Painter), 창과 루프(main) 다섯 부분으로 나눕니다. 언리얼 쪽은 `SoundOverlaySender`가 프레임을 JSON으로 바꿔 보내고, Python 도구 두 개가 메시지를 기록하고 재생합니다. 분석기(모델 경로)는 별도 계획(`2026-11-25-halo-overlay-analyzer.md`)에서 만듭니다.

**Tech Stack:** C++20, MSVC 2022, CMake, Win32, Direct3D 11, DirectComposition, Direct2D, WIC, nlohmann/json 3.11.3(헤더 하나, 저장소에 포함). Unreal Engine 5.8(`D:\Epic Games\UE_5.8`). Python 3.12 표준 라이브러리.

**Spec:** `Docs/superpowers/specs/2026-11-25-external-sound-halo-overlay-design.md`

## Global Constraints

- 저장소 루트는 `C:\Users\Home\Documents\Github\SoundCompass`, 브랜치는 `PSH`입니다. 로컬에만 커밋하고 push하지 않습니다.
- 게임 프로세스의 메모리, 렌더링, 입력에 접근하지 않습니다. 오버레이 창은 포커스와 입력을 가져가지 않습니다.
- 메시지는 UDP 127.0.0.1, JSON, `"v": 1`입니다. 기본 포트는 47800이고, 기록 도구는 47801을 씁니다. 보내는 쪽은 포트 여러 개로 같은 메시지를 보낼 수 있어야 합니다(Windows에서 유니캐스트 UDP는 한 소켓만 받기 때문입니다).
- 클래스는 `Alarm, Dog, Door, Explosion, Fire, Footstep, GlassBreak, Gunshot, Impact, Item, Reload, Throwable, Vehicle, Voice, Other` 15개입니다.
- 기본 화면에 글자를 그리지 않습니다. 화면 중앙(링 안쪽)은 비워 둡니다. 아이콘은 onset 때 불투명하고 곧 10%(`IconOpacity = 0.1`)로 가라앉습니다.
- 유지 0.60초, 서서히 사라짐 0.80초(`MarkerHoldSeconds`, `MarkerFadeSeconds` 기본값)입니다. 아이콘은 최대 4개입니다.
- 가상 카메라 피치는 30°로 고정합니다(설정 가능).
- 언리얼 C++ 변경은 Live Coding 없이 `Build.bat`으로 빌드합니다. 에디터를 닫기 전에 매번 사용자에게 묻습니다. PIE와 화면 확인은 사용자가 합니다.
- C++ 테스트는 외부 프레임워크 없이 `CHECK` 매크로와 CTest로 돌립니다.

## Review Focus

1. 깨진 JSON, 모르는 `v`, 모르는 `cls`가 든 데이터그램: 그 데이터그램만 버리고 오버레이는 계속 돕니다. (Task 1 `test_protocol`의 `malformed_is_ignored`)
2. 보내는 쪽이 소리 도중에 멈춤(분석기 종료, PIE 중지): 표시가 남아 있지 않고 유지와 서서히 사라짐 시간 뒤 사라집니다. (Task 3 `sender_stops_marks_fade_out`)
3. 방위가 ±180°를 넘나드는 같은 소리(179°와 -179°): 한 표시로 합쳐집니다. (Task 3 `merge_across_wrap`)
4. 설정 파일이 없거나 깨졌거나 모니터 번호가 범위를 벗어남: 기본값과 주 모니터로 뜹니다. (Task 4 `test_config`)
5. 한 번에 이벤트 30개: 아이콘은 4개뿐이고 중앙 링은 모든 방위를 밝힙니다. (Task 3 `many_events_cap_markers`)

---

### Task 1: CMake 뼈대와 프로토콜 해석

**Files:**
- Create: `HaloOverlay/overlay/CMakeLists.txt`
- Create: `HaloOverlay/overlay/third_party/nlohmann/json.hpp` (v3.11.3 릴리스의 `json.hpp`, MIT)
- Create: `HaloOverlay/overlay/src/Protocol.h`, `HaloOverlay/overlay/src/Protocol.cpp`
- Create: `HaloOverlay/overlay/tests/Check.h`, `HaloOverlay/overlay/tests/test_protocol.cpp`
- Modify: `.gitignore` (아래 줄 추가: `HaloOverlay/overlay/build/`, `HaloOverlay/**/__pycache__/`, `HaloOverlay/**/.venv/`)

**Interfaces:**
- Produces (namespace `halo`):
  ```cpp
  enum class DistBin { Near, Mid, Far };
  struct Event {
      int id = 0; std::string cls; float conf = 1.0f; float az = 0.0f; float fb = 0.0f; float level = 0.0f;
      std::optional<float> distM; std::optional<DistBin> distBin; std::optional<float> elevM;
  };
  struct Message { std::string src; std::string mode; double t = 0.0; std::vector<Event> events; };
  std::optional<Message> ParseMessage(std::string_view json); // 깨졌거나 v != 1이면 nullopt
  bool IsKnownClass(std::string_view cls);
  ```
  `Check.h`: `CHECK(cond)`는 실패 시 파일, 줄, 조건을 출력하고 전역 실패 수를 늘립니다. 각 테스트 `main`은 실패 수를 반환합니다.
- CMake 타깃: 정적 라이브러리 `halo_core`(src의 Win32 무관 파일), 실행 파일 `HaloOverlay`, 테스트 `test_protocol`(이후 Task가 `test_geometry`, `test_scene`, `test_config`, `test_painter` 추가). include 경로에 `../../Source/SoundCompass/SoundVisualization`을 넣습니다(`SoundPresentation.h`, `SoundImportancePolicy.h`용).

- [ ] **Step 1: 실패하는 테스트 작성** (`test_protocol.cpp`)

  - `parses_full_event`: 설계 문서 6절의 예시 JSON(`dist_src` 포함)을 넣으면 `src == "model"`, `mode == "stereo"`, 이벤트 1개, `id == 17`, `cls == "Gunshot"`, `az == 35.0f`, `fb == 0.2f`, `!distM`, `distBin == DistBin::Mid`, `!elevM`, `level == 0.7f`.
  - `null_fields_are_empty`: `"dist_bin": null`이면 `!distBin`.
  - `malformed_is_ignored`: `"{"`, `"[]"`, `"v": 2`인 메시지, `az`가 문자열인 이벤트는 모두 `nullopt`.
  - `unknown_class_dropped`: `cls`가 `"Banana"`인 이벤트는 빠지고 나머지 이벤트는 남습니다.
  - `empty_events_ok`: `"events": []`는 이벤트 0개인 메시지입니다.

- [ ] **Step 2: 빌드해서 실패 확인**

  Run: `cmake -S HaloOverlay/overlay -B HaloOverlay/overlay/build -G "Visual Studio 17 2022" -A x64` 다음 `cmake --build HaloOverlay/overlay/build --config Release`
  Expected: `ParseMessage` 미정의로 링크 실패.

- [ ] **Step 3: `ParseMessage`, `IsKnownClass` 구현** (`Protocol.cpp`)

  nlohmann `json::parse(..., nullptr, false)`로 예외 없이 해석합니다. 형식이 틀린 필드가 하나라도 있으면 메시지 전체가 아니라 그 이벤트만 버리되, 최상위 필드(`v`, `src`, `events`)가 틀리면 메시지 전체를 버립니다. `HaloOverlay`의 `main.cpp`는 이 단계에서 빈 `wWinMain`입니다.

- [ ] **Step 4: 테스트 통과 확인**

  Run: `cmake --build HaloOverlay/overlay/build --config Release` 다음 `ctest --test-dir HaloOverlay/overlay/build -C Release --output-on-failure`
  Expected: `test_protocol` 통과.

- [ ] **Step 5: 커밋**

  `git add .gitignore HaloOverlay/overlay` 후 `git commit -m "HaloOverlay: CMake skeleton and message parser"`

### Task 2: 기하 코드 이식

**Files:**
- Create: `HaloOverlay/overlay/src/Math.h`, `HaloOverlay/overlay/src/Geometry.h`
- Create: `HaloOverlay/overlay/tests/test_geometry.cpp`

**Interfaces:**
- Consumes: `Check.h`.
- Produces: `halo::Vec2 {double x, y}`, `halo::Vec3 {double x, y, z}`(덧셈, 뺄셈, 스칼라 곱, `Dot`, `Length`, `Normalized`). `halo::geom`에 `Source/SoundCompass/SoundVisualization/SoundHaloGeometry.h`의 모든 타입과 함수를 같은 이름으로 옮깁니다. 타입 이름만 `F` 접두사를 뺍니다(`View`, `Projected`, `MarkerLayout`, `Box2`). 함수 이름과 인자 순서, 기본값은 그대로입니다(`Project`, `CameraPosition`, `FarAmount`, `DistanceToRadius`, `RadiusToDistance`, `RingPoint`, `PointerDirection`, `EdgePoint`, `Nearness`, `IconBox`, `Overlaps`, `MarkerBox`, `LayoutMarkers`).

- [ ] **Step 1: 실패하는 테스트 작성**

  `Source/SoundCompass/Tests/SoundHaloGeometryTest.cpp`의 검사를 하나도 빼지 않고 같은 입력과 같은 허용 오차로 `test_geometry.cpp`에 옮깁니다(원점 투영, 앞은 위, 뒤는 아래, 앞 띠가 더 얇음, 가장자리 점 세 개, 가장자리와 링의 좌우 일치, 거리와 반지름 함수, 포인터 방향 다섯 개, 가까움 세 개, 겹친 표시 배치 두 개).

- [ ] **Step 2: 빌드해서 실패 확인**

  Run: Task 1 Step 4와 같은 빌드 명령. Expected: `Geometry.h` 없음으로 컴파일 실패.

- [ ] **Step 3: `Math.h`, `Geometry.h` 작성**

  `SoundHaloGeometry.h`의 본문을 그대로 옮기고 `FVector`, `FVector2D`, `FMath`, `FBox2D`만 바꿉니다. 계산식은 바꾸지 않습니다.

- [ ] **Step 4: 테스트 통과 확인**

  Run: Task 1 Step 4의 ctest 명령. Expected: `test_protocol`, `test_geometry` 통과.

- [ ] **Step 5: 커밋** — `git commit -m "HaloOverlay: port halo geometry"`

### Task 3: 표시 상태(Scene)

**Files:**
- Create: `HaloOverlay/overlay/src/Scene.h`, `HaloOverlay/overlay/src/Scene.cpp`, `HaloOverlay/overlay/src/Classes.h`
- Create: `HaloOverlay/overlay/tests/test_scene.cpp`

**Interfaces:**
- Consumes: `halo::Message`, `halo::Event`, `halo::DistBin`(Task 1). `SoundCompassPresentation::Retention`, `SoundCompassPresentation::EnvelopeTrail`(`SoundPresentation.h`), `SoundCompassImportance::Score`(`SoundImportancePolicy.h`).
- Produces:
  ```cpp
  namespace halo {
  struct ClassStyle { float r, g, b; const wchar_t* icon; float baseImportance; }; // icon: L"Bullet" 등, 없으면 nullptr(파동을 직접 그림), L"!"는 Alarm
  const ClassStyle& StyleOf(std::string_view cls);
  float BinCentreMeters(DistBin bin);           // Near 2.5, Mid 10, Far 30
  struct SceneConfig { std::string srcFilter = "model"; float holdSeconds = 0.60f; float fadeSeconds = 0.80f;
                       int maxMarkers = 4; float mergeDegrees = 20.0f; float mirrorBelowFb = 0.5f; };
  struct Mark { int track = 0; std::string cls; float az = 0; std::optional<float> distM; std::optional<float> elevM;
                float alpha = 0; float onsetAge = 0; float importance = 0; bool mirrored = false; bool marker = false;
                SoundCompassPresentation::EnvelopeTrail trail; };
  class Scene {
  public:
      explicit Scene(SceneConfig config = {});
      void Apply(const Message& message, double now);   // srcFilter와 다른 src는 무시
      std::vector<Mark> Snapshot(double now) const;      // importance 내림차순, marker는 상위 maxMarkers개만 true
      bool Idle(double now) const;                       // 보이는 Mark가 없으면 true
  };
  }
  ```
  `ClassStyle` 색은 `SoundHaloWidget.cpp`의 `CategoryColor`에서 가져옵니다(Gunshot, Explosion, Footstep, Door, GlassBreak는 같은 이름, Voice는 Conversation, Alarm은 Alert, Throwable은 Throw, Other는 기본 청록). Dog, Fire, Impact, Item, Reload, Vehicle 색은 구현자가 정하되 서로 구분되고 빨강 계열을 쓰지 않습니다. 아이콘은 `IconFor`를 따릅니다(Vehicle은 Wheel). `baseImportance`는 구현자가 정하되 Gunshot, Explosion이 가장 높습니다.

- [ ] **Step 1: 실패하는 테스트 작성** (`test_scene.cpp`, 모든 시각은 초)

  - `event_appears_then_settles`: t=0에 Gunshot 하나를 적용하면 t=0 스냅샷에 Mark 1개, `alpha > 0.99`, `onsetAge == 0`.
  - `sender_stops_marks_fade_out`: t=0에 적용하고 더 보내지 않으면 t=0.5에 `alpha == 1`, t=1.0에 `0 < alpha < 1`, t=1.41에 Mark 0개, `Idle(1.41) == true`.
  - `same_id_is_one_track`: 같은 `id`를 0.1초 간격으로 5번 보내면 Mark 1개이고 `onsetAge`가 0.4.
  - `merge_across_wrap`: 같은 클래스, 다른 `id`, `az` 179와 -179이면 Mark 1개.
  - `different_class_not_merged`: `az`가 같아도 Gunshot과 Door는 Mark 2개.
  - `mirror_when_front_back_unknown`: `fb = 0.2`, `az = 30`이면 Mark 2개(`az` 30과 150), 둘 다 `mirrored == true`, 각각 `alpha`는 `fb = 1`일 때의 절반. `fb = 0.8`이면 Mark 1개.
  - `bin_sets_distance`: `distM` 없음, `distBin = Far`이면 `distM == 30`. 둘 다 없으면 `!distM`.
  - `many_events_cap_markers`: 서로 다른 방위의 이벤트 30개를 적용하면 Mark 30개(미러 없음), 그중 `marker == true`는 4개이고 그 4개의 `importance`가 나머지보다 크거나 같습니다.
  - `src_filter`: 기본 설정에서 `src = "ue"` 메시지는 무시됩니다. `srcFilter = "ue"`면 반영됩니다.
  - `level_feeds_trail`: `level` 0.2, 0.8, 0.5를 차례로 보내면 `trail.Count == 3`, `trail.PeakEnvelope == 0.8f`.

- [ ] **Step 2: 빌드해서 실패 확인** — Expected: `Scene.h` 없음으로 컴파일 실패.

- [ ] **Step 3: `Scene` 구현**

  트랙은 `id`가 같거나, 클래스가 같고 방위 차(±180° 감아서 계산)가 `mergeDegrees` 이내면 같은 트랙입니다. 트랙마다 마지막으로 들린 시각과 onset 시각을 두고, `alpha = Retention(now - lastSeen, hold, fade)`입니다. 미러는 스냅샷을 만들 때 트랙 하나를 Mark 둘로 펼칩니다. 중요도는 `Score(baseImportance, distM 또는 NaN, 20*log10(max(level, 1e-4)))`이고, 거리가 없으면 `Score`가 근접도를 0으로 처리합니다. 사라진 트랙은 `hold + fade`가 지나면 지웁니다.

- [ ] **Step 4: 테스트 통과 확인** — Run: Task 1 Step 4의 ctest 명령. Expected: 세 테스트 모두 통과.

- [ ] **Step 5: 커밋** — `git commit -m "HaloOverlay: scene state with retention, merge and mirroring"`

### Task 4: 투명 창, UDP 수신, 설정, 단축키

**Files:**
- Create: `HaloOverlay/overlay/src/Config.h`, `HaloOverlay/overlay/src/Config.cpp`
- Create: `HaloOverlay/overlay/src/UdpReceiver.h`, `HaloOverlay/overlay/src/UdpReceiver.cpp`
- Create: `HaloOverlay/overlay/src/OverlayWindow.h`, `HaloOverlay/overlay/src/OverlayWindow.cpp`
- Modify: `HaloOverlay/overlay/src/main.cpp`
- Create: `HaloOverlay/overlay/tests/test_config.cpp`
- Create: `HaloOverlay/overlay/halo_overlay.json` (기본 설정, 빌드 후 실행 파일 옆으로 복사)

**Interfaces:**
- Consumes: `ParseMessage`(Task 1), `Scene`(Task 3).
- Produces:
  ```cpp
  struct Config { int port = 47800; int monitor = 0; std::string labelsHotkey = "Ctrl+Alt+L";
                  std::string quitHotkey = "Ctrl+Alt+Q"; float viewPitchDegrees = 30.0f; std::string src = "model"; };
  Config LoadConfig(const std::filesystem::path& path);      // 없거나 깨지면 기본값, 깨진 경우 OutputDebugString으로 알림
  bool ParseHotkey(std::string_view text, UINT& modifiers, UINT& vk); // "Ctrl+Alt+L" → MOD_CONTROL|MOD_ALT, 'L'
  class UdpReceiver { public: explicit UdpReceiver(int port); std::vector<std::string> Drain(); bool Ok() const; };
  class OverlayWindow { public: bool Create(int monitor); HWND Hwnd() const; ID2D1DeviceContext* Begin(); void End(); Vec2 SizePixels() const; };
  ```
  `OverlayWindow::Create`는 범위 밖 모니터 번호면 주 모니터를 씁니다. 확장 스타일은 `WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP`이고, `SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA)`로 클릭 통과를 켭니다. 그리기 대상은 `CreateSwapChainForComposition`(`DXGI_ALPHA_MODE_PREMULTIPLIED`, B8G8R8A8)을 DirectComposition 비주얼에 붙이고 Direct2D 장치 컨텍스트로 그립니다.

- [ ] **Step 1: 실패하는 테스트 작성** (`test_config.cpp`)

  - `missing_file_defaults`: 없는 경로면 `port == 47800`, `monitor == 0`, `viewPitchDegrees == 30`.
  - `broken_file_defaults`: 내용이 `"{"`인 임시 파일이면 기본값.
  - `partial_file_merges`: `{"port": 47900}`이면 `port == 47900`이고 나머지는 기본값.
  - `hotkey_parse`: `"Ctrl+Alt+L"` → `MOD_CONTROL | MOD_ALT`, `'L'`. `"Ctrl+Shift+F9"` → `MOD_CONTROL | MOD_SHIFT`, `VK_F9`. `"Banana"`는 `false`.

- [ ] **Step 2: 빌드해서 실패 확인** — Expected: `Config.h` 없음으로 컴파일 실패.

- [ ] **Step 3: `Config`, `UdpReceiver`, `OverlayWindow`, `main` 구현**

  `UdpReceiver`는 127.0.0.1에 묶은 논블로킹 소켓이고 `Drain`은 쌓인 데이터그램을 모두 꺼냅니다. `main` 루프는 메시지를 `Scene::Apply`에 넣고, `Scene::Idle`이 아니면 약 60 Hz로 그리며, Idle이 되면 한 번 더 그린 뒤(Task 5부터는 빈 링) `MsgWaitForMultipleObjects`로 소켓 이벤트(`WSAEventSelect`)나 단축키를 기다립니다. 이 Task에서 그리기는 화면을 투명하게 지우기만 합니다. 단축키는 `RegisterHotKey`로 등록하고, 등록 실패(다른 프로그램이 사용 중)는 `OutputDebugString`으로 알린 뒤 계속 실행합니다. 포트 바인드 실패면 메시지 상자 하나를 띄우고 종료합니다.

- [ ] **Step 4: 테스트와 창 자체 검사**

  Run: Task 1 Step 4의 ctest 명령. Expected: 네 테스트 통과.
  Run: `HaloOverlay/overlay/build/Release/HaloOverlay.exe --selftest-window`. 이 옵션은 창을 만든 뒤 `GetWindowLongPtr(GWL_EXSTYLE)`에 위 여섯 플래그가 모두 있는지, `GetForegroundWindow() != hwnd`인지 확인하고 종료 코드 0(통과) 또는 1을 반환합니다.
  Expected: 종료 코드 0.

- [ ] **Step 5: 커밋** — `git commit -m "HaloOverlay: click-through composition window, UDP input, config and hotkeys"`

### Task 5: 그리기(Painter)와 아이콘

**Files:**
- Create: `HaloOverlay/overlay/src/Painter.h`, `HaloOverlay/overlay/src/Painter.cpp`
- Create: `HaloOverlay/tools/export_icons.py` (언리얼 에디터 Python)
- Create: `HaloOverlay/overlay/icons/*.png` (export_icons.py 결과, 커밋)
- Modify: `HaloOverlay/overlay/src/main.cpp` (Painter 연결, 라벨 단축키)
- Create: `HaloOverlay/overlay/tests/test_painter.cpp`

**Interfaces:**
- Consumes: `halo::geom::*`(Task 2), `Mark`, `StyleOf`(Task 3), `OverlayWindow::Begin/End`, `Config`(Task 4).
- Produces:
  ```cpp
  struct PaintParams { Vec2 sizePixels; float pitchDegrees = 30.0f; bool labels = false; double now = 0.0; };
  class Painter {
  public:
      Painter(ID2D1RenderTarget* target, IWICImagingFactory* wic, const std::filesystem::path& iconsDir); // 아이콘이 없으면 파동으로 대체
      void Paint(const std::vector<Mark>& marks, const PaintParams& params);
  };
  ```
  크기와 투명도 상수는 `SoundHaloWidget.h`의 속성 기본값을 그대로 씁니다(`RingRadius 130`, `RingThickness 7`, `LevelBulge 2`, `IdleRingOpacity 0.3`, `CameraDistance 4.5`, `MaxRangeRadius 2.6`, `EdgeEllipseScale 0.75`, `NearMarkerRadius 1.6`, `IconSize 40`, `IconOpacity 0.1`, `FarDistanceMeters 40`, `OnsetWaveSeconds 0.6`, `PointerLength 0.4`, `VerticalThresholdMeters 2`). 900 px 높이 기준 값이므로 화면 높이 비율로 늘립니다.

- [ ] **Step 1: 아이콘 내보내기**

  `export_icons.py`는 `/Game/Ariel_NewMaterial/Ariel_{Airplane,Bullet,Door,Footprint,GlassBreak,Grenade,Throw,Wheel}`을 `unreal.AssetExportTask`와 `unreal.TextureExporterPNG`로 `HaloOverlay/overlay/icons/<이름>.png`에 씁니다.
  Run: `"D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\Home\Documents\Github\SoundCompass\SoundCompass.uproject" -run=pythonscript -script="C:\Users\Home\Documents\Github\SoundCompass\HaloOverlay\tools\export_icons.py" -unattended -nosplash`
  Expected: PNG 8개. 실패하면 사용자에게 에디터 Output Log에서 `py "<스크립트 경로>"`로 실행해 달라고 요청합니다.

- [ ] **Step 2: 실패하는 테스트 작성** (`test_painter.cpp`)

  1600×900 WIC 비트맵(`GUID_WICPixelFormat32bppPBGRA`)에 `CreateWicBitmapRenderTarget`으로 그리고 픽셀을 읽습니다.
  - `empty_scene_is_transparent`: Mark가 없으면 중앙(800, 450)과 네 모서리의 알파가 0이고, 정면 링 점(`Project(RingPoint(0))`의 화면 위치)의 알파는 0보다 큽니다(빈 링은 `IdleRingOpacity`로 늘 보입니다).
  - `centre_stays_clear`: 정면 2 m의 Gunshot 하나를 그려도 중앙 픽셀 알파는 0.
  - `right_sound_draws_on_right`: `az = 90`, `distM = 30`인 Mark의 아이콘 영역(`EdgePoint(90)` 근처 40 px 사각형)에 알파가 0이 아닌 픽셀이 있고, 같은 크기의 왼쪽 대칭 영역에는 없습니다.
  - `no_text_by_default`: `labels = false`일 때 그린 결과와 `labels = true`일 때 그린 결과가 다릅니다(라벨이 켤 때만 생긴다는 확인).
  - 통과 후 `HaloOverlay/overlay/build/painter_sample.png`에 대표 장면(4방향, 미러 1개, 거리 없음 1개)을 저장합니다. 사용자 확인용입니다.

- [ ] **Step 3: 빌드해서 실패 확인** — Expected: `Painter.h` 없음으로 컴파일 실패.

- [ ] **Step 4: `Painter` 구현**

  `SoundHaloWidget.cpp`의 `NativePaint`(338~655줄)를 Direct2D로 옮깁니다. 중앙 링, 범위 링, 사면체 포인터, 가장자리 아이콘과 배치(`LayoutMarkers`), onset 파동, F1 라벨 순서와 계산식을 유지합니다. 바뀌는 점은 설계 문서 8절 그대로입니다. 카메라 피치는 `PaintParams::pitchDegrees`, `distM`이 없으면 포인터 생략과 중간 반경 아이콘, `elevM`이 없으면 수평 포인터, `mirrored`는 이미 절반이 된 `alpha`를 그대로 씁니다. 차폐 회색은 없습니다. Slate의 `MakeLines`는 `ID2D1PathGeometry`, `MakeBox`는 `DrawBitmap`, 글자는 DirectWrite로 바꿉니다.

- [ ] **Step 5: 테스트 통과 확인** — Run: Task 1 Step 4의 ctest 명령. Expected: 다섯 테스트 통과, `painter_sample.png` 생성.

- [ ] **Step 6: 커밋** — `git add HaloOverlay` 후 `git commit -m "HaloOverlay: Direct2D halo painter and exported icons"`

### Task 6: 기록과 재생 도구

**Files:**
- Create: `HaloOverlay/tools/record.py`, `HaloOverlay/tools/replay.py`, `HaloOverlay/tools/send_demo.py`
- Create: `HaloOverlay/tools/test_tools.py`

**Interfaces:**
- Produces:
  - `record.py --port 47801 --out <file.jsonl>`: 한 줄에 `{"recv": <time.perf_counter()>, "msg": <받은 JSON>}`. Ctrl+C로 끝냅니다. 깨진 데이터그램은 `{"recv": ..., "raw": "<문자열>"}`로 남깁니다.
  - `replay.py <file.jsonl> --ports 47800 [--speed 1.0]`: `recv` 간격을 지켜 `msg`를 다시 보냅니다.
  - `send_demo.py --ports 47800`: 4방향(0, 90, 180, -90)에 Gunshot, Footstep, Door, Voice를 1초 간격으로 돌아가며 보내고, `fb = 0.2`인 소리와 거리 없는 소리를 하나씩 섞습니다. 사용자 화면 확인용입니다.
  - 함수: `record.record(port: int, out: Path, stop: threading.Event)`, `replay.replay(path: Path, ports: list[int], speed: float = 1.0)`.

- [ ] **Step 1: 실패하는 테스트 작성** (`test_tools.py`, `unittest`)

  - `test_round_trip`: 47851 포트로 `record`를 스레드에서 돌리고, 메시지 3개를 0.1초 간격으로 보낸 뒤 멈추면 파일에 3줄, `msg`가 보낸 것과 같고 `recv`가 증가합니다.
  - `test_replay_timing`: 그 파일을 47852로 `replay`하고 받는 쪽에서 재면 첫 메시지와 마지막 메시지 간격이 0.2초 ± 0.05초입니다.
  - `test_raw_kept`: `b"{"`를 보내면 `raw` 줄이 생깁니다.

- [ ] **Step 2: 실패 확인** — Run: `py -3.12 -m unittest HaloOverlay/tools/test_tools.py -v`. Expected: ImportError.

- [ ] **Step 3: 세 스크립트 구현** (표준 라이브러리만 사용)

- [ ] **Step 4: 통과 확인** — Run: Step 2 명령. Expected: 3개 통과.

- [ ] **Step 5: 사용자 화면 확인 요청**

  `HaloOverlay.exe`를 실행하고 `py -3.12 HaloOverlay/tools/send_demo.py --ports 47800`를 돌린 뒤, 사용자에게 바탕화면이나 테두리 없는 창 모드 게임 위에서 확인해 달라고 요청합니다. 확인 항목은 클릭 통과, 포커스 유지, 중앙이 비어 있는지, 미러 표시, Ctrl+Alt+L 라벨, Ctrl+Alt+Q 종료입니다.

- [ ] **Step 6: 커밋** — `git commit -m "HaloOverlay: record, replay and demo tools"`

### Task 7: 언리얼 정답 송신

**Files:**
- Create: `Source/SoundCompass/SoundVisualization/SoundOverlaySender.h`, `Source/SoundCompass/SoundVisualization/SoundOverlaySender.cpp`
- Modify: `Source/SoundCompass/SoundVisualization/SoundVisualizationSubsystem.h`, `.cpp` (Tick에서 송신)
- Modify: `Source/SoundCompass/SoundCompass.Build.cs` (`PrivateDependencyModuleNames`에 `"Sockets"`, `"Networking"` 추가)
- Create: `Source/SoundCompass/Tests/SoundOverlaySenderTest.cpp`

**Interfaces:**
- Consumes: `FSoundCompassSourceFrame`, `USoundVisualizationSubsystem::GetSourceFrames(APlayerController*)`, `SoundCompassCategory(FName)`.
- Produces:
  ```cpp
  namespace SoundOverlay
  {
  FString ToOverlayClass(FName Category);
  FString BuildMessage(const TArray<FSoundCompassSourceFrame>& Frames, double TimeSeconds); // bPlaying인 프레임만
  class FSender { public: void Send(const FString& Json, const TArray<int32>& Ports); ~FSender(); }; // 소켓은 처음 Send에서 생성
  }
  ```
  콘솔 변수: `SoundCompass.ExternalOverlay`(int, 기본 0), `SoundCompass.ExternalOverlayPorts`(string, 기본 `"47800"`). 켜져 있으면 서브시스템 `Tick`이 1/30초마다 첫 로컬 플레이어 컨트롤러로 `GetSourceFrames`를 불러 보냅니다. 중요도로 거르지 않은 전체 목록이라 정답으로 쓰기에 맞습니다.
  `ToOverlayClass` 규칙(순서대로): 이름이 15개 클래스 중 하나와 대소문자 무시로 같으면 그 클래스. `SoundCompassCategory` 결과가 Gunshot, Explosion, Footstep, Door, GlassBreak면 같은 이름, Conversation은 Voice, Alert는 Alarm, Throw는 Throwable. 결과가 Noise이고 원래 이름에 heli, plane, aircraft, engine, car, traffic이 있으면 Vehicle. 나머지(Ambience 포함)는 Other.
  메시지 필드: `"src": "ue"`, `"mode": "ground_truth"`, `id = SourceId`, `conf = 1`, `fb = 1`, `az = AzimuthDegrees`, `dist_m = DistanceMeters`, `dist_bin`(5 m 미만 near, 15 m 미만 mid, 그 외 far), `dist_src = "ue"`, `elev_m = ElevationMeters`, `level = Clamp(EstimatedListenerLevel, 0, 1)`.

- [ ] **Step 1: 실패하는 테스트 작성** (`SoundOverlaySenderTest.cpp`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, 이름 `SoundCompass.Overlay.Sender`)

  - 매핑: `Conversation→Voice`, `Alert→Alarm`, `Throw→Throwable`, `Ambience→Other`, `Helicopter→Vehicle`, `Jackhammer→Other`, `Gunshot→Gunshot`, `Dog→Dog`, `glassbreak→GlassBreak`.
  - 메시지: 재생 중 프레임 1개(Category `Conversation`, Azimuth 45, Distance 12)와 재생 끝난 프레임 1개를 넣으면 결과를 `FJsonSerializer`로 읽었을 때 `events` 1개, `cls == "Voice"`, `az == 45`, `dist_bin == "mid"`, `v == 1`. 이 검사를 위해 `"Json"` 모듈을 테스트에만 쓰려면 `PrivateDependencyModuleNames`에 `"Json"`도 추가합니다.

- [ ] **Step 2: 에디터 종료 확인 후 빌드해서 실패 확인**

  사용자에게 에디터를 저장하고 닫아도 되는지 묻고, 열려 있는 `UnrealEditor` 프로세스가 없는지 확인합니다.
  Run: `"D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SoundCompassEditor Win64 Development -Project="C:\Users\Home\Documents\Github\SoundCompass\SoundCompass.uproject" -WaitMutex`
  Expected: `SoundOverlay::ToOverlayClass` 미정의로 실패.

- [ ] **Step 3: `SoundOverlaySender` 구현과 서브시스템 연결**

  JSON은 `TJsonWriter`로 씁니다. 소켓은 `ISocketSubsystem`의 UDP 소켓 하나를 만들어 포트마다 `SendTo`합니다. 서브시스템 `Deinitialize`에서 소켓을 닫습니다.

- [ ] **Step 4: 빌드와 테스트 통과 확인**

  Run: Step 2의 빌드 명령. Expected: 성공.
  Run: `"D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\Home\Documents\Github\SoundCompass\SoundCompass.uproject" -ExecCmds="Automation RunTests SoundCompass.Overlay;Quit" -unattended -nullrhi -nosplash -log`
  Expected: 로그에 `SoundCompass.Overlay.Sender` 성공, 기존 `SoundCompass.SoundHalo.Geometry`도 `-ExecCmds="Automation RunTests SoundCompass;Quit"`로 함께 통과.

- [ ] **Step 5: 사용자 통합 확인 요청**

  사용자에게 에디터를 다시 열고, PIE에서 콘솔에 `SoundCompass.ExternalOverlay 1`, `SoundCompass.ExternalOverlayPorts 47800,47801`을 입력한 뒤, `HaloOverlay.exe`(설정 `"src": "ue"`)와 `record.py`를 켜고 테스트 맵 소리를 들어 달라고 요청합니다. 게임 안 Halo와 방향이 맞는지는 사용자가 봅니다. 기록 파일은 분석기 계획의 비교 도구 입력으로 씁니다. 게임 안 Halo는 캐릭터 기준, 송신은 리스너 기준이라 가까운 소리일수록 방향이 조금 다를 수 있다는 점을 함께 알립니다.

- [ ] **Step 6: 커밋** — `git add Source HaloOverlay` 후 `git commit -m "Send ground-truth sound frames to the external halo overlay"`
