# 가상 PLC (vPLC) C++ 런타임 엔진 및 IIoT 디지털 트윈 시뮬레이터

Modern C++ (C++17)로 정교하게 설계 및 구현된 고성능 **가상 PLC (Virtual Programmable Logic Controller) 런타임 에뮬레이터**입니다. 

실제 하드웨어 PLC 장비 없이도 PLC 코어 메모리 관리, 20ms 고정밀 스캔 주기 타이밍 제어, 외부 제어 로직 공유 라이브러리(`.dylib` / `.so`)의 런타임 동적 핫 리로드를 완벽하게 수행합니다.

특히 전 세계 산업용 4대 표준 필드버스 서버 규격인 **Modbus TCP(5020), Siemens S7(1020), Mitsubishi MC(5011), LS Electric XGT(2004)** 및 **IIoT MQTT Telemetry Publisher**를 내재화하여, 공장 현장 제어 시스템부터 스마트 팩토리 클라우드 수집 환경까지 그대로 재현하는 최상급 IIoT 디지털 트윈 시뮬레이터로 기획되었습니다.

---

## 🚀 주요 특징 (Key Features)

*   **멀티프로토콜 동시 기동 (4+1 통신 규격)**:
    *   **Modbus TCP** (기본 Port `5020`)
    *   **Siemens S7comm** (기본 Port `1020`)
    *   **Mitsubishi MELSEC MC 3E Frame** (기본 Port `5011`)
    *   **LS Electric XGT Dedicated FEnet** (기본 Port `2004`)
    *   **MQTT Telemetry Publisher** (기본 Port `1883` 브로커 연동, JSON 방송)
*   **스레드 세이프 메모리 맵 (`PlcMemory`)**: C++17 `std::shared_mutex` 독점/공유 락을 활용하여 4대 필드버스 서버 스레드, PLC 연산 스레드, TUI 스레드 간 충돌 없는 동시 읽기/쓰기를 보장합니다.
*   **고정밀 스케줄러 (`PlcScheduler`)**: 누적 시간 편차(Drift)를 지속 감쇄하는 알고리즘을 도입하여 20ms 사이클 오차(Jitter) 및 연산 스캔 타임을 마이크로초 단위로 정밀하게 추적합니다.
*   **플러그인 로직 로더 (`PlcLoader`)**: dlopen 기반 동적 리로드 설계로, 두 가지 대표적인 리얼 타임 시뮬레이터(물탱크/자동차 조립)를 소스 교체 없이 스위칭 로딩합니다.
*   **선택적 서버 기동 필터 (`-p, --protocols`)**: 필요한 통신 프로토콜만 선별 기동하여 리소스를 최적화하거나 포트 충돌을 회피합니다.
*   **포트 오프셋 다중 인스턴스 (`-o, --port-offset`)**: 단일 플래그로 4대 포트를 통째로 오프셋 이동하여 한 머신에서 다중 vPLC 인스턴스를 충돌 없이 안전하게 가동합니다.
*   **더블 버퍼링 ANSI TUI 대시보드 (`PlcTui`)**: 비깜빡임 반응형 터미널 UI 화면을 구현하고, 기동 시 시뮬레이션 모드 및 서버 활성 여부를 감지해 동적 회색(`[ DISABLED ]`) 처리 및 맞춤형 대시보드 화면을 드로잉합니다.

---

## 📂 디렉터리 구조

