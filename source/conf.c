#include "conf.h"
#include <kernel.h>
#include <string.h>

#define CONF_MAX 1024

static int hex2bin(const char *s, uint8_t *dst, int len)
{
    if (strlen(s) < (size_t)(len * 2)) return 0;
    for (int i = 0; i < len; i++) {
        unsigned int b;
        char pair[3] = { s[i * 2], s[i * 2 + 1], 0 };
        if (sscanf(pair, "%2x", &b) != 1) return 0;
        dst[i] = (uint8_t)b;
    }
    return 1;
}

int conf_load(const char *path, spoof_ids_t *out)
{
    char buf[CONF_MAX];
    int fd, n, ok = 0;

    memset(out, 0, sizeof *out);
    memset(buf, 0, sizeof buf);

    fd = sceKernelOpen(path, SCE_KERNEL_O_RDONLY, 0);
    if (fd < 0) return 0;

    n = sceKernelRead(fd, buf, CONF_MAX - 1);
    sceKernelClose(fd);
    if (n <= 0) return 0;
    buf[n] = 0;

    char *line = strtok(buf, "\n");
    while (line) {
        if (line[0] != '#') {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq++ = 0;
                if (!strcmp(line, "open_psid"))  ok |= hex2bin(eq, out->open_psid,  ID_LEN);
                if (!strcmp(line, "console_id")) ok |= hex2bin(eq, out->console_id, ID_LEN);
                if (!strcmp(line, "psid"))       ok |= hex2bin(eq, out->psid,       ID_LEN);
                if (!strcmp(line, "generation")) out->generation = (uint32_t)strtoul(eq, NULL, 10);
            }
        }
        line = strtok(NULL, "\n");
    }
    return ok;
}