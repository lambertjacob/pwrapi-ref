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
#include <string.h>
#include <errno.h>

#include "pwr_dev.h"
#include "pwr_toiletdev.h"

#define MAX_C_STATES 5
#define MAX_CORES 128
#define MAX_PKGS 4

typedef struct
{
  int coreid;
  double cur_freq;
  double max_freq;
  double min_freq;
  double cpuidle[MAX_C_STATES]; // CPU idle states.
} toilet_core_t;
#define toilet_CORE(X) ((toilet_core_t *)(X))

typedef struct
{
  int pkgid;
  int num_cores;
  double energy;
  double start_energy;
  uint64_t start_time;
  toilet_core_t cores[MAX_CORES];
} toilet_pkg_t;
#define toilet_PKG(X) ((toilet_pkg_t *)(X))

typedef struct
{
  int num_pkgs;
  toilet_pkg_t pkgs[MAX_PKGS];
} toilet_node_t;
#define toilet_NODE(X) ((toilet_node_t *)(X))

// If type = 1, package, else core.
typedef struct
{
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

plugin_devops_t *toilet_init(const char *initstr)
{
  DBGP("initstr='%s'\n", initstr);
  plugin_devops_t *dev = (plugin_devops_t *)malloc(sizeof(plugin_devops_t));
  *dev = devops;

  dev->private_data = malloc(sizeof(toilet_node_t));
  bzero(dev->private_data, sizeof(toilet_node_t));
  return dev;
}

int toilet_final(plugin_devops_t *dev)
{
  free(dev->private_data);
  free(dev);
  return PWR_RET_SUCCESS;
}

pwr_fd_t toilet_open(plugin_devops_t *dev, const char *openstr)
{
  pwr_fd_t *fd = (pwr_fd_t *)malloc(sizeof(toilet_fd_t));
  bzero(fd, sizeof(toilet_fd_t));
  if (strstr(openstr, "pkg") != NULL)
  {
    toilet_FD(fd)->obj = malloc(sizeof(toilet_pkg_t));
    toilet_FD(fd)->type = 1;
    sscanf(openstr, "pkg%d", &(toilet_FD(fd))->num);
  }
  else if (strstr(openstr, "core") != NULL)
  {
    toilet_FD(fd)->obj = malloc(sizeof(toilet_core_t));
    toilet_FD(fd)->type = 0;
    sscanf(openstr, "core%d", &(toilet_FD(fd))->num);
  }
  return fd;
}

int toilet_close(pwr_fd_t fd)
{
  free(fd);
  return PWR_RET_SUCCESS;
}

int toilet_read(pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len,
                PWR_Time *timestamp)
{
  struct timeval tv;
  char path[256] = "", strval[101] = "";
  int file;
  if (len != sizeof(int64_t))
  {
    DBGP("Error: value field size of %u incorrect, should be %ld\n", len,
         sizeof(unsigned long long));
    return PWR_RET_FAILURE;
  }
  if (toilet_FD(fd)->type == 1)
  {
    switch (attr)
    {
    case PWR_ATTR_ENERGY:
    {
      snprintf(
          path, 255,
          "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:%d/energy_uj",
          toilet_FD(fd)->num);
    }
    break;
    default:
    {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  }
  else
  {
    switch (attr)
    {
    case PWR_ATTR_FREQ:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
               toilet_FD(fd)->num);
    }
    break;
    case PWR_ATTR_FREQ_LIMIT_MAX:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",
               toilet_FD(fd)->num);
    }
    break;
    case PWR_ATTR_FREQ_LIMIT_MIN:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",
               toilet_FD(fd)->num);
    }
    break;
    case PWR_ATTR_GOV:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",
               toilet_FD(fd)->num);
    }
    break;
    default:
    {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  }
  file = open(path, O_RDONLY);
  if (file < 0)
  {
    DBGP("Error: unable to open CPU file at %s\n", path);
    return PWR_RET_FAILURE;
  }
  DBGP("Reading attribute from file %s\n", path);
  if (read(file, strval, 100) < 0)
  {
    DBGP("Error: unable to read PM counter.\n");
    close(file);
    return PWR_RET_FAILURE;
  }
  close(file);

  if (attr == PWR_ATTR_GOV)
  {
    if (strstr(strval, "ondemand") != NULL)
    {
      *(int *)value = PWR_GOV_LINUX_ONDEMAND;
    }
    else if (strstr(strval, "performance") != NULL)
    {
      *(int *)value = PWR_GOV_LINUX_PERFORMANCE;
    }
    else if (strstr(strval, "conservative") != NULL)
    {
      *(int *)value = PWR_GOV_LINUX_CONSERVATIVE;
    }
    else if (strstr(strval, "powersave") != NULL)
    {
      *(int *)value = PWR_GOV_LINUX_POWERSAVE;
    }
    else if (strstr(strval, "userspace") != NULL)
    {
      *(int *)value = PWR_GOV_LINUX_USERSPACE;
    }
    else if (strstr(strval, "schedutil") != NULL)
    {
      *(int *)value = PWR_GOV_LINUX_SCHEDUTIL;
    }
  }
  else
  {
    sscanf(strval, "%lu", (uint64_t *)value);
  }
  gettimeofday(&tv, NULL);
  *timestamp = tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
  DBGP("Info: reading of type %u at time %llu with value %s\n", attr,
       *(unsigned long long *)timestamp, strval);
  return PWR_RET_SUCCESS;
}

