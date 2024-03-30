#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

// #include "include/threads/thread.h"
#include "include/threads/thread.h"

bool install_page (void *upage, void *kpage, bool writable);
bool lazy_load_segment (struct page *page, void *aux);

int process_create_initd (const char *file_name);
int process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

#endif /* userprog/process.h */
