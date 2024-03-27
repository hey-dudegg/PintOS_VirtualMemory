#include "userprog/process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "threads/synch.h"

#ifdef VM
#include "vm/vm.h"
#include "include/threads/thread.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);
struct thread *find_child(tid_t child_tid);

/* General process initializer for initd and other process. */
/* process_init 함수는 Pintos의 initd 및 다른 프로세스를 위한 일반적인 프로세스 초기화를 수행합니다.
현재 스레드의 포인터를 가져와서 현재 프로세스의 초기화 작업을 수행합니다.  */
static void  process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */

/* 첫 번째 유저랜드(사용자 영역) 프로그램인 "initd"를 시작합니다. 이 프로그램은 FILE_NAME에서 로드됩니다.
새로운 스레드는 process_create_initd()가 반환되기 전에 스케줄될 수 있으며 (심지어 종료될 수도 있음),
initd의 스레드 ID를 반환하거나 스레드가 생성되지 않은 경우 TID_ERROR를 반환합니다.
이 함수는 한 번만 호출되어야 합니다. */
tid_t process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME. Otherwise there's a race between the caller and load(). */
	/* FILE_NAME의 사본을 만듭니다. 그렇지 않으면 호출자와 load() 사이에 경합이 발생합니다. */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;

	strlcpy (fn_copy, file_name, PGSIZE);
	char *token, *save_ptr;
	token = strtok_r(file_name," ",&save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (token, PRI_DEFAULT, initd, fn_copy);
	// printf("==================%d==================\n", tid);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	
	return tid;
}

/* A thread function that launches first user process. */
/* 첫 번째 사용자 프로세스를 시작하는 스레드 함수입니다. */
static void initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0){
		PANIC("Fail to launch initd\n");
	}
	
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
/* 현재 프로세스를 'name'으로 복제합니다. 새 프로세스의 스레드 ID를 반환하거나
스레드가 생성되지 않은 경우 TID_ERROR를 반환합니다. */

// tid_t
// process_fork (const char *name, struct intr_frame *if_ UNUSED) {
// 	/* Clone current thread to new thread.*/
// 	struct thread *curr = thread_current();
// 	memcpy (curr->parent_if, &if_, sizeof (struct intr_frame));
// 	return thread_create (name,
// 			PRI_DEFAULT, __do_fork, thread_current ());
// }
tid_t process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
	struct thread *curr = thread_current();
	memcpy(&curr->parent_if, if_, sizeof (struct intr_frame)); // &curr->tf를 parent_if에 copy
	tid_t tid = thread_create (name, PRI_DEFAULT, __do_fork, thread_current ());
	if (tid == TID_ERROR){
		return TID_ERROR;
	}
	struct thread *t = find_child(tid);
	sema_down(&t->child_load_sema);
	if (t->exit_status == -1){
		//sema_up(&t->exit_sema);
		return TID_ERROR;
	}
	return tid;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	if (is_kernel_vaddr(va))
		return true;
	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);
	if (parent_page == NULL)
		return false;
	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page(PAL_USER | PAL_ZERO);
	if (newpage == NULL)
		return false;
	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);
	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
		palloc_free_page(newpage);
		return false;
	}
	return true;
}

#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */

/* 부모의 실행 컨텍스트를 복제하는 스레드 함수입니다.
힌트) parent->tf는 프로세스의 유저랜드 컨텍스트를 보유하지 않습니다.
  즉, process_fork()의 두 번째 인자를 이 함수로 전달해야 합니다. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if = &parent->parent_if;
	bool succ = true;
	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));
	if_.R.rax = 0;

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();				// PML4 생성
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);					// pml4를 CR3 레지스터에 load
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/
	
	// for(int i=2; i<parent->fd_idx; i++){
	// 	struct file *new_file =file_duplicate(parent->fdt[i]);
	// 	current->fdt[i] = new_file;
	// 	}

	if (parent->fd_idx == FDT_COUNT_LIMIT){
		goto error;
	}
	
	for(int i = 0; i < FDT_COUNT_LIMIT; i++)
	{
		struct file *file = parent->fdt[i];
		if(file==NULL)
			continue;
		if(file > 2)
			file = file_duplicate(file);
		current->fdt[i] = file;
	}
	current->fd_idx = parent->fd_idx;
	sema_up(&current->child_load_sema);
	process_init ();

	/* Finally, switch to the newly created process. */
	if (succ)
		do_iret (&if_);
