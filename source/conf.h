#ifndef CONF_H
#define CONF_H
#include <stdint.h>

#define ID_LEN 16

typedef struct {
    uint8_t open_psid[ID_LEN];
    uint8_t console_id[ID_LEN];
    uint8_t psid[ID_LEN];
    uint32_t generation;
} spoof_ids_t;

/* Liest ids.conf. 1 = ok, 0 = Datei fehlt/kaputt */
int conf_load(const char *path, spoof_ids_t *out);

#endif