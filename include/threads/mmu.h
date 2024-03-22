#ifndef THREAD_MMU_H
#define THREAD_MMU_H

#include <stdbool.h>
#include <stdint.h>
#include "threads/pte.h"

typedef bool pte_for_each_func (uint64_t *pte, void *va, void *aux);

uint64_t *pml4e_walk (uint64_t *pml4, const uint64_t va, int create);
uint64_t *pml4_create (void);

/* 각 PML4 아래에 있는 valid entry 마다, 함수 func에 보조값 AUX를 계산,
가상주소(VA)는 엔트리 가상주소에 해당함. each_func의 결과가 false라면, 반복을 멈추고 false 반환 */
bool pml4_for_each (uint64_t *, pte_for_each_func *, void *);
void pml4_destroy (uint64_t *pml4);
void pml4_activate (uint64_t *pml4);
void *pml4_get_page (uint64_t *pml4, const void *upage);
bool pml4_set_page (uint64_t *pml4, void *upage, void *kpage, bool rw);
void pml4_clear_page (uint64_t *pml4, void *upage);
bool pml4_is_dirty (uint64_t *pml4, const void *upage);
void pml4_set_dirty (uint64_t *pml4, const void *upage, bool dirty);
bool pml4_is_accessed (uint64_t *pml4, const void *upage);
void pml4_set_accessed (uint64_t *pml4, const void *upage, bool accessed);

/* pte가 가리키는 가상 주소가 작성 가능한지 아닌지를 확인 */
#define is_writable(pte) (*(pte) & PTE_W)

/* 페이지 테이블 엔트리의 주인이 유저인지 커널인지 확인 */
#define is_user_pte(pte) (*(pte) & PTE_U)
#define is_kern_pte(pte) (!is_user_pte (pte))

#define pte_get_paddr(pte) (pg_round_down(*(pte)))

/* Segment descriptors for x86-64. */
struct desc_ptr {
	uint16_t size;
	uint64_t address;
} __attribute__((packed));

#endif /* thread/mm.h */
