# 택트 스위치 4개 테스트 — 비트필드(레지스터 직접 제어) 버전 — TMS320F28P659DK8-Q1

[6_GP_Tactile4_Driverlib](../6_GP_Tactile4_Driverlib/README.md)(DriverLib 버전)와 동일한 동작을
**DriverLib을 전혀 쓰지 않고** TI의 클래식 레지스터 비트필드 구조체(`GpioDataRegs`)만으로
구현했습니다. `driverlib.lib` 자체가 이 프로젝트에는 없습니다.

## 이 버전의 핵심
- 시스템 초기화: `InitSysCtrl()` (DriverLib의 `Device_init()`에 대응, 클래식 헬퍼)
- GPIO 입력 설정: `GPIO_SetupPinMux(gpio, GPIO_MUX_CPU1, 0)` + `GPIO_SetupPinOptions(gpio,
  GPIO_INPUT, GPIO_PULLUP)` — DriverLib 버전의 `GPIO_setPinConfig()`/`GPIO_setDirectionMode()`/
  `GPIO_setPadConfig()` 조합과 대응.
- **스위치 상태 읽기**: `GpioDataRegs.GPCDAT.all`(GPIO64~95를 담당하는 32비트 레지스터)을
  루프마다 한 번만 읽고 비트 시프트로 4개 채널을 한꺼번에 뽑아냅니다 — DriverLib 버전의
  `GPIO_readPin()`을 채널마다 4번 호출하는 것과 실질적으로 같은 결과지만, 레지스터를 직접
  다루면 이렇게 한 번에 처리할 수도 있다는 걸 보여주는 지점입니다.

## 요구 하드웨어 / 배선
[6_GP_Tactile4_Driverlib](../6_GP_Tactile4_Driverlib/README.md)와 완전히 동일. **v1.10부터
스위치 번호와 GPIO 대응이 핀 순서와 반대입니다** — 점퍼선 꼬임 방지 목적:

| 스위치 | GPIO | A Side 핀 |
|---|---|---|
| 1번 | GPIO70 | 29번 |
| 2번 | GPIO69 | 27번 |
| 3번 | GPIO68 | 25번 |
| 4번 | GPIO67 | 23번 |

## 소프트웨어 버전
CCS 21.x / **SysConfig·C2000Ware 라이브러리 링크 없음** (products="C2000WARE"만 사용) /
CGT 22.6.3.LTS. `device/common_include`, `device/headers_include`에 필요한 클래식 헤더를
전부 복사해 자기완결형으로 만들었습니다.

## 동작 원리

### 1. GPIO를 입력으로 설정하기
`GPIO_SetupPinOptions(gpio, GPIO_INPUT, GPIO_PULLUP)`가 하는 일은 그 GPIO 핀의
방향 레지스터(`GPyDIR`)에서 해당 비트를 0으로 클리어하는 것뿐입니다 — 방향이
"입력"이면 CPU는 그 핀의 전압을 읽기만 하고, 자기가 그 핀에 전압을 실어보내지
않습니다(하이 임피던스). 여기에 내부 풀업(`GPIO_PULLUP`)까지 켜서, 혹시 점퍼선이
빠지거나 스위치 회로가 아예 연결 안 된 상태에서도 핀이 전기적으로 "붕 뜬
(Floating)" 채로 남지 않고 3.3V 쪽으로 살짝 당겨지도록 안전장치를 둡니다 — 붕 뜬
입력 핀은 주변 노이즈를 주워서 랜덤하게 0/1이 튀는 오작동의 흔한 원인입니다.

### 2. Active High와 "카운트"의 의미
개발보드의 (6) Tactile 스위치 회로는 스위치를 누르면 그 핀에 3.3V(로직 1)가,
안 누르면 0V(로직 0)가 걸리도록 설계돼 있습니다 — 이게 **Active High**입니다
(반대로 누르면 0V가 되는 회로는 Active Low). 그래서 `GpioDataRegs.GPCDAT.all`에서
뽑아낸 값이 1이면 "지금 눌려 있다"는 뜻입니다.

다만 `state`(지금 이 순간 눌려 있는지)와 `pressCount`(몇 번 눌렀는지)는 다른
개념입니다. 매 폴링 주기(10msec)마다 그냥 "지금 1이다"라고 카운트를 올리면,
스위치를 1초 누르고 있는 동안 100번(10msec × 100 = 1초) 카운트가 올라가 버립니다.
그래서 "방금 막 0에서 1로 바뀐 순간"만 잡아내는 **에지 검출(Edge Detection)**을
씁니다:

```c
risingEdges = state & (Uint16)(~prevState);   // 이전엔 0이었는데 지금은 1인 비트만 남김
```

이렇게 하면 스위치를 누르고 있는 동안 계속 1이어도 `pressCount`는 "누르는 순간"
딱 한 번만 올라갑니다.