error:
	sema_up(&current->child_load_sema);
	exit(-1);
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
/* 현재 실행 컨텍스트를 f_name으로 전환합니다. 실패한 경우 -1을 반환합니다. */
int process_exec (void *f_name) {
	char *file_name = f_name;
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */

	/* 현재 스레드가 다시 스케줄되면 실행 정보를 해당 구성원에
	저장하기 때문에 스레드 구조체의 intr_frame을 사용할 수 없습니다. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup ();

	char *fn_copy;
	
	fn_copy = palloc_get_page (0);
	
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);
	char *token, *save_ptrr;
	token = strtok_r(fn_copy," ",&save_ptrr);

	/* And then load the binary */
	lock_acquire(&filesys_lock);
	success = load (token, &_if);
	// printf("=============%s===============\n", file_name);
	palloc_free_page(fn_copy);
	
	lock_release(&filesys_lock);

	/* If load failed, quit. */
	if (!success)
	{
		palloc_free_page(file_name);
		return -1;
	}
	
	//argument_passing(f_name);
	int arg_cnt=1;
	char *save_ptr;

	for(int i=0;i<strlen(file_name);i++)
	{
		if(file_name[i] == ' ')
			arg_cnt++;
	}
	char *arg_list[arg_cnt];
	int64_t arg_addr_list[arg_cnt];

	int total_cnt=0;


	for(int i=0;i<arg_cnt;i++)
	{
		arg_list[i] = strtok_r((i==0) ? file_name : NULL," ",&save_ptr);
		if (arg_list[i] == NULL){
			arg_cnt--;
		}
	}

	for(int i=arg_cnt-1;i>=0;i--)
	{
		_if.rsp -= strlen(arg_list[i])+1;
		total_cnt+=strlen(arg_list[i])+1;
		strlcpy(_if.rsp,arg_list[i],strlen(arg_list[i])+1);
		arg_addr_list[i] = _if.rsp;
	
	}
	
	if(total_cnt%8!=0){
		_if.rsp -= 8-(total_cnt%8);
		memset(_if.rsp,0,8-(total_cnt%8));
	}

	_if.rsp -= 8;
	memset(_if.rsp,0,8);

	for(int i=arg_cnt-1;i>=0;i--)
	{
		_if.rsp -= 8;
		memcpy(_if.rsp,&arg_addr_list[i],8);
	}
	_if.rsp -= 8;
	memset(_if.rsp,0,8);

	_if.R.rdi = arg_cnt;
	_if.R.rsi = _if.rsp+8;

	/* If load failed, quit. */
	palloc_free_page (file_name);
	// if (!success)
	// 	return -1;
	// printf("=============%s===============\n", file_name);
	// hex_dump(_if.rsp, _if.rsp, USER_STACK - _if.rsp, true);
	/* Start switched process. */
	do_iret (&_if);
	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */

/* 스레드 TID가 종료될 때까지 기다리고 종료 상태를 반환합니다.
커널에 의해 종료되었을 경우(즉, 예외로 인해 종료되었을 경우) -1을 반환합니다.
TID가 잘못되었거나 호출 프로세스의 자식이 아닌 경우 또는
주어진 TID에 대해 이미 process_wait()가 성공적으로 호출된 경우,
즉시 -1을 반환하고 대기하지 않습니다.
이 함수는 문제 2-2에서 구현됩니다. 현재는 아무 작업도 수행하지 않습니다. */
struct thread *find_child(tid_t child_tid){
	struct thread *curr = thread_current();
	struct list_elem *e;
	for (e=list_begin(&curr->child_list);e != list_end(&curr->child_list);e=list_next(e)){
		struct thread *t = list_entry(e,struct thread,child_elem);
		if (t->tid == child_tid){
			return t;
		}
	}
	return NULL;
}
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	struct thread *t = find_child(child_tid);
	// printf("============================\n");
	if (t == NULL){
		return -1;
	}
	sema_down(&t->wait_sema); //부모가 자식이 종료될때까지 대기 (process_exit에서 자식이 종료될때 sema_up)
	list_remove(&t->child_elem); //자식이 종료됨을 알리는 'wait_signal'을 받으면 현재스레드(부모)의 자식리스트에서 제거
	sema_up(&t->exit_sema); // 부모 프로세스가 자식 프로세스의 종료 상태를 읽고, 자식 프로세스가 이제 완전히 종료될 수 있음을 알림.
	//timer_sleep(10);
	
	return t->exit_status;
}

