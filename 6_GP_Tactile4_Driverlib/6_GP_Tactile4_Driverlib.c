// 파일이름	:	6_GP_Tactile4_Driverlib.c
// 대상장치	:	TMS320F28X EVM V2, TMS320F28P65x(F28P650DK9 산업용 / F28P659DK8-Q1 차량용) 모듈
// 파일버전	:	1.10
// 갱신이력	:	2026-09-15, 버전 1.00
//				2026-09-16, 버전 1.10 - 점퍼선 꼬임 방지를 위해 스위치 번호-GPIO 대응을
//				반대로 변경 (1번=GPIO70, 2번=GPIO69, 3번=GPIO68, 4번=GPIO67)
// 예제설명	:

//************************************************************************************************************************************************************************
//
// 본 예제는 TMS320F28P65x 모듈이 탑재된 TMS320F28X 개발보드(EVM) V2를 대상으로 하고 있으며,
// TMS320F28P65x 칩의 GPI/O 회로를 통해 개발보드의 범용 Tactile 스위치 4개를 읽어냅니다.
//
// TMS320F28X 개발보드 V2의 '(6) 범용 Tactile 스위치 4개' 회로는 스위치를 누르면 Active High
// 신호가 출력되도록 구성되어 있습니다(개발보드 상 점퍼로 Active High/Low 선택 가능).
//
// a.	예제 실행을 위해 아래와 같이 개발보드 Tactile 스위치 회로의 핀-헤더와
//		TMS320F28P65x 칩의 GPIO 포트를 연결하세요.
//
//			>> 1번 스위치 : GPIO 70번 (A Side 29번 핀)
//			>> 2번 스위치 : GPIO 69번 (A Side 27번 핀)
//			>> 3번 스위치 : GPIO 68번 (A Side 25번 핀)
//			>> 4번 스위치 : GPIO 67번 (A Side 23번 핀)
//
//		(핀 번호와 스위치 번호가 반대 순서인 이유: A Side 23,25,27,29번 핀 순서
//		그대로 1,2,3,4번을 배정하면 점퍼선 4개가 서로 엇갈려서 꼬입니다. 반대로
//		배정하면 1번 스위치 점퍼선이 가장 먼 29번 핀에서 시작해 안쪽으로 나란히
//		들어오므로 선이 꼬이지 않습니다.)
//
// 예제 폴더의 CCS Expressions 창에 아래 변수를 등록해두면 동작을 실시간으로 확인할 수 있습니다.
// --> tactSwitchState(4개 스위치 상태를 비트0(1번 스위치=GPIO70)~비트3(4번 스위치=GPIO67)로 담은 값),
//     pressCount(스위치가 눌린(0→1로 바뀐) 총 횟수, 디바운스 없는 단순 에지 검출)
//
// 본 예제는 TI가 제공하는 Driver API Library(DriverLib) 코드만으로 제작되었습니다
// (SysConfig 미사용). 같은 동작을 레지스터를 직접 조작하는 Bit-Field 방식으로 구현한
// 6_GP_Tactile4_Bitfield.c, FreeRTOS 태스크로 구현한 6_GP_Tactile4_Freertos.c와 함께 세 가지 방식을
// 비교해 보실 수 있습니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "driverlib.h"		// TI 제공 Driver API Library 헤더파일 (driverlib)
#include "device.h"

// 전처리 구문 정의
#define	NUM_SWITCHES	4U		// 읽어들일 스위치 개수

// 함수 원형 선언
void initSwitchGpio(void);

// 전역 변수 선언
volatile uint16_t	tactSwitchState;	// 4개 스위치 상태를 비트0(1번 스위치=GPIO70)~비트3(4번 스위치=GPIO67)로 담은 값
volatile uint32_t	pressCount;			// 스위치가 눌린(0->1로 바뀐) 총 횟수 (에지 검출)

// 이 배열의 인덱스 = 비트 번호(스위치 번호-1), 값 = GPIO 번호. 개발보드 A Side 핀-헤더
// 23,25,27,29번(GPIO67~70)과 일부러 반대 순서로 배정했습니다 - 점퍼선 꼬임 방지(파일
// 상단 배선 설명 참고).
static const uint16_t switchGpio[NUM_SWITCHES] =
{
    70U, 69U, 68U, 67U
};