int toilet_write(pwr_fd_t fd, PWR_AttrName attr, void *value,
                 unsigned int len)
{
  char path[256] = "", strval[20] = "";
  int file;
  if (len != sizeof(int64_t))
  {
    DBGP("Error: value field size of %u incorrect, should be %ld\n", len,
         sizeof(unsigned long long));
    return PWR_RET_FAILURE;
  }
  if (toilet_FD(fd)->type == 1)
  {
    switch (attr)
    {
    case PWR_ATTR_ENERGY:
    {
      snprintf(
          path, 255,
          "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:%d/energy_uj",
          toilet_FD(fd)->num);
    }
    break;
    default:
    {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  }
  else
  {
    switch (attr)
    {
    case PWR_ATTR_FREQ:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed",
               toilet_FD(fd)->num);
    }
    break;
    case PWR_ATTR_FREQ_LIMIT_MAX:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",
               toilet_FD(fd)->num);
    }
    break;
    case PWR_ATTR_FREQ_LIMIT_MIN:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",
               toilet_FD(fd)->num);
    }
    break;
    case PWR_ATTR_GOV:
    {
      snprintf(path, 255,
               "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",
               toilet_FD(fd)->num);
    }
    break;
    default:
    {
      DBGP("Error: Unrecognized power attribute.\n");
      return PWR_RET_FAILURE;
    }
    }
  }

  // used to determine whether or not we're setting the governor to a default mode
  // basically, anything but userspace
  int set_default = 0;

  if (attr == PWR_ATTR_GOV)
  {
    if (*(int *)value == PWR_GOV_LINUX_ONDEMAND)
    {
      set_default = 1;
      sprintf(strval, "ondemand");
    }
    else if (*(int *)value == PWR_GOV_LINUX_PERFORMANCE)
    {
      set_default = 1;
      sprintf(strval, "performance");
    }
    else if (*(int *)value == PWR_GOV_LINUX_CONSERVATIVE)
    {
      set_default = 1;
      sprintf(strval, "conservative");
    }
    else if (*(int *)value == PWR_GOV_LINUX_POWERSAVE)
    {
      set_default = 1;
      sprintf(strval, "powersave");
    }
    else if (*(int *)value == PWR_GOV_LINUX_USERSPACE)
    {
      sprintf(strval, "userspace");
    }
    else
    {
      set_default = 1;
      sprintf(strval, "schedutil");
    }

    if (set_default)
    {
      char path2[256] = "";

      // reset the frequency limits to default
      char *files[2] = {"scaling_max_freq", "scaling_min_freq"};
      char *defaults[2] = {"2000000", "1200000"};

      // Reset min and max frequency files to default values
      for (int i = 0; i < 2; i++)
      {
        snprintf(path2, 255, "/sys/devices/system/cpu/cpu%d/cpufreq/%s",
                 toilet_FD(fd)->num, files[i]);
        file = open(path2, O_WRONLY);
        if (file < 0)
        {
          DBGP("Error: unable to open CPU file at %s\n", path2);
          return PWR_RET_FAILURE;
        }
        DBGP("Writing attribute to file %s\n", path2);
        if (write(file, defaults[i], 100) < 0)
        {
          DBGP("Error: unable to write PM counter.\n");
          DBGP("toilet_write(): Failed to write %s, errno: %d (%s)\n", path, errno, strerror(errno));
          close(file);
          return PWR_RET_FAILURE;
        }
        close(file);
      }
    }
  }
  else
  {
    sprintf(strval, "%lu", *(uint64_t *)value);
  }

  // We need to set stuff to multiple files if we're setting frequency.
  if (attr == PWR_ATTR_FREQ)
  {
    char *files[3] = {"scaling_setspeed", "scaling_max_freq", "scaling_min_freq"};

    // For each file, write the the frequency value we want
    for (int i = 0; i < 3; i++)
    {
      snprintf(path, 255, "/sys/devices/system/cpu/cpu%d/cpufreq/%s",
               toilet_FD(fd)->num, files[i]);
      file = open(path, O_WRONLY);
      if (file < 0)
      {
        DBGP("Error: unable to open CPU file at %s\n", path);
        return PWR_RET_FAILURE;
      }
      DBGP("Writing %s to file %s\n", strval, path);
      if (write(file, strval, 20) < 0)
      {
        DBGP("Error: unable to write PM counter.\n");
        DBGP("toilet_write(): Failed to write %s, errno: %d (%s)\n", path, errno, strerror(errno));
        close(file);
        return PWR_RET_FAILURE;
      }
      close(file);
    }
  }
  else
  {
    file = open(path, O_WRONLY);
    if (file < 0)
    {
      DBGP("Error: unable to open CPU file at %s\n", path);
      return PWR_RET_FAILURE;
    }
    DBGP("Writing attribute to file %s\n", path);

    if (write(file, strval, 100) < 0)
    {
      DBGP("Error: unable to write PM counter.\n");
      close(file);
      return PWR_RET_FAILURE;
    }
    close(file);
    DBGP("Info: Writing type %u with value %s\n", attr, strval);
    return PWR_RET_SUCCESS;
  }
}

