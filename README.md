# 가상 PLC (vPLC) C++ 런타임 엔진

Modern C++ (C++17)로 정교하게 기획 및 구현된 고성능 **가상 PLC (Virtual Programmable Logic Controller) 런타임 에뮬레이터**입니다. 

실제 하드웨어 PLC 장비 없이도 PLC 코어 메모리 관리, 20ms 정밀 스캔 주기 타이밍 제어, Modbus TCP 통신 프로토콜 대기 및 수신, TUI 모니터링 인터페이스, 그리고 컴파일된 외부 제어 로직 공유 라이브러리(`.dylib` / `.so`)의 런타임 동적 핫 리로드를 완벽하게 수행합니다.

시멘스(Siemens) 및 오픈 소스 PLC 표준 컴파일러인 **MatIEC** 스펙과의 변수명 및 생명주기 호환성을 제공하여 뛰어난 확장성을 가집니다.

---

## 🚀 주요 특징 (Key Features)

*   **스레드 세이프 메모리 맵 (`PlcMemory`)**: C++17 `std::shared_mutex` 독점/공유 락을 활용하여 Modbus 서버 스레드, PLC 연산 스레드, TUI 스레드 간 충돌 없는 동시 읽기/쓰기를 지원합니다.
*   **고정밀 스케줄러 (`PlcScheduler`)**: 누적 시간 편차(Drift)를 지속 감쇄하는 알고리즘을 도입하여 20ms 사이클 오차(Jitter) 및 연산 스캔 타임을 마이크로초 단위로 정밀하게 추적합니다.
*   **플러그인 로직 로더 (`PlcLoader`)**: POSIX 표준 `dlopen`/`dlsym`을 구현하여 외부 로직 라이브러리를 동적 로드하고, MatIEC 컴파일 규격 변수(`__IX0_0`, `__QX0_0` 등)를 자동 탐색하여 PLC 버퍼에 바인딩합니다.
*   **더블 버퍼링 ANSI TUI 대시보드 (`PlcTui`)**: 외부 모듈 없이 깜빡임이 없는 반응형 터미널 UI 화면을 구현하고, non-blocking termios 키 입력 제어 모드를 지원합니다.
*   **산업용 Modbus TCP 서버 (`ModbusServer`)**: 표준 소켓 API 기반의 멀티스레드 소켓 구조로 8대 Modbus 핵심 펑션 코드(`FC 01, 02, 03, 04, 05, 06, 15, 16`) 및 예외 처리 프레임을 안정적으로 구현했습니다.

---

## 📂 디렉터리 구조

```text
vPlc/
├── CMakeLists.txt              # CMake 빌드 파일
├── README.md                   # 본 설명서 파일
├── libmock_logic.dylib         # 컴파일된 모의 PLC 로직 라이브러리
├── test_modbus.py              # 로우 소켓 기반 독립형 프로토콜 검증 테스트
└── src/
    ├── main.cpp                # 프로그램 진입 엔트리포인트 및 초기화
    ├── core/
    │   ├── PlcMemory.hpp/cpp   # 4대 레지스터 영역 관리 및 스레드 락 엔진
    │   ├── PlcLoader.hpp/cpp   # 동적 공유 라이브러리 로더 및 기호 변수 바인더
    │   └── PlcScheduler.hpp/cpp# 고정밀 타임 스캔 주기 구동 및 통계 기록 엔진
    ├── modbus/
    │   └── ModbusServer.hpp/cpp# 멀티스레드 Modbus TCP 서버 엔진
    ├── tui/
    │   └── PlcTui.hpp/cpp      # termios 로우 모드 ANSI 터미널 UI 제어 엔진
    └── logic/
        └── mock_logic.cpp      # MatIEC lifecycle을 모사한 모의 제어 알고리즘
```

---

## 🛠 컴파일 및 빌드 방법 (Compilation)

시스템에 설치된 C++17 이상 규격의 C++ 컴파일러(Apple Clang / GCC)를 통해 직접 컴파일 및 링크를 수행합니다.

### 1. 가상 PLC 메인 런타임 컴파일
```bash
clang++ -std=c++17 -O3 -pthread src/main.cpp src/core/PlcMemory.cpp src/core/PlcLoader.cpp src/core/PlcScheduler.cpp src/modbus/ModbusServer.cpp src/tui/PlcTui.cpp -o vPlc -Isrc
```

### 2. 모의 PLC 로직 공유 라이브러리 컴파일
*   **macOS (Apple Silicon / Intel)**:
    ```bash
    clang++ -std=c++17 -O3 -shared -fPIC src/logic/mock_logic.cpp -o libmock_logic.dylib
    ```
*   **Linux (Ubuntu 등)**:
    ```bash
    g++ -std=c++17 -O3 -shared -fPIC src/logic/mock_logic.cpp -o libmock_logic.so
    ```