/* Exit the process. This function is called by thread_exit (). */
/* 프로세스 종료. 이 함수는 thread_exit()에서 호출됩니다. */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
	// printf("%s: exit(%d)\n" , curr -> name , curr->status);
	//printf("process_exit\n");

	// printf("PEXIT(): 1\n"); ///

	// printf("fdt at %p\n", curr->fdt);
	// printf("fdt[0] %p\n", curr->fdt[0]);
	int i;
 	for(i=2;i<FDT_COUNT_LIMIT;i++){
		// printf("close iter: %d, curr->fdt[%d] = %p\n", i, i, curr->fdt[i]);
    if (curr->fdt[i] != NULL)
			close(i);
  }
//   printf("PEXIT(): 2\n"); ///
	palloc_free_page(curr->fdt);
	// printf("PEXIT(): 3\n"); ///
	file_close(curr->exec_file);
	// printf("PEXIT(): 4\n"); ///
	process_cleanup ();
	// printf("PEXIT(): 5\n"); ///
	sema_up(&curr->wait_sema); //자식이 종료 될때까지 대기하고 있는 부모에게 signal을 보낸다.
	// printf("PEXIT(): 6\n"); ///
	sema_down(&curr->exit_sema); //자식 프로세스가 부모 프로세스로부터 완전히 종료되기 위한 "허가"를 받을 때까지 자식 프로세스를 대기 상태로 만듬.
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	/* 현재 프로세스의 페이지 디렉터리를 파괴하고 커널 전용 페이지 디렉터리로 되돌립니다. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
/* 올바른 순서가 중요합니다. 페이지 디렉터리를 전환하기 전에
	 * cur->pagedir를 NULL로 설정해야 합니다.
	 * 그렇지 않으면 타이머 인터럽트가 프로세스 페이지 디렉터리로 다시 전환할 수 있습니다.
	 * 프로세스의 페이지 디렉터리를 파괴하기 전에 기본 페이지 디렉터리를 활성화해야 합니다.
	 * 그렇지 않으면 활성화된 페이지 디렉터리는
	 * 이미 해제되고 지워진 것입니다. */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
/* 이 함수는 매번 context switch될 때마다 CPU를 사용자 코드 실행을 위해 설정합니다. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/*
1. 페이지 테이블 활성화
next 스레드의 페이지 테이블을 활성화합니다.
이는 해당 스레드의 주소 공간에 접근할 수 있도록 하는 것으로,
현재 실행 중인 스레드가 사용한 페이지 테이블을 해제하고 next 스레드의 페이지 테이블을 활성화합니다.

2. 인터럽트 처리를 위한 스레드의 커널 스택 설정
인터럽트 발생 시 스레드가 사용할 커널 스택을 설정합니다.
인터럽트 처리 중에는 커널 스택이 필요하므로, 각 스레드가 자신만의 커널 스택을 가지고 있어야 합니다.
tss_update(next) 함수를 호출하여 TSS(태스크 세그먼트 디스크립터)의 스택 포인터를 업데이트합니다.
이는 다음 스레드로의 스위칭 시 인터럽트 처리를 위한 올바른 커널 스택을 사용할 수 있도록 하는 것입니다.
*/


/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */
/* ELF types.  See [ELF1] 1-2. */

/* ELF 이진 파일을 로드합니다. 다음 정의들은 ELF 명세서인 [ELF1]에서 거의 그대로 가져온 것입니다. */
/* ELF 유형입니다. [ELF1] 1-2를 참조하십시오. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */

/* 실행 가능한 헤더입니다. [ELF1] 1-4에서 1-8을 참조하십시오.
이것은 ELF 이진 파일의 맨 처음에 나타납니다. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */

/* ELF는 Executable and Linkable Format의 약어입니다.
이는 실행 가능한 파일 및 공유 라이브러리와 같은 바이너리 파일의 포맷을 설명하는 표준 형식입니다.
이 표준 형식은 주로 UNIX 및 UNIX 계열 운영 체제에서 사용됩니다.
여기서 ELF64_hdr는 ELF 헤더 구조체의 형식을 나타냅니다.
Pintos에서는 64비트 ELF 파일 포맷을 사용하는 것으로 보입니다. */
#define ELF ELF64_hdr

