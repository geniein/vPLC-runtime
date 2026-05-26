# vPLC 매핑 엔진(Mapping Engine) 및 PLC별 메모리 영역 상세 가이드

본 문서는 가상 PLC(vPLC) C++ 런타임에 탑재된 핵심 컴포넌트인 **다이나믹 매핑 엔진(Dynamic Mapping Engine)**의 작동 원리와 사용 방법, 그리고 이 기종 PLC(Siemens, Mitsubishi, LS Electric, Modbus)별 고유 메모리 영역(Area) 및 데이터 타입 명세를 다룹니다.

---

## 1. 다이나믹 매핑 엔진 개요

vPLC의 매핑 엔진은 이기종 산업용 필드버스 장비 간의 메모리 연동을 가상화 환경에서 투명하게 연결해 줍니다. 예를 들어, **Siemens S7의 DB1 데이터 블록의 특정 오프셋 값을 Mitsubishi MC 프로토콜의 D0 영역에 실시간 연동**하고 싶을 때, 소스 코드 수정 없이 간단한 JSON 설정 파일 변경만으로 이를 달성할 수 있습니다.

### 🔄 양방향 실시간 동기화 (Bidirectional Sync)
매핑 엔진은 단순한 단방향 전파가 아닌, **고정밀 양방향 전파(Bidirectional Propagation)**를 지원합니다.
- **소스(src) 값 변경 시**: 자동으로 해당 값이 대상(dst) 주소로 전파됩니다.
- **대상(dst) 값 변경 시**: 자동으로 해당 값이 소스(src) 주소로 역전파됩니다.
- **20ms 스캔 주기 동기화**: PLC의 연산 스케줄러 사이클(`PlcScheduler`)의 최우선 순위 스텝으로 `syncMappings()` 가 매 스캔(20ms) 마다 호출되어 스레드 안전(Thread-Safe)하게 동기화를 보장합니다.

---

## 2. 매핑 주소 표기법 (Address Notation & Grammar)

매핑 엔진은 주소를 파싱할 때 점(`.`) 표기법(Dot Notation)을 사용하며, 문법 구조는 다음과 같습니다:

$$\text{PROTOCOL.AREA.INDEX} \quad \text{또는} \quad \text{PROTOCOL.AREA.SUB\_TYPE.INDEX}$$

### 주소 구성 요소
1. **PROTOCOL**: 대상 프로토콜 엔진 규격 (`S7`, `MC`, `LS`, `MODBUS`)
2. **AREA**: 해당 프로토콜에서 지원하는 물리/가상 레지스터 영역 (예: `DB1`, `D`, `M`, `PE`, `PA`)
3. **SUB_TYPE** (선택 사항): 비트/워드 구분 등 특정 하위 지시자 (예: `W`)
4. **INDEX**: 10진수로 표현되는 메모리 오프셋/인덱스 번호 (0-indexed)

---

## 3. PLC 제조사별 지원 메모리 맵 및 데이터 타입 규격

vPLC에 탑재된 스레드 세이프 메모리 맵(`PlcMemory`)은 전 세계 산업 표준 PLC의 주소 체계를 아래와 같이 완벽히 지원합니다.

### 🇩🇪 Siemens S7 (Snap7 호환)
Siemens S7comm 프로토콜은 데이터 블록(DB) 및 입출력 이미지 영역을 시뮬레이션합니다.

| 표기법 (AREA) | 물리적 의미 | 데이터 타입 | 매핑 주소 예시 |
| :--- | :--- | :--- | :--- |
| **`PE`** | Process Input (입력 비트 이미지) | Bit (0 또는 1) | `S7.PE.0` (첫 번째 입력 비트) |
| **`PA`** | Process Output (출력 비트 이미지) | Bit (0 또는 1) | `S7.PA.3` (네 번째 출력 비트) |
| **`DB1`** | Data Block 1 (데이터 블록 1) | Word (16비트 정수) | `S7.DB1.W.0` 또는 `S7.DB1.0` (DB1.DBW0) |
| **`DB2`** | Data Block 2 (데이터 블록 2) | Word (16비트 정수) | `S7.DB2.W.10` (DB2.DBW20) |

