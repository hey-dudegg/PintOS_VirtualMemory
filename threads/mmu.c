#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "intrinsic.h"

static uint64_t *
pgdir_walk (uint64_t *pdp, const uint64_t va, int create) {
	int idx = PDX (va);
	if (pdp) {
		uint64_t *pte = (uint64_t *) pdp[idx];
		if (!((uint64_t) pte & PTE_P)) {
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page)
					pdp[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
				else
					return NULL;
			} else
				return NULL;
		}
		return (uint64_t *) ptov (PTE_ADDR (pdp[idx]) + 8 * PTX (va));
	}
	return NULL;
}

static uint64_t *
pdpe_walk (uint64_t *pdpe, const uint64_t va, int create) {
	uint64_t *pte = NULL;
	int idx = PDPE (va);
	int allocated = 0;
	if (pdpe) {
		uint64_t *pde = (uint64_t *) pdpe[idx];
		if (!((uint64_t) pde & PTE_P)) {
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page) {
					pdpe[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
					allocated = 1;
				} else
					return NULL;
			} else
				return NULL;
		}
		pte = pgdir_walk (ptov (PTE_ADDR (pdpe[idx])), va, create);
	}
	if (pte == NULL && allocated) {
		palloc_free_page ((void *) ptov (PTE_ADDR (pdpe[idx])));
		pdpe[idx] = 0;
	}
	return pte;
}

/* Returns the address of the page table entry for virtual
 * address VADDR in page map level 4, pml4.
 * If PML4E does not have a page table for VADDR, behavior depends
 * on CREATE.  If CREATE is true, then a new page table is
 * created and a pointer into it is returned.  Otherwise, a null
 * pointer is returned. */

/* 페이지 맵 레벨 4, pml4에서 가상 주소 VADDR에 대한 페이지 테이블 항목의 주소를 반환합니다.
PML4E가 VADDR에 대한 페이지 테이블을 가지고 있지 않으면, 동작은 CREATE에 따라 달라집니다.
CREATE가 true이면 새 페이지 테이블이 생성되고 그 안에 대한 포인터가 반환됩니다.
그렇지 않으면 null 포인터가 반환됩니다. */
uint64_t * pml4e_walk (uint64_t *pml4e, const uint64_t va, int create) {
	uint64_t *pte = NULL;		// 페이지 테이블 엔트리를 가리키는 포인터 초기화
	int idx = PML4 (va);		// 가상 주소의 pml4 인덱스 계산
	int allocated = 0;			// 새 페이지 테이블이 할당되었는지 여부 초기화

	if (pml4e) {				// pmle4 entry가 유효한 경우
		uint64_t *pdpe = (uint64_t *) pml4e[idx];			// page directory pointer entry를 가져오기

		if (!((uint64_t) pdpe & PTE_P)) {					// pdpe가 유효하지 않은 경우
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);			// 페이지 테이블을 위한 메모리 할당
				if (new_page) {												// 할당된 페이지가 있는 경우
					pml4e[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;	// 새 페이지 테이블을 PML4에 매핑 및 속성 설정
					allocated = 1;											// 새 페이지 테이블이 할당되었음을 표시
				} else
					return NULL;			// page table을 할당 할 수 없는 경우
			} else
				return NULL;				// 새 페이지 테이블을 생성하지 않아야 하는 경우 
		}
		pte = pdpe_walk (ptov (PTE_ADDR (pml4e[idx])), va, create);			// 가상 주소에 해당하는 pte 찾기 또는 생성하기
	}
	if (pte == NULL && allocated) {			// 반환된 페이지 테이블 엔트리가 없고, 새 페이지 테이블이 할당된 경우
		palloc_free_page ((void *) ptov (PTE_ADDR (pml4e[idx])));			// 할당된 페이지 테이블을 해제하고, pml4 엔트리를 초기화
		pml4e[idx] = 0;
	}
	return pte;					// pte pointer 반환
}

/*
이 함수는 페이지 맵 레벨 4 엔트리를 통해 가상 주소에 대한 페이지 테이블 엔트리를 찾거나 생성하는 역할을 합니다.
여기서 가상 주소는 va로 전달되며, create 플래그가 설정되어 있는 경우 새로운 페이지 테이블을 만들어야 할지 여부를 결정합니다.

먼저, 함수는 va에 대한 페이지 맵 레벨 4 엔트리의 인덱스를 계산합니다. 그런 다음, 해당 인덱스로부터 페이지 맵 레벨 4 엔트리를 가져옵니다.
다음으로, 가져온 페이지 맵 레벨 4 엔트리가 유효한지 확인합니다. 만약 해당 엔트리가 유효하지 않으면, 새 페이지 테이블을 생성해야 할 지를 create 플래그에 따라 결정합니다.
새 페이지 테이블을 생성하고 엔트리를 설정한 후, 새 페이지 테이블의 가상 주소에 대한 페이지 디렉터리 포인터를 반환합니다.

그러나 가져온 페이지 맵 레벨 4 엔트리가 유효한 경우, 해당 엔트리의 주소를 사용하여 페이지 디렉터리 포인터 테이블로 이동합니다.
여기서 pdpe_walk 함수를 호출하여 가상 주소에 대한 페이지 테이블 엔트리를 찾거나 생성합니다.

마지막으로, 반환된 페이지 테이블 엔트리가 NULL이고 새로운 페이지 테이블이 할당되었으면, 해당 페이지 테이블을 해제하고 페이지 맵 레벨 4 엔트리를 초기화합니다.
그런 다음 NULL을 반환하거나, 이미 유효한 페이지 테이블 엔트리가 반환됩니다.
*/