/* Phdr은 ELF 헤더 내의 프로그램 헤더(ELF Program Header)를 가리키는 약어입니다.
ELF 파일에는 다양한 섹션 및 세그먼트가 있으며, 각 세그먼트는 프로그램이 메모리에서
로드되는 방식을 정의하는 프로그램 헤더를 가집니다.
Pintos에서는 이러한 프로그램 헤더를 나타내기 위해 ELF64_PHDR가 사용되는 것으로 보입니다. */
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */

/* FILE_NAME에서 ELF 실행 파일을 현재 스레드로 불러옵니다.
 * 실행 파일의 진입 지점을 *RIP에 저장하고, 초기 스택 포인터를 *RSP에 저장합니다.
 * 성공하면 true를 반환하고, 그렇지 않으면 false를 반환합니다. */
static bool load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();				// 여기서 프로세스의 PML4가 생성된다.
	if (t->pml4 == NULL)
		goto done;

	process_activate (thread_current ());	// 다음 스레드를 활성화하는 데 필요한 일련의 작업을 수행하여 스레드의 실행이 가능한 상태로 만듦

	/* Open executable file. */
	
	file = filesys_open (file_name);		// 파일을 열어 내부 메모리(커널의 가상 메모리 공간)에 할당한다.
	

	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}
	// palloc_free_page(fn_copy);
	

	/* Read and verify executable header. */
	/*
	실행 파일의 헤더를 읽고 유효성을 검사하는 과정을 수행합니다.
	주어진 파일이 ELF(Executable and Linkable Format) 형식의 실행 파일인지 확인하기 위해 해당 파일의 헤더를 읽어들입니다.
	그리고 다음의 조건을 확인하여 유효한 실행 파일인지를 판단합니다.
	*/
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		
		/*
		1. 파일 헤더가 정상적으로 읽혔는지 확인합니다.
		2. ELF 매직 넘버를 확인하여 ELF 형식의 실행 파일인지 확인합니다.
		3. 실행 파일의 형식(Executable Type)이 2인지 확인합니다. 이 값은 ELF 형식의 실행 파일임을 나타냅니다.
		4. 실행 파일의 아키텍처(Machine)가 0x3E(amd64)인지 확인합니다.
		5. ELF 버전이 1인지 확인합니다.
		6. 프로그램 헤더 엔트리의 크기가 예상된 크기와 일치하는지 확인합니다.
		7. 프로그램 헤더 엔트리의 수가 적절한 범위 내에 있는지 확인합니다.
		*/

		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
			
				/* 이 코드는 ELF 파일에서 각 세그먼트를 메모리로 로드하는 과정을 처리합니다.
				여기에는 세그먼트를 디스크에서 읽고 메모리에 로드하며, 필요한 경우 나머지 부분을 0으로
				초기화하는 작업이 포함됩니다. */
				if (validate_segment (&phdr, file)) {				// 현재 프로그램 헤더(phdr)가 유효한지 확인
					bool writable = (phdr.p_flags & PF_W) != 0;		// 세그먼트가 쓰기 가능한지 여부를 나타내는 부울 변수입니다. p_flags 필드에서 PF_W 비트를 확인하여 세그먼트가 쓰기 가능한지 확인합니다.
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						/* 세그먼트가 일부 데이터를 포함하는 경우, 해당 데이터를 디스크에서 읽고 남은 부분을 0으로 채운다 */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						/* 세그먼트가 전적으로 0으로 구성된 경우 디스크에서 아무것도 읽지 않는다. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					// printf("=====================\n");
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
								// printf("=====================\n");
						goto done;
				}
				else
					goto done;
				break;
		}
	}
	/* file_deny_write*/
	t->exec_file=file;
    file_deny_write(file);				// 파일을 읽기 전용으로 설정.
	// printf("=====================\n");
	/* Set up stack. */
	if (!setup_stack (if_))				// 피지컬 메모리에 페이지로 할당.
	// printf("=====================\n");
		goto done;
		// printf("============================\n");
	
	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	//file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
/* PHDR가 FILE에서 유효한, 로드 가능한 세그먼트를 설명하는지 확인하고,
그렇다면 true를 반환하고, 그렇지 않으면 false를 반환합니다. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

/*
이 함수는 주어진 ELF 파일을 로드하여 프로세스의 유저 가상 메모리에 올리고 물리 메모리에 매핑하는 작업을 수행합니다.

주요 과정은 다음과 같습니다.

* 페이지 디렉터리 할당 및 활성화:
현재 스레드의 페이지 디렉터리(t->pml4)를 생성합니다.
이 페이지 디렉터리는 새로운 프로세스의 가상 주소 공간을 관리합니다.
pml4_create() 함수를 사용하여 새로운 페이지 디렉터리를 할당하고,
해당 스레드의 pml4 필드에 할당된 페이지 디렉터리의 주소를 저장합니다.

* 프로세스 활성화:
새로운 프로세스가 생성되었으므로, 이를 스케줄링 가능한 상태로 만듭니다.
process_activate() 함수를 사용하여 현재 스레드를 활성화하고, 스케줄링 가능한 상태로 만듭니다.

* 실행 파일 열기:
주어진 파일명을 사용하여 실행 파일을 엽니다.
filesys_open() 함수를 사용하여 파일 시스템에서 실행 파일을 엽니다.

* ELF 헤더 읽기 및 검증:
열린 파일로부터 ELF 헤더를 읽어들입니다.
ELF 헤더의 유효성을 확인합니다. 유효하지 않은 경우 실패 처리를 하고 함수를 종료합니다.

* 프로그램 헤더 읽기:
ELF 파일에서 프로그램 헤더들을 읽어들입니다.
각 프로그램 헤더에는 프로그램의 세그먼트에 대한 정보가 포함되어 있습니다.

* 세그먼트 로드:
읽어들인 프로그램 헤더를 기반으로 세그먼트를 메모리에 로드합니다.
세그먼트의 종류에 따라 적절한 처리를 수행합니다. 코드, 데이터 또는 스택이 될 수 있습니다.

* 실행 파일에 대한 파일 디스크립터 설정:
현재 스레드의 exec_file 필드에 열린 실행 파일에 대한 파일 디스크립터를 저장합니다.

* 스택 설정:
프로세스의 초기 스택을 설정합니다.
setup_stack() 함수를 호출하여 스택을 설정합니다.

* 시작 주소 설정:
ELF 파일의 진입점(entry point)을 설정합니다.
현재 스레드의 if_->rip 필드에 진입점의 주소를 저장합니다.

* 성공 여부 반환:
모든 작업이 성공적으로 완료되면 true를 반환하고, 실패한 경우 false를 반환합니다.
이 함수는 프로세스의 실행 파일을 메모리에 로드하는 데 필요한 모든 단계를 수행하고, 성공 여부를 반환합니다.
*/


#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */
/* load() helpers. */

/* 이 블록의 코드는 프로젝트 2에서만 사용됩니다.
전체 프로젝트 2를 위해 함수를 구현하려면 #ifndef 매크로 외부에 구현하십시오. */

/* load() 도우미 함수. */
// static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */

/* 주소 UPAGE에서 시작하여 파일의 오프셋 OFS에있는 세그먼트를 로드합니다.
총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리가 다음과 같이 초기화됩니다:
UPAGE에서의 READ_BYTES 바이트는 OFS에서 시작하는 파일에서 읽어야합니다.
UPAGE + READ_BYTES에서의 ZERO_BYTES 바이트는 모두 0으로 설정되어야합니다.
이 함수에 의해 초기화된 페이지는 WRITABLE이 true 인 경우 사용자 프로세스에 의해 쓰기 가능해야합니다.
그렇지 않으면 읽기 전용이어야합니다.
성공하면 true를 반환하고, 메모리 할당 오류 또는 디스크 읽기 오류가 발생하면 false를 반환합니다. */

/* 이 코드는 파일에서 가상 메모리에 세그먼트를 로드하는 함수입니다.
주어진 파일에서 오프셋(ofs)부터 시작하여 읽어야 할 바이트(read_bytes) 및
0으로 초기화해야 할 바이트(zero_bytes)를 가져와 가상 주소 공간에 매핑합니다. */
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	struct supplemental_page_table cur_spt = thread_current()->spt;

	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);	// 읽어야 할 바이트와 0으로 초기화해야 할 바이트의 합이 페이지 크기의 배수인지 확인
	ASSERT (pg_ofs (upage) == 0);						// 가상 페이지의 오프셋이 0인지 확인
	ASSERT (ofs % PGSIZE == 0);							// 오프셋이 페이지 크기의 배수인지 확인

	file_seek (file, ofs);								// 파일의 오프셋을 설정하여 읽어야 할 위치로 이동
	
	while (read_bytes > 0 || zero_bytes > 0) {			// 읽어야 할 바이트나 0으로 초기화해야 할 바이트가 남아 있다면 반복
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;	// 읽어야 할 바이트가 페이지 크기보다 작으면 남은 바이트만큼 읽는다.
		size_t page_zero_bytes = PGSIZE - page_read_bytes;					// 페이지 크기에서 읽어야 할 바이트를 뺀 만큼을 0으로 치고화해야 할 바이트로 설정

		#ifdef VM
		// cur_spt.file = file;
		// cur_spt.offset = ofs;
		// cur_spt.type = upage;
		// cur_spt.read_bytes = read_bytes;
		// cur_spt.zero_bytes = zero_bytes;
		// cur_spt.writable = writable;
	
		// vm_entry 생성: 가상 메모리의 각 세그먼트에 대한 정보를 관리하기 위해 vm_entry 구조체를 생성해야 합니다.
		// supplemental_page_table_init (&cur_spt);

		/* vm_entry 초기화: 새로운 vm_entry를 생성한 후, 해당 entry의 필드 값을 초기화해야 합니다.
		이 필드에는 가상 주소 범위, 파일 오프셋, 파일 크기, 읽어야 할 바이트 수, 0으로 초기화해야 할 바이트 수,
		읽기 가능 여부 등이 포함될 수 있습니다. */
		// vm_alloc_page_with_initializer();

		/* vm_entry를 해시 테이블에 삽입: 생성한 vm_entry를 해시 테이블에 삽입하여 가상 주소와 해당 세그먼트 간의 매핑을
		관리할 수 있도록 해야 합니다. 이를 위해 해시 함수와 삽입 함수가 필요합니다. */


		/**************************************************/
		/* 이 코드 블록은 파일의 내용을 물리 페이지로 읽어와 프로세스의 가상 주소 공간에
		매핑하는 과정을 수행함. 이를 통해 프로세스는 파일의 내용을 가상 메모리에서 접근할 수 있게 됨 */

		#endif
		/* Get a page of memory. */
		/* PAL_USER 플래그를 사용하여 가상 메모리에 대응되는 사용자 영역에 물리 페이지를 할당
		PAL_USER 플래그는 페이지 할당의 목적을 나타내며, 사용자 모드에서의 접근을 허용한다. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL) return false;
		
		/* Load this page. */
		/* 파일에서 데이터를 읽어와 페이지에 쓰고, 읽은 바이트 수가 예상한 바이트 수와 일치하는지 확인
		파일의 내용을 물리 페이지에 쓴다. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			
			return false;
		}
		
		/* 읽은 바이트 이후의 메모리를 0으로 초기화 */
		/* 파일의 크기가 페이지 크기보다 작은 경우 해당 */
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		/* 페이지를 프로세스의 주소 공간에 매핑, 매핑에 실패하면 페이지를 해제하고 실패를 반환
		페이지 테이블에 새로운 매핑을 추가 */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			
			return false;
		/* 이 코드 블록은 페이지를 프로세스의 주소 공간에 매핑하는 과정을 수행합니다.
		여기서 "매핑"이란 가상 주소와 물리 주소 간의 대응 관계를 설정하는 것을 의미합니다.
		매핑이 성공하면 프로세스는 해당 가상 주소를 사용하여 메모리에 접근할 수 있게 됩니다. */
		}
		
		/* Advance. */
		/* 남은 읽어야 할 바이트와 0으로 초기화해야 할 바이트 수를 갱신하고, 
		가상 페이지 주소를 증가시켜 적절한 위치로 이동 */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		ofs += page_read_bytes;					// ?
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
/* 이 코드는 주어진 인터럽트 프레임을 기준으로 사용자 스택을 설정하기 위해 호출되는 함수입니다.
사용자 스택은 새 프로세스의 초기 스택을 나타내며, 사용자 프로그램이 함수 호출 및 로컬 변수를 저장하는 데 사용.
새로운 프로세스가 실행될 때 필요한 스택을 설정하고, 스택에 필요한 초기화된 상태와 인자들을 전달하는 작업을 수행 */
static bool setup_stack (struct intr_frame *if_) {
	// uint8_t *kpage;
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t) USER_STACK) - PGSIZE);	// 사용자 스택을 나타내는 가장 주소 획득
	
	if (vm_alloc_page (VM_ANON | VM_MARKER_0, stack_bottom, 1)) {		// 할당된 페이지를 매핑시키고 페이지 테이블에 등록
		success = vm_claim_page (stack_bottom);	
		// printf("===========%d==========\n",success);
		if (success) {
			// printf("=====================\n");
			if_->rsp = USER_STACK;				// 현재 스레드의 스택 포인터를 USER_STACK으로 설정
			thread_current()->stack_bottom = stack_bottom;	
		}
	}

	return success;