---

### 🇯🇵 Mitsubishi MELSEC MC (3E Frame Binary 호환)
MELSEC MC 프로토콜은 디바이스 코드별 비트(Bit)와 워드(Word) 디바이스 영역을 풍부하게 파싱합니다.

#### 1) 비트 디바이스 영역 (Bit Device Area)
*   모두 비트(Bit) 데이터 타입(0 또는 1)을 사용합니다.

| 표기법 (AREA) | 의미 | 매핑 주소 예시 |
| :--- | :--- | :--- |
| **`X`** | Input (입력 비트) | `MC.X.0` (X000) |
| **`Y`** | Output (출력 비트) | `MC.Y.2` (Y002) |
| **`M`** | Internal Relay (내부 릴레이) | `MC.M.100` (M100) |
| **`L`** | Latch Relay (래치 릴레이) | `MC.L.10` (L10) |
| **`B`** | Link Relay (링크 릴레이) | `MC.B.32` (B32) |
| **`F`** | Annunciator (경보 표시기) | `MC.F.5` (F5) |
| **`SM`** | Special Relay (특수 릴레이) | `MC.SM.8000` (SM8000) |

#### 2) 워드 디바이스 영역 (Word Device Area)
*   모두 16비트 정수(Word) 데이터 타입을 사용합니다.

| 표기법 (AREA) | 의미 | 매핑 주소 예시 |
| :--- | :--- | :--- |
| **`D`** | Data Register (데이터 레지스터) | `MC.D.0` (D0), `MC.D.100` (D100) |
| **`W`** | Link Register (링크 레지스터) | `MC.W.10` (W10) |
| **`R`** | File Register (파일 레지스터) | `MC.R.500` (R500) |
| **`ZR`** | Expansion File Register (확장 파일 레지스터) | `MC.ZR.200` (ZR200) |
| **`SD`** | Special Register (특수 레지스터) | `MC.SD.200` (SD200) |

---

### 🇰🇷 LS Electric XGT (Dedicated FEnet 호환)
LS Electric XGT 전용 프로토콜은 국산 스마트 팩토리 제어망에서 널리 쓰이는 비트와 워드 영역을 지원합니다.

| 표기법 (AREA) | 물리적 의미 | 데이터 타입 | 매핑 주소 예시 |
| :--- | :--- | :--- | :--- |
| **`I`** | Input Relay (입력 비트 릴레이) | Bit (0 또는 1) | `LS.I.0` (%IX0.0.0) |
| **`Q`** | Output Relay (출력 비트 릴레이) | Bit (0 또는 1) | `LS.Q.16` (%QX0.1.0) |
| **`M`** | Auxiliary Relay (보조 비트 릴레이) | Bit (0 또는 1) | `LS.M.0` (%MX0) |
| **`W`** | Word Data Register (워드 데이터 레지스터) | Word (16비트 정수) | `LS.W.1` (%MW1 - 탱크 설정값 등) |

---

### 🌐 Modbus (공통 산업용 필드버스)
가장 보편적인 Modbus 표준 4대 메모리 맵 영역입니다.

| 표기법 (AREA) | 하위 별칭 | 데이터 타입 | 매핑 주소 예시 |
| :--- | :--- | :--- | :--- |
| **`COIL`** | `C` | Coil Status (Bit) | `MODBUS.COIL.0` (Coil 00001) |
| **`DISCRETE_INPUT`** | `DI`, `I` | Discrete Input (Bit) | `MODBUS.DI.5` (Discrete Input 10006) |
| **`INPUT_REGISTER`** | `IR` | Input Register (Word) | `MODBUS.IR.1` (Input Register 30002) |
| **`HOLDING_REGISTER`** | `HR` | Holding Register (Word) | `MODBUS.HR.0` (Holding Register 40001) |

