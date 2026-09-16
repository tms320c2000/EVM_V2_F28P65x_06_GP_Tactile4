// 파일이름	:	6_GP_Tactile4_Bitfield.c
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
// 본 예제는 28X 칩 MMR(Memory Mapped Register)을 직접 조작하는 TI의 Bit-Field Approach
// 만으로 제작되었습니다 (Driverlib 미사용, driverlib.lib 자체가 이 프로젝트에는 없습니다).
// 같은 동작을 DriverLib API로 구현한 6_GP_Tactile4_Driverlib.c, FreeRTOS 태스크로 구현한
// 6_GP_Tactile4_Freertos.c와 함께 세 가지 방식을 비교해 보실 수 있습니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "f28x_project.h"		// TI 제공 칩-지원 헤더 통합 Include 용 헤더파일 (bit-field)

// 전처리 구문 정의
#define	NUM_SWITCHES	4U		// 읽어들일 스위치 개수
#define	GPC_BASE_GPIO	64U		// GpioDataRegs.GPCDAT가 담당하는 첫 GPIO 번호(GPIO64~95)

// 함수 원형 선언


// 전역 변수 선언
volatile Uint16	tactSwitchState;	// 4개 스위치 상태를 비트0(1번 스위치=GPIO70)~비트3(4번 스위치=GPIO67)로 담은 값
volatile Uint32	pressCount;			// 스위치가 눌린(0->1로 바뀐) 총 횟수 (에지 검출)

// 이 배열의 인덱스 = 비트 번호(스위치 번호-1), 값 = GPIO 번호. 개발보드 A Side 핀-헤더
// 23,25,27,29번(GPIO67~70)과 일부러 반대 순서로 배정했습니다 - 점퍼선 꼬임 방지(파일
// 상단 배선 설명 참고). GPIO67~70은 모두 GpioDataRegs.GPCDAT(GPIO64~95) 한 레지스터
// 안에 들어있어서, 루프마다 .all을 한 번만 읽고 비트를 뽑아 쓰면 충분합니다.
static const Uint16 switchGpio[NUM_SWITCHES] =
{
    70U, 69U, 68U, 67U
};


// 메인 함수
void main(void)
{
	Uint16 prevState = 0U;

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
	DINT;			// 전역 인터럽트 스위치 OFF (/INTM OFF)
	IER = 0x0000;	// CPU 인터럽트 벡터 비-활성화
	IFR = 0x0000;	// CPU 인터럽트 플래그 클리어


//	2. 시스템 초기화 - InitSysCtrl( ) 함수 호출 (f28p65x_sysctrl.c)
//	* 왓치독 타이머 비-활성화
//	* CPU 클럭 주파수 설정 (PLL)
//	* 주변회로 클럭 공급 설정
	InitSysCtrl();

//	* 범용 입출력 포트(GPIO) 설정 - InitGpio( ) 함수 호출 후, 스위치 4개용 GPIO67~70을
//	  하나씩 입력(내부 풀업, 동기 입력)으로 개별 설정
	{
	    Uint16 i;
	    InitGpio();
	    for(i = 0U; i < NUM_SWITCHES; i++)
	    {
	        GPIO_SetupPinMux(switchGpio[i], GPIO_MUX_CPU1, 0);
	        GPIO_SetupPinOptions(switchGpio[i], GPIO_INPUT, GPIO_PULLUP);
	    }
	}


//	3. 주변회로 인터럽트 확장회로 초기화 - InitPieCtrl( ) 함수 호출 (f28p65x_piectrl.c)
	InitPieCtrl();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - InitPieVectTable( ) 함수 호출 (f28p65x_pievect.c)
	InitPieVectTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 재-연결, 인터럽트 벡터 활성화
//	   (본 예제는 인터럽트를 사용하지 않습니다)


//	6. 주변회로 초기화
//	   (스위치용 GPIO 설정은 2번에서 이미 처리했으므로 생략)


//	7. 전역 변수 및 S/W 모듈 초기화
	tactSwitchState = 0U;
	pressCount = 0UL;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정 (실시간 디버깅이 가능하도록 ST1 레지스터의 /DBGM 비트를 0으로 클리어)
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. Idle(Background) Loop
	for(;;)
	{
		Uint16 i;
		Uint16 state = 0U;
		Uint16 risingEdges;
		Uint32 gpcdat = GpioDataRegs.GPCDAT.all;	// GPIO64~95를 한 번에 읽음

		for(i = 0U; i < NUM_SWITCHES; i++)
		{
			if((gpcdat & (1UL << (switchGpio[i] - GPC_BASE_GPIO))) != 0UL)
			{
				state |= (Uint16)(1U << i);
			}
		}

		//
		// 0->1로 새로 바뀐 비트만 골라 눌림 횟수 누적 (아주 단순한 에지 검출,
		// 디바운스는 하지 않으므로 채터링이 여러 번의 카운트로 잡힐 수 있음)
		//
		risingEdges = (Uint16)(state & (Uint16)(~prevState));
		for(i = 0U; i < NUM_SWITCHES; i++)
		{
			if((risingEdges & (Uint16)(1U << i)) != 0U)
			{
				pressCount++;
			}
		}

		tactSwitchState = state;
		prevState = state;

		DELAY_US(10000);	// 10msec 지연 (폴링 주기)
	}
}

//	10. 인터럽트 서비스 루틴 및 기타 함수들
//	    (본 예제는 인터럽트 서비스 루틴 및 별도 함수가 없습니다)


// 파일 끝.