/* Creates a new page map level 4 (pml4) has mappings for kernel
 * virtual addresses, but none for user virtual addresses.
 * Returns the new page directory, or a null pointer if memory
 * allocation fails. */
uint64_t *
pml4_create (void) {
	uint64_t *pml4 = palloc_get_page (0);
	if (pml4)
		memcpy (pml4, base_pml4, PGSIZE);
	return pml4;
}

static bool
pt_for_each (uint64_t *pt, pte_for_each_func *func, void *aux,
		unsigned pml4_index, unsigned pdp_index, unsigned pdx_index) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = &pt[i];
		if (((uint64_t) *pte) & PTE_P) {
			void *va = (void *) (((uint64_t) pml4_index << PML4SHIFT) |
								 ((uint64_t) pdp_index << PDPESHIFT) |
								 ((uint64_t) pdx_index << PDXSHIFT) |
								 ((uint64_t) i << PTXSHIFT));
			if (!func (pte, va, aux))
				return false;
		}
	}
	return true;
}

static bool
pgdir_for_each (uint64_t *pdp, pte_for_each_func *func, void *aux,
		unsigned pml4_index, unsigned pdp_index) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pte) & PTE_P)
			if (!pt_for_each ((uint64_t *) PTE_ADDR (pte), func, aux,
					pml4_index, pdp_index, i))
				return false;
	}
	return true;
}

static bool
pdp_for_each (uint64_t *pdp,
		pte_for_each_func *func, void *aux, unsigned pml4_index) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pde = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pde) & PTE_P)
			if (!pgdir_for_each ((uint64_t *) PTE_ADDR (pde), func,
					 aux, pml4_index, i))
				return false;
	}
	return true;
}

/* Apply FUNC to each available pte entries including kernel's. */
/* FUNC을 커널을 포함한 각 사용 가능한 페이지 테이블 엔트리에 적용 */
bool pml4_for_each (uint64_t *pml4, pte_for_each_func *func, void *aux) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pdpe = ptov((uint64_t *) pml4[i]);
		if (((uint64_t) pdpe) & PTE_P)
			if (!pdp_for_each ((uint64_t *) PTE_ADDR (pdpe), func, aux, i))
				return false;
	}
	return true;
}

static void pt_destroy (uint64_t *pt) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pt[i]);
		if (((uint64_t) pte) & PTE_P)
			palloc_free_page ((void *) PTE_ADDR (pte));
	}
	palloc_free_page ((void *) pt);
}

static void pgdir_destroy (uint64_t *pdp) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pte) & PTE_P)
			pt_destroy (PTE_ADDR (pte));
	}
	palloc_free_page ((void *) pdp);
}

static void pdpe_destroy (uint64_t *pdpe) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pde = ptov((uint64_t *) pdpe[i]);
		if (((uint64_t) pde) & PTE_P)
			pgdir_destroy ((void *) PTE_ADDR (pde));
	}
	palloc_free_page ((void *) pdpe);
}

/* Destroys pml4e, freeing all the pages it references. */
/* PML4e를 파괴하고 해당하는 모든 페이지를 해제합니다. */
void pml4_destroy (uint64_t *pml4) {
	if (pml4 == NULL)
		return;
	ASSERT (pml4 != base_pml4);

	/* if PML4 (vaddr) >= 1, it's kernel space by define. */
	/* PML4 (vaddr)가 1보다 크거나 같으면 커널 공간입니다. */
	uint64_t *pdpe = ptov ((uint64_t *) pml4[0]);
	if (((uint64_t) pdpe) & PTE_P)
		pdpe_destroy ((void *) PTE_ADDR (pdpe));
	palloc_free_page ((void *) pml4);
}

/* Loads page directory PD into the CPU's page directory base register. */
/* 페이지 디렉터리 PD를 CPU의 페이지 디렉터리 베이스 레지스터로 로드합니다. */
void pml4_activate (uint64_t *pml4) {
	lcr3 (vtop (pml4 ? pml4 : base_pml4));
}

/* Looks up the physical address that corresponds to user virtual
 * address UADDR in pml4.  Returns the kernel virtual address
 * corresponding to that physical address, or a null pointer if UADDR is unmapped. */
