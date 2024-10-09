/*
 * Copyright 2014-2016 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000, there is a non-exclusive license for use of this work
 * by or on behalf of the U.S. Government. Export of this program may require
 * a license from the United States Government.
 *
 * This file is part of the Power API Prototype software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <unistd.h>

#include "pwr_dev.h"
#include "pwr_powermetrics.h"
#include "util.h"

#define BUFFER_LEN 10

typedef struct {
  double values[BUFFER_LEN];
  PWR_Time timeStamps[BUFFER_LEN];
} buffer_t;

typedef struct {
  buffer_t buffers[PWR_NUM_ATTR_NAMES];
  double lastAccess;
} pwrmetricsfd_t;

static double getTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double value;
  value = tv.tv_sec * 1000000000;
  value += tv.tv_usec * 1000;
  return value;
}

static double getTimeSec() { return getTime() / 1000000000.0; }

static pwr_fd_t powermetrics_open(plugin_devops_t *ops, const char *openstr) {
  pwrmetricsfd_t *tmp = (pwrmetricsfd_t *)malloc(sizeof(pwrmetricsfd_t));

  int link[2];
  pid_t pid;
  char output[4096];
  assert(pipe(link) != -1);
  assert((pid = fork()) != -1);

    if (pid == 0) {
	dup2(link[1], STDOUT_FILENO);
	close(link[0]);
	close(link[1]);
    } else {
	close(link[1]);
	int nbytes = read(link[0], output, sizeof(output));
	printf("Output: (%.*s)\n", nbytes, output);
	wait(NULL);
    }

  tmp->buffers[PWR_ATTR_POWER].values[0] = 10.1234;
  tmp->buffers[PWR_ATTR_ENERGY].values[0] = 100000000;
  tmp->lastAccess = getTimeSec();
  DBGP("`%s` ptr=%p\n", openstr, tmp);
  return tmp;
}

static int powermetrics_close(pwr_fd_t fd) {
  DBGP("\n");
  free(fd);

  return 0;
}

static int powermetrics_read(pwr_fd_t fd, PWR_AttrName type, void *ptr,
                             unsigned int len, PWR_Time *ts) {
  double now = getTimeSec();
  pwrmetricsfd_t *info = (pwrmetricsfd_t *)fd;
  info->buffers[type].values[0] += (now - info->lastAccess) * JOULES_PER_SEC;

  info->lastAccess = now;

  *(double *)ptr = info->buffers[type].values[0];

  DBGP("type=%s %f\n", attrNameToString(type), *(double *)ptr);

  if (ts) {
    *ts = now;
  }

  return PWR_RET_SUCCESS;
}

static int powermetrics_write(pwr_fd_t fd, PWR_AttrName type, void *ptr,
                              unsigned int len) {
  pwrmetricsfd_t *info = fd;
  info->lastAccess = getTimeSec();
  DBGP("type=%s %f\n", attrNameToString(type), *(double *)ptr);

  info->buffers[type].values[0] = *(double *)ptr;
  return PWR_RET_SUCCESS;
}

static int powermetrics_readv(pwr_fd_t fd, unsigned int arraysize,
                              const PWR_AttrName attrs[], void *buf,
                              PWR_Time ts[], int status[]) {
  int i;
  for (i = 0; i < arraysize; i++) {

    ((double *)buf)[i] = ((pwrmetricsfd_t *)fd)->buffers[attrs[i]].values[0];

    DBGP("type=%s %f\n", attrNameToString(attrs[i]), ((double *)buf)[i]);

    ts[i] = getTime();

    status[i] = PWR_RET_SUCCESS;
  }
  return PWR_RET_SUCCESS;
}

static int powermetrics_writev(pwr_fd_t fd, unsigned int arraysize,
                               const PWR_AttrName attrs[], void *buf,
                               int status[]) {
  int i;
  DBGP("num attributes %d\n", arraysize);
  for (i = 0; i < arraysize; i++) {
    DBGP("type=%s %f\n", attrNameToString(attrs[i]), ((double *)buf)[i]);

    ((pwrmetricsfd_t *)fd)->buffers[attrs[i]].values[0] = ((double *)buf)[i];

    status[i] = PWR_RET_SUCCESS;
  }
  return PWR_RET_SUCCESS;
}

static int powermetrics_time(pwr_fd_t fd, PWR_Time *timestamp) {
  DBGP("\n");

  return 0;
}

static int powermetrics_clear(pwr_fd_t fd) {
  DBGP("\n");

  return 0;
}

static int powermetrics_log_start(pwr_fd_t fd, PWR_AttrName name) {
  buffer_t *ptr = &((pwrmetricsfd_t *)fd)->buffers[name];
  DBGP("\n");
  ptr->timeStamps[0] = getTime();
  return PWR_RET_SUCCESS;
}

static int powermetrics_log_stop(pwr_fd_t fd, PWR_AttrName name) {
  return PWR_RET_SUCCESS;
}

static int powermetrics_get_samples(pwr_fd_t fd, PWR_AttrName name,
                                    PWR_Time *ts, double period,
                                    unsigned int *nSamples, void *buf)

{
  DBGP("period=%f samples=%d\n", period, *nSamples);
  struct timeb tp;
  ftime(&tp);
  srand((unsigned)tp.millitm);
  int i;

  struct timeval tv;
  gettimeofday(&tv, NULL);

  srand(tv.tv_usec);
  for (i = 0; i < *nSamples; i++) {
    ((double *)buf)[i] = 100 + (float)rand() / (float)(RAND_MAX / 2.0);
    DBGP("%f\n", ((double *)buf)[i]);
  }
  *ts = getTime() - ((*nSamples * period) * 1000000000);
  DBGP("ts=%llu\n", *ts);
  return PWR_RET_SUCCESS;
}

static plugin_devops_t devOps = {
    .open = powermetrics_open,
    .close = powermetrics_close,
    .read = powermetrics_read,
    .write = powermetrics_write,
    .readv = powermetrics_readv,
    .writev = powermetrics_writev,
    .time = powermetrics_time,
    .clear = powermetrics_clear,
    .log_start = powermetrics_log_start,
    .log_stop = powermetrics_log_stop,
    .get_samples = powermetrics_get_samples,
};

static plugin_devops_t *powermetrics_init(const char *initstr) {
  DBGP("initstr='%s'\n", initstr);
  plugin_devops_t *ops = malloc(sizeof(*ops));
  *ops = devOps;
  ops->private_data = malloc(sizeof(pwrmetricsfd_t));
  return ops;
}

static int powermetrics_final(plugin_devops_t *ops) {
  DBGP("\n");
  free(ops->private_data);
  free(ops);
  return 0;
}

static plugin_dev_t dev = {
    .init = powermetrics_init,
    .final = powermetrics_final,
};

plugin_dev_t *getDev() { return &dev; }

static int powermetrics_getPluginName(size_t len, char *buf) {
  strncpy(buf, "PowerMetrics", len);
  return 0;
}

static int powermetrics_readObjs(int i, PWR_ObjType *ptr) {
  DBGP("\n");
  ptr[0] = PWR_OBJ_CORE;
  return 0;
}

// How may object types does this plugin support?
// this is the size of the array referenced by powermetrics_readObjs()
static int powermetrics_numObjs() {
  DBGP("\n");
  return 1;
}

// What attributes does an object support?
// Note that currently this function does not specify Object type
// so all objects must support all attributues
// Yes, this is limiting.
// This interface could be revised to remove this restriction by adding
// PWR_ObjType as a parameter.
static int powermetrics_readAttrs(PWR_ObjType type, int i, PWR_AttrName *ptr) {
  DBGP("\n");
  ptr[0] = PWR_ATTR_ENERGY;
  ptr[1] = PWR_ATTR_POWER;
  return 0;
}

// How many attributes does an object support?
// this is the size of the array referenced by powermetrics_readAttr()
static int powermetrics_numAttrs(PWR_ObjType type) {
  DBGP("\n");
  return 2;
}

// a plugin device can be initialized multiple times, once for each
// object type, however this is not necessry, you can have one device
// to handle all object types
static int powermetrics_getDevName(PWR_ObjType type, size_t len, char *buf) {
  strncpy(buf, "powermetrics0", len);
  DBGP("type=%d name=`%s`\n", type, buf);
  return 0;
}

// Create the device initialized string for the specified dev. The name
// was returned the the framework by powermetrics_getDevName()
static int powermetrics_getDevInitStr(const char *devName, size_t len,
                                      char *buf) {
  strncpy(buf, "", len);
  DBGP("dev=`%s` str=`%s`\n", devName, buf);
  return 0;
}

// a device can be opened multiple times, get the info to pass to the
// open call for this object type
static int powermetrics_getDevOpenStr(PWR_ObjType type, int global_index,
                                      size_t len, char *buf) {
  snprintf(buf, len, "%d %d", type, global_index);
  DBGP("type=%d global_index=%d str=`%s`\n", type, global_index, buf);
  return 0;
}

static plugin_meta_t meta = {
    .numObjs = powermetrics_numObjs,
    .numAttrs = powermetrics_numAttrs,
    .readObjs = powermetrics_readObjs,
    .readAttrs = powermetrics_readAttrs,
    .getDevName = powermetrics_getDevName,
    .getDevOpenStr = powermetrics_getDevOpenStr,
    .getDevInitStr = powermetrics_getDevInitStr,
    .getPluginName = powermetrics_getPluginName,
};

plugin_meta_t *getMeta() { return &meta; }
