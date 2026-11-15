#ifndef HOOK_H
#define HOOK_H
#include <stdint.h>
#include <stdbool.h>

#define HOOK_LEN 12   /* mov rax, imm64 ; jmp rax */

typedef struct {
    uintptr_t target;
    uint8_t   orig[HOOK_LEN];
    bool      installed;
} hook_t;

void hook_install(uintptr_t target, void *detour, hook_t *h);
void hook_remove(hook_t *h);

#endif