### 3. 왜 한 번 눌렀는데 여러 번 카운트될 수 있는가 — 스위치 바운스(Bounce)
에지 검출을 쓰는데도 `pressCount`가 한 번의 물리적 클릭에 2~5번씩 잡히는 걸
보실 수 있습니다. 코드 버그가 아니라 **기계식 스위치의 물리적 특성** 때문입니다
— 스위치 내부 금속 접점이 닫히는 순간 완벽하게 한 번에 붙는 게 아니라, 수 msec
동안 미세하게 튕기면서(Bounce) 붙었다 떨어졌다를 여러 번 반복합니다. 이 예제는
10msec마다 폴링하는데, 바운스가 보통 1~10msec 정도 지속되기 때문에 그 튕기는
구간에서 0→1→0→1이 여러 번 잡혀 `pressCount`가 과다 계수됩니다.

실제 제품에서 쓰는 대표적인 디바운스(Debounce) 방법:
- **시간 기반**: 에지를 한 번 잡으면, 그 뒤 일정 시간(예: 20~50msec) 동안은 같은
  핀의 변화를 무시합니다 — 바운스가 끝날 시간을 벌어주는 방식.
- **연속 샘플 확인**: 값이 바뀐 걸 감지해도 바로 확정하지 않고, N번 연속(예: 3~5회)
  같은 값이 나올 때만 "진짜 바뀌었다"고 인정합니다.
- **하드웨어 디바운스**: 스위치 회로 자체에 RC 필터+슈미트 트리거를 넣어 바운스를
  전기적으로 걸러내는 방법(가장 깔끔하지만 회로 수정이 필요).

이 예제는 일부러 디바운스를 빼서 "에지 검출만 있을 때 실제로 어떤 문제가 생기는지"를
직접 관찰할 수 있게 했습니다 — 정식 디바운스 구현은 이후 예제에서 다룰 예정입니다.

## Import → Build → Flash → Run
1. CCS에서 `CCS/6_GP_Tactile4_Bitfield.projectspec`를 Import
2. Build (CPU1_RAM 또는 CPU1_FLASH)
3. Debug 연결 후 Flash/Run — `.ccxml`은 `TMS320F28P650DK9.ccxml` 사용 (DK8-Q1 최초 연결 시
   정상 인식 확인 필요)

## 정상 동작 확인
DriverLib 버전과 동일하게, 4개 택트 스위치를 누를 때마다 스위치 옆 LED Indicator가 즉시
반응하면 배선이 맞는 것입니다. `tactSwitchState`(비트0=1번 스위치=GPIO70 ~ 비트3=4번 스위치=GPIO67), `pressCount`도
CCS Expressions에서 동일하게 확인 가능합니다. 디바운스 처리는 하지 않아 `pressCount`가
한 번의 물리적 클릭에 여러 번 잡힐 수 있습니다 — 의도된 단순화입니다.

## 세 가지 버전 비교
| 버전 | 폴더 | 핵심 차이 |
|---|---|---|
| DriverLib | [6_GP_Tactile4_Driverlib](../6_GP_Tactile4_Driverlib/) | TI 표준 HAL 함수 호출 |
| **비트필드(이 폴더)** | 6_GP_Tactile4_Bitfield | 레지스터 구조체 직접 조작, DriverLib 미사용 |
| FreeRTOS | [6_GP_Tactile4_Freertos](../6_GP_Tactile4_Freertos/) | DriverLib + 태스크 스케줄링, 정적 할당 |

### 메모리 실측 비교 (2026-09-16, CCS 21.x, CPU1_RAM 빌드, `.map` 기준)

세 프로젝트 모두 `workspace_ccstheia`에서 실제로 Import → Build까지 성공한 뒤의 `.map`
파일 MODULE SUMMARY "Grand Total"(code/ro data/rw data) 기준 실측치입니다.

| 버전 | code | ro data | rw data | 합계 |
|---|---:|---:|---:|---:|
| DriverLib | 2,968 B | 603 B | 1,030 B (스택 1,016B) | **4,601 B** |
| **비트필드(이 폴더)** | 3,801 B | 478 B | 1,685 B (스택 256B) | **5,964 B** |
| FreeRTOS | 5,084 B | 820 B | 1,239 B (RTS 스택 512B + 태스크 정적 스택 512B + 전역변수 215B) | **7,143 B** |

비트필드 버전의 rw data(1,685B)가 유독 큰 건 태스크 코드 때문이 아니라, 클래식
`f28p65x_globalvariabledefs.c`가 `GpioCtrlRegsFile`/`CpuSysRegsFile` 등 주변장치
레지스터 구조체 전체를 예제가 실제로 쓰든 안 쓰든 항상 전역 심볼로 선언해서 링커가
"rw data"로 잡기 때문입니다(실제로 추가 RAM을 소비하는 게 아니라 이미 있는 레지스터
주소를 심볼로 덮어씌운 것 — 물리적으로는 공짜입니다). DriverLib/FreeRTOS 버전은 이런
이름 붙은 전역 구조체 없이 주소 매크로만 쓰기 때문에 이 항목이 없습니다.

## 관련 링크
- 상품 페이지: https://tms320f28x.co.kr/goods/goods_view.php?goodsNo=200903127
- 게시판 글: (게시 후 URL 추가 예정)
- 유튜브 영상: (게시 후 URL 추가 예정)