// 메인 함수
void main(void)
{
	uint16_t prevState = 0U;

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
//	   (DriverLib에서는 별도 처리 없이, 3번의 Interrupt_initModule( ) 함수가 한 번에
//	   담당합니다 — 아래 3번 참고)


//	2. 시스템 초기화 - Device_init( ) 함수 호출
//	* 왓치독 타이머 비-활성화
//	* CPU 클럭 주파수 설정 (PLL)
//	* 주변회로 클럭 공급 설정
	Device_init();

//	* 범용 입출력 포트(GPIO) 핀 락 해제, 내부 풀업 활성화 - Device_initGPIO( ) 함수 호출
	Device_initGPIO();


//	3. 주변회로 인터럽트 확장회로 초기화 - Interrupt_initModule( ) 함수 호출
//	   (전역 인터럽트 비활성화 + PIE 인터럽트 인에이블/플래그 클리어까지 이 함수 하나로
//	   처리합니다 — 클래식 Bit-Field 버전의 1번+3번을 합친 것과 같습니다)
	Interrupt_initModule();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - Interrupt_initVectorTable( ) 함수 호출
	Interrupt_initVectorTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 재-연결, 인터럽트 벡터 활성화
//	   (본 예제는 인터럽트를 사용하지 않습니다)


//	6. 주변회로 초기화 - GPIO67~GPIO70을 입력으로 설정, initSwitchGpio( ) 함수 호출 (본 파일 하단)
	initSwitchGpio();


//	7. 전역 변수 및 S/W 모듈 초기화
	tactSwitchState = 0U;
	pressCount = 0U;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정 (실시간 디버깅이 가능하도록 ST1 레지스터의 /DBGM 비트를 0으로 클리어)
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. Idle(Background) Loop
	for(;;)
	{
		uint16_t i;
		uint16_t state = 0U;
		uint16_t risingEdges;

		for(i = 0U; i < NUM_SWITCHES; i++)
		{
			if(GPIO_readPin(switchGpio[i]) != 0U)
			{
				state |= (uint16_t)(1U << i);
			}
		}

		//
		// 0->1로 새로 바뀐 비트만 골라 눌림 횟수 누적 (아주 단순한 에지 검출,
		// 디바운스는 하지 않으므로 채터링이 여러 번의 카운트로 잡힐 수 있음)
		//
		risingEdges = (uint16_t)(state & (uint16_t)(~prevState));
		for(i = 0U; i < NUM_SWITCHES; i++)
		{
			if((risingEdges & (uint16_t)(1U << i)) != 0U)
			{
				pressCount++;
			}
		}

		tactSwitchState = state;
		prevState = state;

		DEVICE_DELAY_US(10000U);	// 10msec 지연 (폴링 주기)
	}
}

//	10. 인터럽트 서비스 루틴 및 기타 함수들
//	    (본 예제는 인터럽트 서비스 루틴이 없습니다)

//
// initSwitchGpio - GPIO67~GPIO70을 전부 입력(내부 풀업, 동기 입력)으로 설정
//
void initSwitchGpio(void)
{
    GPIO_setPadConfig(67U, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPinConfig(GPIO_67_GPIO67);
    GPIO_setDirectionMode(67U, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(67U, GPIO_QUAL_SYNC);

    GPIO_setPadConfig(68U, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPinConfig(GPIO_68_GPIO68);
    GPIO_setDirectionMode(68U, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(68U, GPIO_QUAL_SYNC);

    GPIO_setPadConfig(69U, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPinConfig(GPIO_69_GPIO69);
    GPIO_setDirectionMode(69U, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(69U, GPIO_QUAL_SYNC);

    GPIO_setPadConfig(70U, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPinConfig(GPIO_70_GPIO70);
    GPIO_setDirectionMode(70U, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(70U, GPIO_QUAL_SYNC);
}

// 파일 끝.