/*페이지 할당 함수를 사용하여 사용자 페이지를 가져옵니다.
PAL_USER 플래그는 사용자 영역에 페이지를 할당하고, PAL_ZERO 플래그는 페이지를 0으로 초기화합니다. */
	// kpage = palloc_get_page (PAL_USER | PAL_ZERO);

/* 페이지를 성공적으로 할당했을 때, install_page 함수를 호출하여 페이지 테이블에 매핑합니다.
이 때, USER_STACK에 사용자 스택의 시작 주소가 정의되어 있습니다.*/
	// if (kpage != NULL) {
	// 	success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
	// 	/* 페이지를 성공적으로 매핑했을 경우, if_ 구조체의 스택 포인터를 USER_STACK으로 설정합니다.
	// 	그렇지 않으면 할당한 페이지를 해제합니다. */
	// 	if (success)
	// 		if_->rsp = USER_STACK;
	// 	else
	// 		palloc_free_page (kpage);
	// }

	/* TODO.
	기존 - 단일 페이지 할당, 페이지 테이블 설정, 스택 포인터 설정(esp)
	추가
	* 먼저, 4KB 스택의 가상 메모리 엔트리(vm_entry)를 생성해야 합니다.
	* 그런 다음, 생성된 vm_entry의 필드 값을 초기화해야 합니다.
	* 마지막으로, 초기화한 vm_entry를 해시 테이블에 삽입하여 스택 메모리를 추적할 수 있도록 합니다.
	*/
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */

