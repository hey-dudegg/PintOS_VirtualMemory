#include "userprog/tss.h"
#include <debug.h>
#include <stddef.h>
#include "userprog/gdt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "intrinsic.h"

/* The Task-State Segment (TSS).
 *
 *  Instances of the TSS, an x86-64 specific structure, are used to
 *  define "tasks", a form of support for multitasking built right
 *  into the processor.  However, for various reasons including
 *  portability, speed, and flexibility, most x86-64 OSes almost
 *  completely ignore the TSS.  We are no exception.
 *
 *  Unfortunately, there is one thing that can only be done using
 *  a TSS: stack switching for interrupts that occur in user mode.
 *  When an interrupt occurs in user mode (ring 3), the processor
 *  consults the rsp0 members of the current TSS to determine the
 *  stack to use for handling the interrupt.  Thus, we must create
 *  a TSS and initialize at least these fields, and this is
 *  precisely what this file does.
 *
 *  When an interrupt is handled by an interrupt or trap gate
 *  (which applies to all interrupts we handle), an x86-64 processor
 *  works like this:
 *
 *    - If the code interrupted by the interrupt is in the same
 *      ring as the interrupt handler, then no stack switch takes
 *      place.  This is the case for interrupts that happen when
 *      we're running in the kernel.  The contents of the TSS are
 *      irrelevant for this case.
 *
 *    - If the interrupted code is in a different ring from the
 *      handler, then the processor switches to the stack
 *      specified in the TSS for the new ring.  This is the case
 *      for interrupts that happen when we're in user space.  It's
 *      important that we switch to a stack that's not already in
 *      use, to avoid corruption.  Because we're running in user
 *      space, we know that the current process's kernel stack is
 *      not in use, so we can always use that.  Thus, when the
 *      scheduler switches threads, it also changes the TSS's
 *      stack pointer to point to the new thread's kernel stack.
 *      (The call is in schedule in thread.c.) */

/* Kernel TSS. */

/*
태스크 상태 세그먼트(TSS).

TSS의 인스턴스는 x86-64 특정 구조체로서, "태스크"를 정의하는 데 사용됩니다.
이것은 프로세서에 내장된 멀티태스킹을 지원하는 형태입니다.
그러나 이식성, 속도 및 유연성을 포함한 여러 가지 이유로 대부분의 x86-64 OS는 TSS를 거의 완전히 무시합니다.
우리도 예외는 아닙니다.

불행히도, TSS를 사용해야만 하는 한 가지 작업이 있습니다:
사용자 모드에서 발생하는 인터럽트에 대한 스택 전환입니다.
사용자 모드(ring 3)에서 인터럽트가 발생하면 프로세서는 현재 TSS의 rsp0 멤버를 확인하여
인터럽트를 처리하는 데 사용할 스택을 결정합니다.
따라서, 우리는 TSS를 생성하고 적어도 이러한 필드를 초기화해야 하며, 이 파일이 바로 이를 수행합니다.

인터럽트가 인터럽트 게이트나 트랩 게이트에 의해 처리될 때(x86-64 프로세서는 모든 인터럽트를 처리하는 방식),
다음과 같이 작동합니다:

- 인터럽트로 인해 중단된 코드가 인터럽트 핸들러와 동일한 ring에 있는 경우, 스택 전환이 발생하지 않습니다.
  이것은 커널에서 실행 중일 때 발생하는 인터럽트의 경우입니다.
  TSS의 내용은 이 경우에는 관계가 없습니다.

- 중단된 코드가 핸들러와 다른 ring에 있는 경우, 프로세서는 새로운 ring을 위해 TSS에 지정된 스택으로 전환합니다.
  이것은 사용자 공간에서 실행 중일 때 발생하는 인터럽트의 경우입니다.
  스택이 이미 사용 중인 경우 손상을 피하기 위해 사용되지 않은 스택으로 전환해야 합니다.
  사용자 공간에서 실행 중이므로 현재 프로세스의 커널 스택이 사용 중이 아님을 알 수 있으므로
  항상 해당 스택을 사용할 수 있습니다.
  따라서, 스케줄러가 스레드를 전환할 때마다 TSS의 스택 포인터를 새 스레드의 커널 스택을 가리키도록 변경합니다.
  (호출은 thread.c의 schedule에 있습니다.)

*/


struct task_state *tss;

/* Initializes the kernel TSS. */
void
tss_init (void) {
	/* Our TSS is never used in a call gate or task gate, so only a
	 * few fields of it are ever referenced, and those are the only
	 * ones we initialize. */
	tss = palloc_get_page (PAL_ASSERT | PAL_ZERO);
	tss_update (thread_current ());
}

/* Returns the kernel TSS. */
struct task_state *
tss_get (void) {
	ASSERT (tss != NULL);
	return tss;
}

/* Sets the ring 0 stack pointer in the TSS to point to the end
 * of the thread stack. */

/* 이 함수는 TSS(Task-State Segment) 내의 ring 0 스택 포인터를 스레드 스택의 끝을 가리키도록 설정합니다.
TSS는 x86 아키텍처에서 사용되며 다중 작업 및 인터럽트 처리와 관련된 정보를 저장합니다. ring 0은 커널 모드를 나타냅니다.
따라서 TSS 내의 ring 0 스택 포인터는 커널 스택의 끝을 가리키도록 설정됩니다. */
void
tss_update (struct thread *next) {
	ASSERT (tss != NULL);
	tss->rsp0 = (uint64_t) next + PGSIZE;
}

/* 
TSS(Task-State Segment)는 x86 아키텍처에서 사용되는 특별한 데이터 구조입니다.
이는 멀티태스킹 및 인터럽트 처리와 관련된 정보를 저장하는 데 사용됩니다.
TSS는 CPU가 프로세스 간의 전환을 관리하고 프로세스가 인터럽트 또는 예외를 처리하는 데 필요한 상태 정보를 제공합니다.

TSS에는 다음과 같은 정보가 포함될 수 있습니다.

1. 스택 포인터(스택 주소): 인터럽트나 예외 발생 시 해당 이벤트를 처리하기 위한 스택의 위치를 나타냅니다.
사용자 모드에서 발생한 인터럽트나 예외가 커널 모드로 전환될 때 사용됩니다.
2. 사용자 모드 스택 포인터: 사용자 모드에서 실행 중인 프로세스의 스택 주소를 저장합니다.
사용자 프로세스의 컨텍스트 전환 시 사용됩니다.
3. I/O 맵 베이스 주소: 입출력 명령에 대한 I/O 맵의 위치를 지정합니다.
4. LDT(로컬 디스크립터 테이블) 선택자: 로컬 디스크립터 테이블을 가리키는 선택자입니다.
LDT는 프로세스의 메모리에 대한 접근 권한을 설정합니다.

TSS는 x86 아키텍처의 중요한 구성 요소이지만, 실제 운영 체제에서는 대부분의 경우 사용되지 않습니다.
이는 현대의 운영 체제가 다중 작업을 관리하기 위해 가상 메모리 및 스레드 스케줄링과 같은 다른 메커니즘을 사용하기 때문입니다.
그러나 인터럽트 및 예외 처리와 같은 특정한 하드웨어 요구 사항에 의해 TSS가 필요한 경우가 있습니다.
*/