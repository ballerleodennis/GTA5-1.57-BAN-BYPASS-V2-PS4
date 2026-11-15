#include <stdint.h>
#include <string.h>
#include <kernel.h>
#include <elf.h>
#include "hook.h"
#include "conf.h"
#include "sysent.h"

#ifndef GOLDHEN_PLUGIN_EXPORT
#define GOLDHEN_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define CONF_PATH "/data/GoldHEN/plugins/gta_ban_bypass/ids.conf"

#define NID_OPEN_PSID_FOR_SYSTEM 0x567D9A86u
#define NID_PSID                 0x2E4B1F2Fu

static spoof_ids_t g_ids;

/* --- Export-Ebene (nicht-inline Aufrufe) --- */

static hook_t g_hook_open_psid, g_hook_psid;

static int detour_open_psid(void *out)
{
    if (out) memcpy(out, g_ids.open_psid, ID_LEN);
    return 0;
}

static int detour_psid(void *out)
{
    if (out) memcpy(out, g_ids.psid, ID_LEN);
    return 0;
}

/* --- Syscall-Ebene (inline Aufrufe) ---
   uap zeigt auf ein Struct mit dem User-Zeiger als erstem Feld. */

static int sys_detour_open_psid(void *td, void *uap)
{
    (void)td;
    void *out = uap ? *(void **)uap : NULL;
    if (out) memcpy(out, g_ids.open_psid, ID_LEN);
    return 0;
}

static int sys_detour_psid(void *td, void *uap)
{
    (void)td;
    void *out = uap ? *(void **)uap : NULL;
    if (out) memcpy(out, g_ids.psid, ID_LEN);
    return 0;
}

/* --- Auflösung --- */

static uintptr_t libkernel_base(void)
{
    int handles[256];
    size_t count = 0;
    if (sceKernelGetModuleList(0, handles, 256, &count) != 0) return 0;
    for (size_t i = 0; i < count; i++) {
        SceKernelModuleInfo info;
        memset(&info, 0, sizeof info);
        info.size = sizeof info;
        if (sceKernelGetModuleInfo(handles[i], &info) != 0) continue;
        if (!strncmp(info.name, "libkernel", 9))
            return (uintptr_t)info.segmentInfo[0].address;
    }
    return 0;
}

static uintptr_t resolve_nid(uintptr_t base, uint32_t nid, const char *name)
{
    Elf64_Ehdr *eh = (Elf64_Ehdr *)base;
    Elf64_Phdr *ph = (Elf64_Phdr *)(base + eh->e_phoff);
    Elf64_Dyn  *dyn = NULL;
    Elf64_Sym  *symtab = NULL;
    const char *strtab = NULL;
    uint32_t nsyms = 0;

    for (int i = 0; i < eh->e_phnum; i++)
        if (ph[i].p_type == PT_DYNAMIC)
            dyn = (Elf64_Dyn *)(base + ph[i].p_vaddr);
    if (!dyn) return 0;

    for (Elf64_Dyn *d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
        case DT_SYMTAB: symtab = (Elf64_Sym *)(base + d->d_un.d_ptr); break;
        case DT_STRTAB: strtab = (const char *)(base + d->d_un.d_ptr); break;
        case DT_HASH:   nsyms  = ((uint32_t *)(base + d->d_un.d_ptr))[1]; break;
        }
    }
    if (!symtab || !strtab || !nsyms) return 0;

    char lo[16], hi[16];
    snprintf(lo, sizeof lo, "%08x", nid);
    snprintf(hi, sizeof hi, "%08x", __builtin_bswap32(nid));

    for (uint32_t i = 1; i < nsyms; i++) {
        const char *s = strtab + symtab[i].st_name;
        if (!strcmp(s, name) || !strcmp(s, lo) || !strcmp(s, hi))
            return base + symtab[i].st_value;
    }
    return 0;
}

/* Beide Ebenen fuer eine Funktion setzen */
static void install_pair(uintptr_t base, uint32_t nid, const char *name,
                         void *export_detour, void *sys_detour, hook_t *h)
{
    uintptr_t stub = resolve_nid(base, nid, name);
    if (!stub) {
        klog("[gta_ban_bypass] %s nicht aufgeloest\n", name);
        return;
    }

    hook_install(stub, export_detour, h);

    uint32_t nr = stub_syscall_nr(stub);
    if (nr == 0xFFFFFFFFu) {
        klog("[gta_ban_bypass] %s: Stub-Form unbekannt, nur Export-Hook\n", name);
        return;
    }
    if (!sysent_hook(nr, sys_detour))
        klog("[gta_ban_bypass] %s: syscall %u nicht gehookt\n", name, nr);
}

GOLDHEN_PLUGIN_EXPORT int module_start(size_t argc, const void *argv)
{
    (void)argc; (void)argv;

    if (!conf_load(CONF_PATH, &g_ids) || !g_ids.generation) {
        klog("[gta_ban_bypass] keine gueltige ids.conf\n");
        return 0;
    }

    uintptr_t base = libkernel_base();
    if (!base) return 0;

    bool have_table = sysent_init();

    install_pair(base, NID_OPEN_PSID_FOR_SYSTEM, "sceKernelGetOpenPsIdForSystem",
                 detour_open_psid, sys_detour_open_psid, &g_hook_open_psid);
    install_pair(base, NID_PSID, "sceKernelGetPsid",
                 detour_psid, sys_detour_psid, &g_hook_psid);

    klog("[gta_ban_bypass] Generation %u, sysent=%s\n",
         g_ids.generation, have_table ? "ok" : "nicht gefunden");
    return 0;
}

GOLDHEN_PLUGIN_EXPORT int module_stop(size_t argc, const void *argv)
{
    (void)argc; (void)argv;
    hook_remove(&g_hook_open_psid);
    hook_remove(&g_hook_psid);
    sysent_unhook_all();
    klog("[gta_ban_bypass] alles zurueckgesetzt\n");
    return 0;
}