```text
vPlc/
├── CMakeLists.txt              # CMake 빌드 파일
├── README.md                   # 본 설명서 파일
├── .gitignore                  # 컴파일 바이너리 및 dylib 제외 필터
├── docs/                       # 핵심 상세 가이드 및 설계 산출물 폴더
│   └── mapping_engine_guide.md # 👉 매핑 엔진 & 이기종 PLC 메모리 영역 가이드
├── libmock_logic.dylib         # 물탱크 수위 시뮬레이터 동적 라이브러리
├── libassembly_logic.dylib     # 자동차 조립 라인 시뮬레이터 동적 라이브러리
├── test_s7_tank                # 컴파일된 물탱크용 S7 테스트 바이너리
├── test_s7_assembly            # 컴파일된 자동차용 S7 테스트 바이너리
├── src/
│   ├── main.cpp                # 런타임 기동, 매개변수 스캔 및 컴포넌트 통합 진입점
│   ├── core/
│   │   ├── PlcMemory.hpp/cpp   # 4대 레지스터 영역 관리 및 스레드 락 엔진
│   │   ├── PlcLoader.hpp/cpp   # 동적 공유 라이브러리 로더 및 기호 변수 바인더
│   │   └── PlcScheduler.hpp/cpp# steady_clock 기반 20ms 정밀 스케줄링 엔진
│   ├── modbus/
│   │   └── ModbusServer.hpp/cpp# 멀티스레드 Modbus TCP 서버 엔진
│   ├── s7/
│   │   └── S7Server.hpp/cpp    # Siemens S7 TCP 서버 엔진 (DB1 동기화 지원)
│   ├── mc/
│   │   └── McServer.hpp/cpp    # MELSEC MC 3E Frame Binary 자체 구현 TCP 서버 엔진
│   ├── xgt/
│   │   └── XgtServer.hpp/cpp   # LSIS-XGT Dedicated FEnet 자체 구현 TCP 서버 엔진
│   ├── mqtt/
│   │   └── MqttPublisher.hpp/cpp# libmosquitto 기반 비동기 스레드식 IIoT JSON Publisher
│   ├── tui/
│   │   └── PlcTui.hpp/cpp      # termios 로우 모드 ANSI 터미널 UI 제어 엔진
│   └── logic/
│       ├── mock_logic.cpp      # 물탱크 물리 시뮬레이션 제어 알고리즘 DLL 소스
│       └── assembly_logic.cpp  # 자동차 생산 컨베이어 & 로봇 암 순차 제어(SFC) 알고리즘 DLL 소스
└── scratch/                    # 프로토콜 및 시뮬레이션 맞춤형 실시간 통신 조작 테스트 툴 폴더
    ├── test_tank_modbus.py     # 물탱크 모드 Modbus TCP 테스트
    ├── test_tank_mc.py         # 물탱크 모드 Mitsubishi MC 테스트
    ├── test_tank_xgt.py        # 물탱크 모드 LS XGT 테스트
    ├── test_tank_s7.cpp        # 물탱크 모드 Siemens S7 C++ 테스트 소스
    ├── test_assembly_modbus.py # 자동차 조립 모드 Modbus TCP 실시간 조작 테스트
    ├── test_assembly_mc.py     # 자동차 조립 모드 Mitsubishi MC 실시간 조작 테스트
    ├── test_assembly_xgt.py    # 자동차 조립 모드 LS XGT 실시간 조작 테스트
    └── test_assembly_s7.cpp    # 자동차 조립 모드 Siemens S7 C++ 실시간 조작 테스트 소스
```

---

## 🔄 다이나믹 매핑 엔진 (Dynamic Mapping Engine)

vPLC는 이기종 산업용 필드버스 장비 간의 레지스터를 소스 코드 수정 없이 실시간 연동해 주는 **양방향 다이나믹 매핑 엔진**을 탑재하고 있습니다.
*   **양방향 동기화**: Siemens S7, Mitsubishi MC, LS Electric XGT, Modbus의 비트/워드 메모리 값을 20ms 주기 스캔 타임 안에서 양방향 실시간 동기화합니다.
*   **실시간 핫 리로드**: `mappings.json` 파일을 통한 부팅 시 로드는 물론, 내장된 웹 서버 API(POST `/api/mappings`)를 통해 런타임 중에 매핑 규칙을 실시간 동기화 상태로 핫 리로드할 수 있습니다.