/* pml4에서 사용자 가상 주소 UADDR에 해당하는 물리 주소를 찾습니다.
해당하는 물리 주소에 대응하는 커널 가상 주소를 반환하거나, UADDR이 매핑되지 않은 경우 null 포인터를 반환합니다. */
void *
pml4_get_page (uint64_t *pml4, const void *uaddr) {
	ASSERT (is_user_vaddr (uaddr));

	uint64_t *pte = pml4e_walk (pml4, (uint64_t) uaddr, 0);

	if (pte && (*pte & PTE_P))
		return ptov (PTE_ADDR (*pte)) + pg_ofs (uaddr);
	return NULL;
}

/* Adds a mapping in page map level 4 PML4 from user virtual page
 * UPAGE to the physical frame identified by kernel virtual address KPAGE.
 * UPAGE must not already be mapped. KPAGE should probably be a page obtained
 * from the user pool with palloc_get_page().
 * If WRITABLE is true, the new page is read/write;
 * otherwise it is read-only.
 * Returns true if successful, false if memory allocation
 * failed. */


/* 사용자 가상 페이지 UPAGE에서 커널 가상 주소 KPAGE가 식별하는 물리 프레임으로 PML4의 페이지 맵 레벨 4에 매핑을 추가합니다.
UPAGE가 이미 매핑되어 있으면 안됩니다. KPAGE는 일반적으로 palloc_get_page()를 사용하여 사용자 풀에서 얻은 페이지여야 합니다.
WRITABLE이 true이면 새 페이지는 읽기/쓰기가 가능하며, 그렇지 않으면 읽기 전용입니다.
성공하면 true를 반환하고, 메모리 할당에 실패하면 false를 반환합니다. */
bool pml4_set_page (uint64_t *pml4, void *upage, void *kpage, bool rw) {
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (pg_ofs (kpage) == 0);
	ASSERT (is_user_vaddr (upage));
	ASSERT (pml4 != base_pml4);

	uint64_t *pte = pml4e_walk (pml4, (uint64_t) upage, 1);

	if (pte)
		*pte = vtop (kpage) | PTE_P | (rw ? PTE_W : 0) | PTE_U;
	return pte != NULL;
}

/* Marks user virtual page UPAGE "not present" in page
 * directory PD.  Later accesses to the page will fault.  Other
 * bits in the page table entry are preserved. UPAGE need not be mapped. */
/* 페이지 디렉터리 PD에서 사용자 가상 페이지 UPAGE의 PTE를 "not present"로 표시합니다.
나중에 페이지에 액세스하면 오류가 발생합니다. 페이지 테이블 엔트리의 다른 비트는 보존됩니다.
UPAGE가 매핑되어 있지 않아도 됩니다. */
void pml4_clear_page (uint64_t *pml4, void *upage) {
	uint64_t *pte;
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (is_user_vaddr (upage));

	pte = pml4e_walk (pml4, (uint64_t) upage, false);

	if (pte != NULL && (*pte & PTE_P) != 0) {
		*pte &= ~PTE_P;
		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) upage);
	}
}

/* Returns true if the PTE for virtual page VPAGE in PML4 is dirty,
 * that is, if the page has been modified since the PTE was installed.
 * Returns false if PML4 contains no PTE for VPAGE. */
/* PML4의 VPAGE에 대한 가상 페이지 PTE가 최근에 수정되었는지 여부를 반환합니다.
PML4에 VPAGE에 대한 PTE가 없으면 false를 반환합니다. */
bool pml4_is_dirty (uint64_t *pml4, const void *vpage) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	return pte != NULL && (*pte & PTE_D) != 0;
}

/* Set the dirty bit to DIRTY in the PTE for virtual page VPAGE in PML4. */
/* PML4의 VPAGE에 대한 가상 페이지 PTE의 dirty 비트를 DIRTY로 설정합니다. */
void pml4_set_dirty (uint64_t *pml4, const void *vpage, bool dirty) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	if (pte) {
		if (dirty)
			*pte |= PTE_D;
		else
			*pte &= ~(uint32_t) PTE_D;

		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) vpage);
	}
}

/* Returns true if the PTE for virtual page VPAGE in PML4 has been
 * accessed recently, that is, between the time the PTE was
 * installed and the last time it was cleared.  Returns false if
 * PML4 contains no PTE for VPAGE. */

/* PML4의 VPAGE에 대한 가상 페이지 PTE가 최근에 액세스되었는지 여부를 반환합니다.
   PML4에 VPAGE에 대한 PTE가 없으면 false를 반환합니다. */
bool pml4_is_accessed (uint64_t *pml4, const void *vpage) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	return pte != NULL && (*pte & PTE_A) != 0;
}

/* Sets the accessed bit to ACCESSED in the PTE for virtual page VPAGE in PD. */
/* PML4의 VPAGE에 대한 가상 페이지 PTE의 액세스된 비트를 ACCESSED로 설정합니다. */
void pml4_set_accessed (uint64_t *pml4, const void *vpage, bool accessed) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	if (pte) {
		if (accessed)
			*pte |= PTE_A;
		else
			*pte &= ~(uint32_t) PTE_A;

		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) vpage);
	}
}
