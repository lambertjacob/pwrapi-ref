/*
 * Copyright 2024 Ethan Silver, Sam Emard-Thibault, Jacob Lambert, Severn
 * Lortie.
 */

#ifndef PWR_CAC_H
#define PWR_CAC_H

#include "pwrdev.h"

#ifdef __cplusplus
extern "C" {
#endif

plugin_devops_t *cac_init(const char *initstr);

int cac_final(plugin_devops_t *dev);

pwr_fd_t cac_open(plugin_devops_t *dev, const char *openstr);

int cac_close(pwr_fd_t fd);

int cac_read(pwr_fd_t fd, PWR_AttrName attr, void *value,
                      unsigned int len, PWR_Time *timestamp);

int cac_write(pwr_fd_t fd, PWR_AttrName attr, void *value,
                       unsigned int len);

#ifdef __cplusplus
}
#endif

#endif