/* 사용자 가상 주소 UPAGE에서 커널 가상 주소 KPAGE로의 매핑을 페이지 테이블에 추가합니다.
 * WRITABLE이 true인 경우, 사용자 프로세스는 페이지를 수정할 수 있습니다. 그렇지 않으면 읽기 전용입니다.
 * UPAGE는 이미 매핑되어 있지 않아야 합니다.
 * KPAGE는 일반적으로 palloc_get_page()로 사용자 풀에서 얻은 페이지여야 합니다.
 * UPAGE가 이미 매핑되어 있거나 메모리 할당이 실패한 경우 false를 반환합니다. */
static bool install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	/* 해당 가상 주소에 이미 페이지가 있는지 확인한 후 페이지를 매핑합니다. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));

/* 현재 스레드의 페이지 디렉토리(또는 PML4)를 가져옵니다.
해당 가상 주소에 이미 페이지가 있는지 확인하기 위해 pml4_get_page() 함수를 호출합니다.
만약 이미 페이지가 존재한다면, 새로운 페이지를 매핑할 수 없으므로 false를 반환합니다.
새로운 페이지를 매핑하기 위해 pml4_set_page() 함수를 호출합니다.
이 함수는 페이지 테이블 항목을 설정하여 가상 주소와 물리 주소를 매핑하고, 이때 해당 페이지에 대한 권한도 설정합니다.
만약 페이지 테이블 항목을 성공적으로 설정하면 true를 반환합니다. */

}