---

## 🏃 구동 및 사용 방법 (How to Run)

### 1. 기본 구동
컴파일이 완료되면 인자값 없이 실행 시 기본 모의 로직(`libmock_logic.dylib`)을 불러와서 구동합니다.
```bash
./vPlc
```

### 2. 특정 커스텀 제어 공유 라이브러리 로드
첫 번째 인자로 직접 컴파일한 공유 라이브러리 경로를 지정할 수 있습니다.
```bash
./vPlc ./my_custom_logic.dylib
```

---

## 🎮 TUI 모니터 실시간 조작법 (Interactive Controls)

`vPlc`가 구동되면 터미널 화면이 대시보드로 변경됩니다. 터미널이 선택된 상태에서 실시간으로 키보드를 눌러 데이터를 제어할 수 있습니다.

*   **`[I]` 키**: 디지털 입력 **`%IX0.0`** 상태를 토글(ON/OFF)합니다.
    - `%IX0.0`이 ON이 되면 로직 규칙에 따라 릴레이 출력 **`%QX0.0`**이 즉각 연동되어 활성화되고, 데이터 레지스터 카운터 **`%MW0`**의 값이 빠르게 상승하기 시작합니다.
*   **`[O]` 키**: 디지털 입력 **`%IX0.1`** 상태를 토글합니다.
*   **`[A]` 키**: 아날로그 센서 인풋 **`%IW0`**의 값을 `+10` 증가시킵니다.
    - 값 변경 시 로직 식(`%MW1 = %IW0 * 2`)이 연산 스레드에 의해 즉각 돌며 **`%MW1`**에 2배 곱해진 수치가 실시간 반영됩니다.
*   **`[Z]` 키**: 아날로그 센서 인풋 **`%IW0`**의 값을 `-10` 감소시킵니다. (음수 방지 최소값 0)
*   **`[Q]` 키**: 통신 소켓을 깨끗이 닫고, 백그라운드 스레드를 소멸한 뒤 터미널 버퍼 환경을 완벽 복구하며 **안전하게 종료**합니다.

---

## 🧪 Modbus TCP 통신 검증 테스트 (Modbus Test)

가상 PLC 런타임은 외부 SCADA/HMI나 분산 제어용 소켓 통신을 위해 기본 포트 **`5020`**번(root 권한 우회용)으로 통신을 열어 둡니다.

가상 PLC TUI 화면이 구동 중인 상태에서 **다른 터미널 창을 열고** 테스트 클라이언트 스크립트를 기동합니다:
```bash
python3 test_modbus.py
```

### 검증 자동화 시나리오:
1.  **Pre-Test**: `FC 04`로 입력 레지스터 `%IW0`을 실시간 감지.
2.  **Test 1**: `FC 03`으로 유지 레지스터 일괄 스캔 -> 로직 카운터 시작 여부(`>=100`) 및 센서 배율 연산 데이터(`%MW1 == %IW0 * 2`)의 완벽한 일치를 대조 검증.
3.  **Test 2/3**: `FC 06` 단일 레지스터 강제 쓰기 및 데이터 보존성 검증.
4.  **Test 4**: `FC 01`로 현재 구동 코일 현황 감지.
5.  **Test 5/6**: `FC 0F`로 여러 코일 일괄 강제 쓰기 및 로직 오버라이트 차단성(로직 미맵핑 코일의 독립 저장 상태 유지) 최종 무인 판정.

---

## 📝 MatIEC 컴파일 및 연동 가이드

**MatIEC (IEC 61131-3 Standard to C Compiler)**와 결합하여 직접 작성한 래더 로직(LD)이나 구조화 텍스트(ST) 알고리즘을 C 소스로 변환 후 가상 PLC 런타임에 핫 리로드할 수 있습니다.

1.  Structured Text (`logic.st`) 작성 시 입력 변수는 `%IX0.0`, 출력은 `%QX0.0`, 아날로그 및 메모리는 `%IW0`, `%MW0` 계열로 선언합니다.
2.  `iec2c logic.st`를 실행하여 컴파일된 `POUS.c` 및 `config.c`를 확보합니다.
3.  생성된 C 코드들을 묶어 공유 라이브러리로 컴파일합니다:
    ```bash
    clang++ -O3 -shared -fPIC POUS.c config.c -o my_logic.dylib
    ```
4.  새 라이브러리 경로로 가상 PLC를 기동하여 연동시킵니다:
    ```bash
    ./vPlc ./my_logic.dylib
    ```
    *가상 PLC의 `PlcLoader`가 라이브러리 내 전역 기호(`__IX0_0`, `__QX0_0` 등) 및 라이프사이클(`config_init__`, `config_run__`)을 동적으로 수집 바인딩하여 자동으로 PLC 스캔 타이밍 엔진에 병합시킵니다.*