> [!TIP]
> PLC 제조사별 상세 메모리 주소 체계(DB, D, M, PE, PA, W 등) 및 데이터 타입 규격, cURL을 이용한 실시간 핫 리로드 API 활용법은 아래 상세 가이드 문서를 참고하십시오:
> **👉 [vPLC 매핑 엔진 & PLC별 메모리 영역 상세 가이드](file:///home/ingenie/vPLC-runtime/docs/mapping_engine_guide.md)**

---

## 🛠 컴파일 및 빌드 방법 (Compilation)

### 1. 시뮬레이션 공유 라이브러리 컴파일 (macOS용 .dylib)
```bash
clang++ -std=c++17 -dynamiclib -Isrc -I/opt/homebrew/include -o libmock_logic.dylib src/logic/mock_logic.cpp
clang++ -std=c++17 -dynamiclib -Isrc -I/opt/homebrew/include -o libassembly_logic.dylib src/logic/assembly_logic.cpp
```

### 2. 가상 PLC 메인 바이너리 컴파일 (Snap7 및 Mosquitto 동시 링크)
```bash
clang++ -std=c++17 -Wall -Wextra -O3 -pthread -Isrc -I/opt/homebrew/include -L/opt/homebrew/lib -lsnap7 -lmosquitto src/main.cpp src/core/PlcMemory.cpp src/core/PlcLoader.cpp src/core/PlcScheduler.cpp src/modbus/ModbusServer.cpp src/tui/PlcTui.cpp src/s7/S7Server.cpp src/mc/McServer.cpp src/xgt/XgtServer.cpp src/mqtt/MqttPublisher.cpp -o vPlc
```

### 3. Siemens S7 테스트 클라이언트 컴파일
```bash
clang++ -std=c++17 -Isrc -I/opt/homebrew/include -L/opt/homebrew/lib -lsnap7 scratch/test_tank_s7.cpp -o test_s7_tank
clang++ -std=c++17 -Isrc -I/opt/homebrew/include -L/opt/homebrew/lib -lsnap7 scratch/test_assembly_s7.cpp -o test_s7_assembly
```

---

## 🏃 실행 및 매개변수 가이드 (How to Run)

유연한 파라미터 파서가 탑재되어, 스택의 입력 순서나 결합 여부에 관계없이 직관적인 옵션으로 동작 모드를 조합할 수 있습니다.

### 도움말 스크린 확인
```bash
./vPlc --help
```

### 1. 시뮬레이션 모드 선택
*   **물탱크 수위 시뮬레이터 로드 (기본값)**:
    ```bash
    ./vPlc --tank
    ```
*   **자동차 생산 조립 라인 시뮬레이터 로드**:
    ```bash
    ./vPlc --assembly
    ```

### 2. MQTT 원격 데이터 송출 활성화
*   `--mqtt [broker_ip]` 또는 `-m [broker_ip]` 플래그를 추가하면 비동기 스레드로 500ms마다 주요 계측 데이터를 JSON 포맷 토픽으로 발행합니다.
    ```bash
    ./vPlc --assembly --mqtt 127.0.0.1
    ```

### 3. 선택적 통신 서버 기동
*   `-p, --protocols` 플래그를 사용하여 포트 충돌을 피하거나 필요 프로토콜만 명시적으로 가동합니다.
    ```bash
    # S7 및 Modbus 서버만 선별 기동 (MC, XGT는 비활성화되어 포트를 열지 않음)
    ./vPlc --assembly --protocols modbus,s7
    
    # 모든 통신 서버를 비활성화하고 오직 TUI 그래픽 시뮬레이터만 단독 구동
    ./vPlc --protocols none
    ```

### 4. 포트 오프셋 다중 인스턴스 기동
*   `-o, --port-offset` 플래그를 사용해 4대 포트를 통째로 평행 이동하여 다중 vPLC 실행을 지원합니다.
    ```bash
    # 모든 포트 번호에 100을 더해 안전하게 기동 (Modbus: 5120, S7: 1120 등)
    ./vPlc --assembly --port-offset 100
    ```

---

## 🎮 TUI 모니터 실시간 조작법 (Interactive Controls)

터미널창이 대시보드로 변경된 상태에서 터미널을 포커싱한 채 아래의 조작키를 눌러 PLC 계측치를 인터랙티브하게 강제 조정할 수 있습니다.

*   **`[I]` 키**: 디지털 입력 **`%IX0.0`** (Auto Mode / Auto Run) 상태를 실시간 토글합니다.
*   **`[A]` 키**: 물리 상수 값인 **`%MW1`** (Target Set Point / Conveyor Speed) 값을 `+50`만큼 가속/증가시킵니다.
*   **`[Z]` 키**: 물리 상수 값인 **`%MW1`** 값을 `-50`만큼 감속/감소시킵니다.
*   **`[Q]` 키**: 구동 중인 통신 소켓 스레드와 스케줄러를 깨끗하게 소멸하고 터미널 버퍼 환경을 완벽하게 복구하며 **안전하게 shutdown**합니다.

---

## 🧪 실시간 통신 조작 테스트 툴 실행

`scratch/` 폴더 내에 배치된 테스트 툴은 **TUI 상에 다이나믹한 시각적 변화(예: 속도를 크게 올렸을 때 컨베이어 벨트가 눈에 띄게 초고속 회전하는 모션)**를 즉시 관찰할 수 있도록 최적화되어 있습니다. vPLC TUI 대시보드가 구동 중인 상태에서 다른 터미널을 열고 스크립트를 기동합니다.

### 🚗 자동차 조립 시뮬레이터가 켜져 있을 때 (`./vPlc --assembly`)
```bash
# Modbus TCP (Port 5020)
python3 scratch/test_assembly_modbus.py

# Mitsubishi MC (Port 5011)
python3 scratch/test_assembly_mc.py

# LS Electric XGT (Port 2004)
python3 scratch/test_assembly_xgt.py

# Siemens S7comm (Port 1020)
./test_s7_assembly
```

### 🚰 물탱크 시뮬레이터가 켜져 있을 때 (`./vPlc --tank` 또는 `./vPlc`)
```bash
# Modbus TCP (Port 5020)
python3 scratch/test_tank_modbus.py

# Mitsubishi MC (Port 5011)
python3 scratch/test_tank_mc.py

# LS Electric XGT (Port 2004)
python3 scratch/test_tank_xgt.py

# Siemens S7comm (Port 1020)
./test_s7_tank
```

### 📡 MQTT IIoT 실시간 수집 모니터링
vPLC에 `--mqtt` 브로커 옵션을 켜두었다면, 다른 창에서 아래와 같이 토픽을 구독하여 실시간 원격 제어 정보가 예쁜 JSON 페이로드로 계속 송출되는 광경을 관찰할 수 있습니다:
```bash
mosquitto_sub -h 127.0.0.1 -t "vplc/assembly/state"
```