#else
bool install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	/* 해당 가상 주소에 이미 페이지가 있는지 확인한 후 페이지를 매핑합니다. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

/* 여기부터는 프로젝트 3 이후에 사용될 코드입니다.
 * 프로젝트 2에서만 해당 기능을 구현하려면, 위의 블록에 구현하십시오. */

/* lazy loading은 메모리 로딩이 필요한 시점까지 지연되는 디자인
가상 page만 할당해두고 필요한 page를 요청하면 page fault가 발생하고 해당 page를 type에 맞게 초기화하고
frame과 연결하고 userprogram으로 제어권을 넘긴다.

* 작동 과정
파일 오프셋을 지정된 위치로 이동시킵니다.
파일에서 읽어들인 데이터를 페이지 프레임의 커널 가상 주소(page->frame->kva)에 씁니다.
세그먼트의 나머지 부분을 0으로 초기화합니다(필요한 경우).
함수가 호출될 때 제공된 가상 주소(VA)가 첫 번째 페이지 폴트가 발생한 위치입니다.
이 위치의 페이지를 로드하고 해당 페이지가 가리키는 프레임에 데이터를 씁니다. */
static bool lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */

	struct file *file = ((struct supplemental_page_table *)aux)->file;
	off_t offset_of = ((struct supplemental_page_table *)aux)->offset;
	size_t page_read_bytes = ((struct supplemental_page_table *)aux)->read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;
	
	/* TODO: 파일에서 세그먼트를 로드합니다. */
	file_seek (file, offset_of);				//	파일 오프셋을 지정된 위치로 이동

	if (file_read (file, page->frame->kva, page_read_bytes) != (int)page_read_bytes) {	// spt의 파일 크기와 frame의 파일 크기 체크
		palloc_free_page (page->frame->kva);
		
		return false;
	}
	
	memset ((page->frame->kva + page_read_bytes), 0, page_zero_bytes);

	return true;
	/* TODO: 이 함수가 호출될 때 VA 주소에서 첫 번째 페이지 폴트가 발생합니다. */
	/* TODO: VA는 이 함수를 호출할 때 사용 가능합니다. */
}


