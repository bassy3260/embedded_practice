#define F_CPU 14745600UL
#include <avr/io.h>
#include <util/delay.h>

#define PWM_LOW  16    // 1단: 약 6.25%
#define PWM_MID  128   // 2단: 약 50%
#define PWM_HIGH 240   // 3단: 약 94%

int main(void)
{
	unsigned char level = 0;              // 현재 단계 (0,1,2)
	unsigned char prev_sw = 1;            // 이전 버튼 상태 (풀업이라 기본 1)
	unsigned char cur_sw;
	unsigned char pwm_table[3] = {PWM_LOW, PWM_MID, PWM_HIGH};

	DDRE |= (1 << PE3);                   // PE3(OC3A) : PWM 출력
	DDRE &= ~(1 << PE7);                  // PE7 : 스위치 입력
	PORTE |= (1 << PE7);                  // PE7 내부 Pull-up

	TCCR3A = (1 << COM3A1) | (1 << WGM30);
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);  // 64분주
	OCR3A = pwm_table[level];             // 초기값 = 1단

	while (1)
	{
		cur_sw = (PINE & (1 << PE7)) ? 1 : 0;   // 현재 버튼 상태 (0=눌림)

		// 버튼이 "안 눌림(1) → 눌림(0)"으로 바뀌는 순간만 잡기
		if (prev_sw == 1 && cur_sw == 0)
		{
			_delay_ms(20);                      // 디바운싱
			if (!(PINE & (1 << PE7)))           // 여전히 눌려있으면 진짜 입력
			{
				level++;                        // 다음 단계로
				if (level >= 3) level = 0;       // 3단 넘으면 다시 1단
				OCR3A = pwm_table[level];        // 밝기 적용
			}
		}

		prev_sw = cur_sw;                        // 현재 상태 저장
	}
}