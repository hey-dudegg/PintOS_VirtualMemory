/* inspect.c: Testing utility for VM. */
/* DO NOT MODIFY THIS FILE. */

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "vm/inspect.h"

static void
inspect (struct intr_frame *f) {
	const void *va = (const void *) f->R.rax;
	f->R.rax = PTE_ADDR (pml4_get_page (thread_current ()->pml4, va));
}

/* Tool for testing vm component. Calling this function via int 0x42.
 * Input:
 *   @RAX - Virtual address to inspect
 * Output:
 *   @RAX - Physical address that mmaped to input. */

/* 이 함수는 내부 검사용 인터럽트를 등록하는 데 사용됩니다. 주로 가상 메모리를 검사하고 디버깅하는 데 사용됩니다.
이 함수는 intr_register_int() 함수를 호출하여 내부 인터럽트 번호 0x42에 대한 핸들러를 등록합니다.
핸들러 함수는 inspect이라는 이름으로 정의되어 있으며, 가상 메모리를 검사하는 데 사용됩니다.
이 함수는 DPL을 3으로 설정하여 사용자 모드에서도 호출할 수 있도록 합니다.
또한 인터럽트가 발생하면 현재 인터럽트가 비활성화되도록 INTR_OFF를 설정합니다.
이러한 구성은 가상 메모리 검사 과정 중에 인터럽트가 발생하는 것을 방지하여 검사 도중 데이터의 일관성을 유지하는 데 도움이 됩니다. */
void
register_inspect_intr (void) {
	intr_register_int (0x42, 3, INTR_OFF, inspect, "Inspect Virtual Memory");
}
