/*
 * Copyright 2024 Ethan Silver, Jacob Lambert, Severn Lortie, Sam
 * Emard-Thibault.
 */

#include <assert.h>
#include <cstdio>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include "pwr_cacdev.h"
#include "pwr_dev.h"

#define MAX_C_STATES 5
#define MAX_CORES 128
#define MAX_PKGS 4

typedef struct {
  int coreid;
  double cur_freq;
  double max_freq;
  double min_freq;
  double cpuidle[MAX_C_STATES]; // CPU idle states.
} cac_core_t;
#define CAC_CORE(X) ((cac_core_t *)(X))

typedef struct {
  int pkgid;
  int num_cores;
  double energy;
  double start_energy;
  uint64_t start_time;
  cac_core_t cores[MAX_CORES];
} cac_pkg_t;
#define CAC_PKG(X) ((cac_pkg_t *)(X))

typedef struct {
  int num_pkgs;
  cac_pkg_t pkgs[MAX_PKGS];
} cac_node_t;
#define CAC_NODE(X) ((cac_node_t *)(X))

// If type = 1, package, else core.
typedef struct {
  int num;
  int type;
  void *obj; // This can either be a node or a core.
} cac_fd_t;
#define CAC_FD(X) ((cac_fd_t *)(X))

plugin_devops_t devops = {
    .open = cac_open,
    .close = cac_close,
    .read = cac_read,
    .write = cac_write,
};

plugin_dev_t dev = {
    .init = cac_init,
    .final = cac_final,
};

int _core_read(int cpu, const char *name, int64_t *val) {
  char path[256] = "";
  FILE *fd;
  snprintf(path, 255, "/sys/devices/system/cpu/cpu%i/%s", cpu, name);
  DBGP("Opening file: %s\n", path);
  fd = fopen(path, O_RDONLY);
  if (fd == NULL) {
    fprintf(stderr, "Error: unable to open CPU file at %s\n", path);
    return PWR_RET_FAILURE;
  }
  if (fscanf(fd, "%lld", val) != 1) {
    fprintf(stderr, "Error: unable to read value from file: %s\n", path);
    return PWR_RET_FAILURE;
  }
  return PWR_RET_SUCCESS;
}

plugin_devops_t *pwr_cpudev_init(const char *initstr) {
  DBGP("initstr='%s'\n", initstr);
  plugin_devops_t *dev = (plugin_devops_t *)malloc(sizeof(plugin_devops_t));
  *dev = devops;

  dev->private_data = malloc(sizeof(cac_node_t));
  bzero(dev->private_data, sizeof(cac_node_t));
  return dev;
}

int cac_final(plugin_devops_t *dev) {
  free(dev->private_data);
  free(dev);
  return PWR_RET_SUCCESS;
}

pwr_fd_t cac_open(plugin_devops_t *dev, const char *openstr) {
  pwr_fd_t *fd = (pwr_fd_t *)malloc(sizeof(cac_fd_t));
  bzero(fd, sizeof(cac_fd_t));
  if (strstr(openstr, "pkg") != NULL) {
    CAC_FD(fd)->obj = malloc(sizeof(cac_pkg_t));
    CAC_FD(fd)->type = 1;
    sscanf(openstr, "pkg%d", &(CAC_FD(fd))->num);
  }
  // This is a core.
  else if (strstr(openstr, "core") != NULL) {
    CAC_FD(fd)->obj = malloc(sizeof(cac_core_t));
    CAC_FD(fd)->type = 0;
    sscanf(openstr, "core%d", &(CAC_FD(fd))->num);
  } else {
    fprintf(stderr, "Invalid config string %s", openstr);
    return NULL;
  }
  return fd;
}

int cac_close(pwr_fd_t fd) {
  free(fd);
  return PWR_RET_SUCCESS;
}

