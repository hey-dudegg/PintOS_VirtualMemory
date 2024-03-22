#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,						// VM_BIN, 바이너리 파일로부터 데이터를 로드
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "include/lib/kernel/hash.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
 /* page에 대해 우리가 알아야 하는 모든 필요한 정보를 담고 있다. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	/* 하나의 메모리 영역에 다른 타입의 데이터를 저장하는 것을 허용하는 특별한 자료형 */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
/* 각각이 하나의 함수 테이블 */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
/* 
/* 현재 프로세스의 메모리 공간을 나타내는 구조체입니다.
이 구조체에 대해 특정한 디자인을 강요하고 싶지 않습니다.
이에 대한 모든 디자인은 여러분에게 달려 있습니다. */
struct supplemental_page_table {
	// 1. 가상 주소와 그에 해당하는 물리 페이지의 매핑
	void *vaddr;				// vm_entry가 관리하는 가상페이지 번호
	bool is_loaded;				// 물리 메모리의 탑재 여부를 알려주는 플래그

	/* vm_entry를 위한 자료구조 */
	struct hash hash;
	struct hash_elem hash_elem;	// 해시 테이블 element

	/* VA initialize */
	struct file* file;			// 가상 주소와 맵핑된 파일
	size_t offset;				// 읽어야 할 파일 오프셋
	uint8_t type;				// VM_BIN, VM_FILE, VM_ANON
	size_t read_bytes;			// 가상페이지에 쓰여져 있는 데이터의 크기
	size_t zero_bytes;			// 0으로 채울 남은 페이지의 바이트
	bool writable;				// True -> 쓰기 가능, False -> 읽기만 가능

	/* Memory Mapped File */
	struct list_elem mmap_elem;	// mmap 리스트 element	

	/* Swapping */
	size_t swap_slot;			// 스왑 슬롯

	// 2. 페이지 부재 처리 지원, 필요한 페이지를 디스크에서 가져오거나 필요에 따라 초기화
	// 3. 메모리 관리 및 보호, 각 페이지의 상태를 추적하고 메모리 보호를 제공.
	// 4. 페이지의 읽기/쓰기 여부, 페이지의 초기화 여부

/* 
	1. Virtual page number
	2. Read/write permission

	3. Type of virtual page
	a page of ELF executable file
	a page of general file
	a page of swap area

	4. Reference to the file object and offset(memory mapped file)
	5. Amount of data in the page
	6. Location in the swap area
	7. In-memory flag: is it in memory?
*/
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);

struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

/* 이 매크로는 vm_alloc_page라는 이름의 매크로를 정의하고 있습니다. 이 매크로는 세 개의 매크로 인자를 받습니다: type, upage, writable.
그리고 이 매크로는 vm_alloc_page_with_initializer 함수를 호출하는데, 여기서 type, upage, writable은 그대로 전달되고,
init와 aux는 NULL로 설정됩니다. 즉, vm_alloc_page_with_initializer 함수를 호출할 때 init와 aux 인자는 NULL로 전달됩니다.*/
#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

#endif  /* VM_VM_H */
