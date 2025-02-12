/*
 * Copyright 2024 Ethan Silver, Sam Emard-Thibault, Jacob Lambert, Severn
 * Lortie.
 */

#ifndef PWR_TOILET_H
#define PWR_TOILET_H

#include "pwrdev.h"

#ifdef __cplusplus
extern "C" {
#endif

plugin_devops_t *toilet_init(const char *initstr);

int toilet_final(plugin_devops_t *dev);

pwr_fd_t toilet_open(plugin_devops_t *dev, const char *openstr);

int toilet_close(pwr_fd_t fd);

int toilet_read(pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len,
                PWR_Time *timestamp);

int toilet_write(pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len,
                 PWR_Time *timestamp);

#ifdef __cplusplus
}
#endif

#endif
