#include "sysent.h"
#include <stddef.h>
#include <string.h>
#include <kernel.h>

#define CR0_WP     (1UL << 16)
#define SCAN_LIMIT (8u * 1024u * 1024u)
#define VALIDATE_N 32
#define MAX_HOOKS  8

typedef struct {
    uint32_t nr;
    void    *orig;
    bool     used;
} sysent_slot_t;

static uintptr_t g_table;
static uintptr_t g_text_lo, g_text_hi;
static sysent_slot_t g_slots[MAX_HOOKS];

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

static void kwrite64(uintptr_t addr, uint64_t val)
{
    uint64_t cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);
    *(volatile uint64_t *)addr = val;
    __asm__ volatile("mfence" ::: "memory");
    write_cr0(cr0);
}

static bool in_text(uintptr_t a)
{
    return a >= g_text_lo && a < g_text_hi;
}

/* Kernel-Modul finden, Text-/Datengrenzen setzen */
static bool kernel_ranges(void)
{
    int handles[256];
    size_t count = 0;

    if (sceKernelGetModuleList(0, handles, 256, &count) != 0) return false;

    for (size_t i = 0; i < count; i++) {
        SceKernelModuleInfo info;
        memset(&info, 0, sizeof info);
        info.size = sizeof info;
        if (sceKernelGetModuleInfo(handles[i], &info) != 0) continue;
        if (strncmp(info.name, "kernel", 6)) continue;

        g_text_lo = (uintptr_t)info.segmentInfo[0].address;
        g_text_hi = g_text_lo + info.segmentInfo[0].size;
        return true;
    }
    return false;
}

static bool table_plausible(uintptr_t t)
{
    for (uint32_t i = 1; i <= VALIDATE_N; i++) {
        uint64_t h = *(volatile uint64_t *)(t + i * SYENT_SIZE + SYENT_CALL_OFF);
        if (!in_text((uintptr_t)h)) return false;
    }
    return true;
}

bool sysent_init(void)
{
    if (!kernel_ranges()) {
        klog("[gta_ban_bypass] Kernel-Modul nicht gefunden\n");
        return false;
    }

    uintptr_t data = 0, data_sz = 0;
    int handles[256];
    size_t count = 0;
    if (sceKernelGetModuleList(0, handles, 256, &count) == 0) {
        for (size_t i = 0; i < count; i++) {
            SceKernelModuleInfo info;
            memset(&info, 0, sizeof info);
            info.size = sizeof info;
            if (sceKernelGetModuleInfo(handles[i], &info) != 0) continue;
            if (!strncmp(info.name, "kernel", 6)) {
                data = (uintptr_t)info.segmentInfo[1].address;
                data_sz = info.segmentInfo[1].size;
                break;
            }
        }
    }
    if (!data) return false;

    if (data_sz > SCAN_LIMIT) data_sz = SCAN_LIMIT;

    for (uintptr_t p = data; p + VALIDATE_N * SYENT_SIZE < data + data_sz; p += 8) {
        uintptr_t cand = *(volatile uint64_t *)p;
        if (cand < data || cand > data + data_sz) continue;
        if (table_plausible(cand)) {
            g_table = cand;
            klog("[gta_ban_bypass] sysent bei %p\n", (void *)cand);
            return true;
        }
    }

    klog("[gta_ban_bypass] sysent nicht gefunden\n");
    return false;
}

uint32_t stub_syscall_nr(uintptr_t stub)
{
    const uint8_t *p = (const uint8_t *)stub;
    if (p[0] == 0x48 && p[1] == 0xC7 && p[2] == 0xC0)              /* mov rax, imm32 */
        return *(const uint32_t *)(p + 3);
    if (p[0] == 0xB8)                                                /* mov eax, imm32 */
        return *(const uint32_t *)(p + 1);
    return 0xFFFFFFFFu;
}

bool sysent_hook(uint32_t nr, void *detour)
{
    if (!g_table || !detour || nr > 1100) return false;

    uintptr_t slot = g_table + nr * SYENT_SIZE + SYENT_CALL_OFF;
    void *orig = (void *)*(volatile uint64_t *)slot;
    if (!in_text((uintptr_t)orig) || orig == detour) return false;

    for (int i = 0; i < MAX_HOOKS; i++) {
        if (!g_slots[i].used) {
            g_slots[i].used = true;
            g_slots[i].nr   = nr;
            g_slots[i].orig = orig;
            kwrite64(slot, (uint64_t)(uintptr_t)detour);
            klog("[gta_ban_bypass] syscall %u gehookt\n", nr);
            return true;
        }
    }
    return false;
}

void sysent_unhook_all(void)
{
    if (!g_table) return;
    for (int i = 0; i < MAX_HOOKS; i++) {
        if (g_slots[i].used) {
            kwrite64(g_table + g_slots[i].nr * SYENT_SIZE + SYENT_CALL_OFF,
                     (uint64_t)(uintptr_t)g_slots[i].orig);
            g_slots[i].used = false;
        }
    }
}