/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */

/* 파일의 오프셋 OFS에서 시작하는 세그먼트를 주소 UPAGE에 로드합니다.
 * 총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리가 초기화됩니다. 다음과 같이 작동합니다:
 * - UPAGE에서 READ_BYTES 바이트는 OFS에서 시작하여 FILE에서 읽어야 합니다.
 * - UPAGE + READ_BYTES에서 ZERO_BYTES 바이트는 제로화되어야 합니다.
 * 
 * 이 함수에 의해 초기화된 페이지는 WRITABLE이 true인 경우 사용자 프로세스에 의해 쓰기 가능해야 하며,
 * 그렇지 않으면 읽기 전용이어야 합니다.
 *
 * 성공하면 true를 반환하고, 메모리 할당 오류 또는 디스크 읽기 오류가 발생하면 false를 반환합니다. */
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {

	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		/* 이 페이지를 채우는 방법을 계산합니다.
		 * FILE에서 PAGE_READ_BYTES 바이트를 읽고,
		 * 나머지 PAGE_ZERO_BYTES 바이트는 제로화합니다. */

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;		// 최대 페이지 사이즈와 비교
		size_t page_zero_bytes = PGSIZE - page_read_bytes;						// 남은 만큼 0으로 패딩

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		/* lazy_load_segment에 정보를 전달하기 위해 aux를 설정합니다. */

		// void *aux = NULL;
		struct supplemental_page_table *spt = (struct supplemental_page_table *) malloc 
													(sizeof(struct supplemental_page_table));
		
		ASSERT(file);
		
		spt->file = file;
		spt->read_bytes = page_read_bytes;
		spt->zero_bytes = page_zero_bytes;
		spt->offset = ofs;

		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, spt)) 
			return false;
			
		/* Advance. */
		/*  주어진 세그먼트에서 페이지를 읽은 내용을 제로화한 후에 다음 페이지로 넘어가는 과정 */
		read_bytes -= page_read_bytes;		// 현재 쓴 바이트 수를 뺌
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;					// 포인터를 다음 시작 주소로 이동
		ofs += page_read_bytes;				// 현재 페이지에 읽은 바이트 수 만큼 이동
	}

	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
/* 스택을 stack_bottom에 매핑하고 즉시 페이지를 요청합니다. 성공하면 rsp를 적절히 설정합니다. */
static bool setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */
	/* 스택을 stack_bottom에 매핑하고 즉시 페이지를 요청합니다.
	 * 성공하면 rsp를 적절히 설정합니다.
	 * 페이지를 스택으로 표시해야 합니다. */
	/* 여기에 코드를 작성하세요 */
	if (vm_alloc_page (VM_ANON | VM_MARKER_0, stack_bottom, 1)) {
		success = vm_claim_page (stack_bottom);					// 할당된 페이지를 매핑시키고 페이지 테이블에 등록
		// printf("============================\n");
		if (success) {
			if_->rsp = USER_STACK;								// 현재 스레드의 스택 포인터를 USER_STACK으로 설정
			thread_current()->stack_bottom = stack_bottom;
		}
	}

	return success;
}
#endif /* VM */
