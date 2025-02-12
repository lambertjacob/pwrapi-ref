/*
 * Copyright 2024 Ethan Silver, Jacob Lambert, Severn Lortie, Sam
 * Emard-Thibault.
 */

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include "pwr_dev.h"
#include "pwr_toiletdev.h"

#define MAX_C_STATES 5
#define MAX_CORES 128
#define MAX_PKGS 4

typedef struct {
  int coreid;
  double cur_freq;
  double max_freq;
  double min_freq;
  double cpuidle[MAX_C_STATES]; // CPU idle states.
} toilet_core_t;
#define toilet_CORE(X) ((toilet_core_t *)(X))

typedef struct {
  int pkgid;
  int num_cores;
  double energy;
  double start_energy;
  uint64_t start_time;
  toilet_core_t cores[MAX_CORES];
} toilet_pkg_t;
#define toilet_PKG(X) ((toilet_pkg_t *)(X))

typedef struct {
  int num_pkgs;
  toilet_pkg_t pkgs[MAX_PKGS];
} toilet_node_t;
#define toilet_NODE(X) ((toilet_node_t *)(X))

// If type = 1, package, else core.
typedef struct {
  int num;
  int type;
  void *obj; // This can either be a node or a core.
} toilet_fd_t;
#define toilet_FD(X) ((toilet_fd_t *)(X))

plugin_devops_t devops = {
    .open = toilet_open,
    .close = toilet_close,
    .read = toilet_read,
    .write = toilet_write,
    .private_data = 0x0,
};

plugin_dev_t dev = {
    .init = toilet_init,
    .final = toilet_final,
};

plugin_dev_t *getDev() { return &dev; }

plugin_devops_t *toilet_init(const char *initstr) {
  DBGP("initstr='%s'\n", initstr);
  plugin_devops_t *dev = (plugin_devops_t *)malloc(sizeof(plugin_devops_t));
  *dev = devops;

  dev->private_data = malloc(sizeof(toilet_node_t));
  bzero(dev->private_data, sizeof(toilet_node_t));
  return dev;
}

int toilet_final(plugin_devops_t *dev) {
  free(dev->private_data);
  free(dev);
  return PWR_RET_SUCCESS;
}

pwr_fd_t toilet_open(plugin_devops_t *dev, const char *openstr) {
  pwr_fd_t *fd = (pwr_fd_t *)malloc(sizeof(toilet_fd_t));
  bzero(fd, sizeof(toilet_fd_t));
  char path[256] = "", strval[10] = "userspace";
  int file;
  if (strstr(openstr, "pkg") != NULL) {
    toilet_FD(fd)->obj = malloc(sizeof(toilet_pkg_t));
    toilet_FD(fd)->type = 1;
    sscanf(openstr, "pkg%d", &(toilet_FD(fd))->num);
  } else if (strstr(openstr, "core") != NULL) {
    toilet_FD(fd)->obj = malloc(sizeof(toilet_core_t));
    toilet_FD(fd)->type = 0;
    sscanf(openstr, "core%d", &(toilet_FD(fd))->num);
    snprintf(path, 255,
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",
             toilet_FD(fd)->num);
    file = open(path, O_WRONLY);
    if (file < 0) {
      DBGP("Error: unable to open CPU file at %s\n", path);
      return PWR_RET_FAILURE;
    }
    DBGP("Writing attribute to file %s\n", path);
    if (write(file, strval, 100) < 0) {
      DBGP("Error: unable to write scaling_governor.\n");
      close(file);
      return PWR_RET_FAILURE;
    }
  }
  return fd;
}

int toilet_close(pwr_fd_t fd) {
  char path[256] = "", strval[10] = "schedutil";
  int file;
  snprintf(path, 255, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",
           toilet_FD(fd)->num);
  file = open(path, O_WRONLY);
  if (file < 0) {
    DBGP("Error: unable to open CPU file at %s\n", path);
    return PWR_RET_FAILURE;
  }
  DBGP("Writing attribute to file %s\n", path);
  if (write(file, strval, 100) < 0) {
    DBGP("Error: unable to write scaling_governor.\n");
    close(file);
    return PWR_RET_FAILURE;
  }
  free(fd);
  return PWR_RET_SUCCESS;
}