static int pwr_toiletdev_numObjs()
{
  DBGP("\n");
  return 2;
}

static int pwr_toiletdev_readObjs(int i, PWR_ObjType *ptr)
{
  DBGP("\n");
  ptr[0] = PWR_OBJ_SOCKET;
  ptr[1] = PWR_OBJ_CORE;
  return 0;
}

static int pwr_toiletdev_numAttrs(PWR_ObjType type)
{
  DBGP("\n");
  return 5;
}

static int pwr_toiletdev_readAttrs(PWR_ObjType type, int i, PWR_AttrName *ptr)
{
  DBGP("\n");
  ptr[0] = PWR_ATTR_POWER;
  ptr[1] = PWR_ATTR_FREQ;
  ptr[2] = PWR_ATTR_FREQ_LIMIT_MIN;
  ptr[3] = PWR_ATTR_FREQ_LIMIT_MAX;
  ptr[4] = PWR_ATTR_GOV;
  return 0;
}

static int pwr_toiletdev_getDevName(PWR_ObjType type, size_t len, char *buf)
{
  strncpy(buf, "toilet_dev0", len);
  DBGP("type=%d name=`%s`\n", type, buf);
  return 0;
}

static int pwr_toiletdev_getDevOpenStr(PWR_ObjType type, int global_index,
                                       size_t len, char *buf)
{
  snprintf(buf, len, "%d", global_index);
  DBGP("type=%d global_index=%d str=`%s`\n", type, global_index, buf);
  return 0;
}

static int pwr_toiletdev_getDevInitStr(const char *name, size_t len,
                                       char *buf)
{
  strncpy(buf, "", len);
  DBGP("dev=`%s` str=`%s`\n", name, buf);
  return 0;
}

static int pwr_toiletdev_getPluginName(size_t len, char *buf)
{
  strncpy(buf, "toilet", len);
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