---

## 4. 실전 매핑 설정 예시 (`mappings.json`)

매핑 정보는 JSON 배열 형태로 구성되며, 각 객체는 `src`와 `dst` 한 쌍의 매핑 규칙을 정의합니다.

### 예시: `mappings.json`
```json
[
  {
    "comment": "1. Siemens S7 출력 비트 0번이 바뀌면 Mitsubishi 출력 비트 0번으로 양방향 전파",
    "src": "S7.PA.0",
    "dst": "MC.Y.0"
  },
  {
    "comment": "2. Siemens S7 DB1의 Word 0번 값을 Mitsubishi 데이터 레지스터 D0과 실시간 연동",
    "src": "S7.DB1.W.0",
    "dst": "MC.D.0"
  },
  {
    "comment": "3. LS Electric XGT 워드 영역 1번(설정 수위)과 Modbus Holding Register 1번 양방향 동기화",
    "src": "LS.W.1",
    "dst": "MODBUS.HR.1"
  },
  {
    "comment": "4. Modbus DI 0번 비트를 LS Electric 출력 비트 Q 0번에 매핑",
    "src": "MODBUS.DI.0",
    "dst": "LS.Q.0"
  }
]
```

---

## 5. 매핑엔진 실시간 API 및 핫 리로드 (REST API Guide)

vPLC의 임베디드 웹 서버(기본 Port `8080`)는 매핑 규칙을 실시간으로 핫 리로드할 수 있는 고성능 REST API 엔드포인트를 제공합니다. 이를 통해 vPLC를 정지 및 재부팅하지 않고도 런타임 상에서 실시간으로 이기종 매핑 규칙을 유연하게 교체할 수 있습니다.

### 1) 현재 등록된 모든 매핑 규칙 조회 (GET)
- **URL**: `http://localhost:8080/api/mappings`
- **Method**: `GET`
- **Response**: 현재 메모리 엔진에 탑재되어 실행 중인 매핑 목록 (JSON 형식)

#### cURL 실행 예시
```bash
curl -s http://localhost:8080/api/mappings
```

### 2) 새로운 매핑 규칙 주입 및 핫 리로드 (POST)
- **URL**: `http://localhost:8080/api/mappings`
- **Method**: `POST`
- **Headers**: `Content-Type: application/json`
- **Body**: 신규 주입할 매핑 JSON 데이터 배열

#### cURL 실행 예시
```bash
curl -X POST -H "Content-Type: application/json" -d '[
  {"src": "S7.DB1.W.1", "dst": "MC.D.1"},
  {"src": "LS.Q.0", "dst": "MC.Y.0"}
]' http://localhost:8080/api/mappings
```
*   **성공 시**: 서버 로그에 `[PlcMemory] Successfully loaded 2 dynamic mapping rules from POST request` 가 기록되며, 20ms 주기 연산 루프에 새 규칙이 지연 없이 즉시 반영됩니다.

---

## 6. 주의 사항 및 권장 설계 기법
- **동일한 dst 중복 금지**: 한 개의 dst(대상)에 여러 개의 src(소스)를 매핑하면, 20ms 주기 동기화 과정에서 서로 덮어쓰는 피드백 루프(값의 롤백 또는 발진 현상)가 발생할 수 있습니다. 한 주소는 오직 한 방향의 매핑 규칙만 가지도록 정렬하십시오.
- **데이터 타입 일치 권장**: Bit 영역(`PE`, `PA`, `X`, `Y`, `COIL` 등)은 반드시 Bit 영역끼리 매핑하고, Word 영역(`DB`, `D`, `W`, `HR` 등)은 Word 영역끼리 매핑하여 값의 일관성을 유지하십시오. (워드 값을 비트 영역에 매핑 시 `0`이 아니면 `1`로 치환 파싱됩니다.)
