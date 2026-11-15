#include "hook.h"
#include <string.h>

#define CR0_WP (1UL << 16)

static inline uint64_t read_cr0(void)
{
    uint64_t v;
    __asm__ volatile("mov %%cr0, %0" : "=r"(v));
    return v;
}

static inline void write_cr0(uint64_t v)
{
    __asm__ volatile("mov %0, %%cr0" :: "r"(v) : "memory");
}

/* Ersetzt den Funktionsanfang durch "mov rax, detour ; jmp rax".
   Kein Trampoline, weil der Aufrufer nach unserem ret direkt
   wieder in seinen eigenen Code zurueckspringt. */
void hook_install(uintptr_t target, void *detour, hook_t *h)
{
    if (!target || !detour || !h) return;

    h->target = target;
    memcpy(h->orig, (void *)target, HOOK_LEN);

    uint8_t patch[HOOK_LEN] = { 0x48, 0xB8 };       /* mov rax, imm64 */
    memcpy(patch + 2, &detour, 8);
    patch[10] = 0xFF;                                /* jmp rax */
    patch[11] = 0xE0;

    uint64_t cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);                        /* WP aus, Kernel-Text ist read-only */
    memcpy((void *)target, patch, HOOK_LEN);
    __asm__ volatile("mfence" ::: "memory");
    write_cr0(cr0);

    h->installed = true;
}

void hook_remove(hook_t *h)
{
    if (!h || !h->installed) return;

    uint64_t cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);
    memcpy((void *)h->target, h->orig, HOOK_LEN);
    __asm__ volatile("mfence" ::: "memory");
    write_cr0(cr0);

    h->installed = false;
}