int toilet_read(pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len,
                PWR_Time *timestamp) {
  struct timeval tv;
  char path[256] = "", strval[101] = "";
  int file;
  if (len != sizeof(int64_t)) {
    DBGP("Error: value field size of %u incorrect, should be %ld\n", len,
         sizeof(unsigned long long));
    return PWR_RET_FAILURE;
  }
  if (toilet_FD(fd)->type == 1) {
    switch (attr) {
    case PWR_ATTR_ENERGY: {
      snprintf(
          path, 255,
          "/sys/devices/virtual/powercap/intel-rapl/intel-rapl\:%d/energy_uj",
          toilet_FD(fd)->num);
    } break;
    default: {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  } else {
    switch (attr) {
    case PWR_ATTR_FREQ: {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
               toilet_FD(fd)->num);
    } break;
    case PWR_ATTR_FREQ_LIMIT_MAX: {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",
               toilet_FD(fd)->num);
    } break;
    case PWR_ATTR_FREQ_LIMIT_MIN: {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",
               toilet_FD(fd)->num);
    } break;
    default: {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  }
  file = open(path, O_RDONLY);
  if (file < 0) {
    DBGP("Error: unable to open CPU file at %s\n", path);
    return PWR_RET_FAILURE;
  }
  DBGP("Reading attribute from file %s\n", path);
  if (read(file, strval, 100) < 0) {
    DBGP("Error: unable to read PM counter.\n");
    close(file);
    return PWR_RET_FAILURE;
  }
  sscanf(strval, "%llu", value);
  close(file);
  gettimeofday(&tv, NULL);
  *timestamp = tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
  DBGP("Info: reading of type %u at time %llu with value %llu\n", attr,
       *(unsigned long long *)timestamp, (uint64_t)&value);
  return PWR_RET_SUCCESS;
}

int toilet_write(pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len,
                 PWR_Time *timestamp) {
  struct timeval tv;
  char path[256] = "", strval[20] = "";
  int file;
  if (len != sizeof(int64_t)) {
    DBGP("Error: value field size of %u incorrect, should be %ld\n", len,
         sizeof(unsigned long long));
    return PWR_RET_FAILURE;
  }
  if (toilet_FD(fd)->type == 1) {
    switch (attr) {
    case PWR_ATTR_ENERGY: {
      snprintf(
          path, 255,
          "/sys/devices/virtual/powercap/intel-rapl/intel-rapl\:%d/energy_uj",
          toilet_FD(fd)->num);
    } break;
    default: {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  } else {
    switch (attr) {
    case PWR_ATTR_FREQ: {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
               toilet_FD(fd)->num);
    } break;
    case PWR_ATTR_FREQ_LIMIT_MAX: {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",
               toilet_FD(fd)->num);
    } break;
    case PWR_ATTR_FREQ_LIMIT_MIN: {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",
               toilet_FD(fd)->num);
    } break;
    default: {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  }
  sprintf(strval, "%llu", &value);
  file = open(path, O_WRONLY);
  if (file < 0) {
    DBGP("Error: unable to open CPU file at %s\n", path);
    return PWR_RET_FAILURE;
  }
  DBGP("Writing attribute to file %s\n", path);
  if (write(file, strval, 100) < 0) {
    DBGP("Error: unable to read PM counter.\n");
    close(file);
    return PWR_RET_FAILURE;
  }
  close(file);
  gettimeofday(&tv, NULL);
  *timestamp = tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
  DBGP("Info: Writing type %u at time %llu with value %llu\n", attr,
       *(unsigned long long *)timestamp, (uint64_t)&value);
  return PWR_RET_SUCCESS;
}

static int pwr_toiletdev_numObjs() {
  DBGP("\n");
  return 1;
}

static int pwr_toiletdev_readObjs(int i, PWR_ObjType *ptr) {
  DBGP("\n");
  ptr[0] = PWR_OBJ_SOCKET;
  return 0;
}

static int pwr_toiletdev_numAttrs(PWR_ObjType type) {
  DBGP("\n");
  return 2;
}

static int pwr_toiletdev_readAttrs(PWR_ObjType type, int i, PWR_AttrName *ptr) {
  DBGP("\n");
  ptr[0] = PWR_ATTR_POWER;
  ptr[0] = PWR_ATTR_FREQ;
  return 0;
}

static int pwr_toiletdev_getDevName(PWR_ObjType type, size_t len, char *buf) {
  strncpy(buf, "cpu_dev0", len);
  DBGP("type=%d name=`%s`\n", type, buf);
  return 0;
}

static int pwr_toiletdev_getDevOpenStr(PWR_ObjType type, int global_index,
                                       size_t len, char *buf) {
  snprintf(buf, len, "%d", global_index);
  DBGP("type=%d global_index=%d str=`%s`\n", type, global_index, buf);
  return 0;
}

static int pwr_toiletdev_getDevInitStr(const char *name, size_t len,
                                       char *buf) {
  strncpy(buf, "", len);
  DBGP("dev=`%s` str=`%s`\n", name, buf);
  return 0;
}

static int pwr_toiletdev_getPluginName(size_t len, char *buf) {
  strncpy(buf, "CPU", len);
  return 0;
}

static plugin_meta_t meta = {
    .numObjs = pwr_toiletdev_numObjs,
    .numAttrs = pwr_toiletdev_numAttrs,
    .readObjs = pwr_toiletdev_readObjs,
    .readAttrs = pwr_toiletdev_readAttrs,
    .getDevName = pwr_toiletdev_getDevName,
    .getDevOpenStr = pwr_toiletdev_getDevOpenStr,
    .getDevInitStr = pwr_toiletdev_getDevInitStr,
    .getPluginName = pwr_toiletdev_getPluginName,
};

plugin_meta_t *getMeta() { return &meta; }