int cac_read(pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len,
             PWR_Time *timestamp) {
  struct timeval tv;

  if (len != sizeof(int64_t)) {
    fprintf(stderr, "Error: value field size of %u incorrect, should be %ld\n",
            len, sizeof(unsigned long long));
    return PWR_RET_FAILURE;
  }

  // If this is a core reading.
  if (CAC_FD(fd)->type == 0) {
    switch (attr) {
    case PWR_ATTR_SSTATE:
      if (_core_read(CAC_FD(fd)->num, "online", (int64_t *)value) < 0) {
        fprintf(stderr, "Error: unable to read cpu %d sleep state\n",
                CAC_FD(fd)->num);
        return PWR_RET_FAILURE;
      }
      *((double *)value) = (*((double *)value) / 1000);
      break;
    case PWR_ATTR_FREQ:
      if (_core_read(CAC_FD(fd)->num, "cpufreq/cpuinfo_cur_freq",
                     (int64_t *)value) < 0) {
        fprintf(stderr, "Error: unable to read cpu %d current frequency\n",
                CAC_FD(fd)->num);
        return PWR_RET_FAILURE;
      }
      *((double *)value) = (*((double *)value) / 1000);
      break;
    case PWR_ATTR_FREQ_LIMIT_MIN:
      if (_core_read(CAC_FD(fd)->num, "cpufreq/scaling_min_freq",
                     (int64_t *)value) < 0) {
        fprintf(stderr,
                "Error: unable to read cpu %d minimum scaling frequency\n",
                CAC_FD(fd)->num);
        return PWR_RET_FAILURE;
      }
      *((double *)value) = (*((double *)value) / 1000);
      break;
    case PWR_ATTR_FREQ_LIMIT_MAX:
      if (_core_read(CAC_FD(fd)->num, "cpufreq/scaling_max_freq",
                     (int64_t *)value) < 0) {
        fprintf(stderr,
                "Error: unable to read cpu %d maximum scaling frequency\n",
                CAC_FD(fd)->num);
        return PWR_RET_FAILURE;
      }
      *((double *)value) = (*((double *)value) / 1000);
      break;
    case PWR_ATTR_THROTTLED_COUNT:
      if (_core_read(CAC_FD(fd)->num, "thermal_throttle/core_throttle_count",
                     (int64_t *)value) < 0) {
        fprintf(stderr,
                "Error: unable to read cpu %d maximum scaling frequency\n",
                CAC_FD(fd)->num);
        return PWR_RET_FAILURE;
      }
      break;
    default:
      fprintf(stderr, "Warning: unknown PWR reading attr (%u) requested\n",
              attr);
      return PWR_RET_FAILURE;
    }
    // Otherwise this is a package reading.
  } else {
    /*
     * Still have to add the package stuff to the XML file, and add the acquisition stuff here.
     *
     * This is where I found values that I haven't implemented yet (all in sys/devices or sys/class)
     * - pstate https://www.kernel.org/doc/html/v4.19/admin-guide/pm/intel_pstate.html
     * - powercap https://www.kernel.org/doc/Documentation/power/powercap/powercap.txt
     * - msr/dev (Didn't spend much time with it)
     * - hwmon https://www.kernel.org/doc/Documentation/hwmon/sysfs-interface
     * - thermal (might overlap with hwmon) https://www.kernel.org/doc/Documentation/thermal/sysfs-api.txt
     * - Already implemented but some info on cpufrq: https://www.pantz.org/software/cpufreq/usingcpufreqonlinux
     */
  }

  gettimeofday(&tv, NULL);
  *timestamp = tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;

  DBGP("Info: reading of type %u at time %llu with value %lf\n", attr,
       *(unsigned long long *)timestamp, *(double *)value);

  return 0;
}

int cac_write(pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len);

static int cpudev_read(int cpu, const char *name, double *val) {
  char path[256] = "", strval[20] = "";
  int offset = 0;
  int fd;

  snprintf(path, 255, "/sys/devices/system/cpu/cpu%i/%s", cpu, name);
  DBGP("%s\n", path);
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Error: unable to open CPU file at %s\n", path);
    return -1;
  }

  while (read(fd, strval + offset, 1) != EOF) {
    if (strval[offset] == ' ') {
      *val = atof(strval);
      return 0;
    }
    offset++;
  }

  fprintf(stderr, "Error: unable to parse PM counter value\n");
  return -1;
}

static int cpudev_write(int cpu, const char *name, double val) {
  char path[256] = "", strval[20] = "";
  int fd;

  snprintf(path, 255, "/sys/devices/system/cpu/cpu%i/%s", cpu, name);
  fd = open(path, O_WRONLY);
  if (fd < 0) {
    fprintf(stderr, "Error: unable to open CPU file at %s\n", path);
    return -1;
  }

  snprintf(strval, 19, "%lf", val);
  if (write(fd, strval, strlen(strval)) < 0) {
    fprintf(stderr, "Error: unable to write PM counter\n");
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}
