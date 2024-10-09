/*
 * Copyright 2024 Ethan Silver.
 */

#ifndef PWR_POWERMETRICS_H
#define PWR_POWERMETRICS_H

#include "pwrdev.h"

#ifdef __cplusplus
extern "C" {
#endif

plugin_devops_t *pwr_powermetrics_init(const char *initstr);
int pwr_powermetrics_final(plugin_devops_t *dev);

pwr_fd_t pwr_powermetrics_open(plugin_devops_t *dev, const char *openstr);
int pwr_powermetrics_close(pwr_fd_t fd);

int pwr_powermetrics_read(pwr_fd_t fd, PWR_AttrName attr, void *value,
                    unsigned int len, PWR_Time *timestamp);
int pwr_powermetrics_write(pwr_fd_t fd, PWR_AttrName attr, void *value,
                     unsigned int len);

int pwr_powermetrics_readv(pwr_fd_t fd, unsigned int arraysize,
                     const PWR_AttrName attrs[], void *values,
                     PWR_Time timestamp[], int status[]);
int pwr_powermetrics_writev(pwr_fd_t fd, unsigned int arraysize,
                      const PWR_AttrName attrs[], void *values, int status[]);

int pwr_powermetrics_time(pwr_fd_t fd, PWR_Time *timestamp);
int pwr_powermetrics_clear(pwr_fd_t fd);

#ifdef __cplusplus
}
#endif

#endif
