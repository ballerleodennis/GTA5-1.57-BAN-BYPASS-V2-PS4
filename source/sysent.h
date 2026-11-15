#ifndef SYSENT_H
#define SYSENT_H
#include <stdint.h>
#include <stdbool.h>

/* FreeBSD-9-sysent: sy_narg(4) sy_flags(2) pad(2) sy_call(8) sy_thrcnt(4)
   -> Groesse 24, sy_call auf +8. Bei abweichendem Layout hier anpassen. */
#define SYENT_SIZE     24
#define SYENT_CALL_OFF 8

bool sysent_init(void);                 /* Tabelle finden + validieren */
bool sysent_hook(uint32_t nr, void *detour);
void sysent_unhook_all(void);
uint32_t stub_syscall_nr(uintptr_t stub);   /* liest "mov rax, imm" aus dem Stub */
#endif