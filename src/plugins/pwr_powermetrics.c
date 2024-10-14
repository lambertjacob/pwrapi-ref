/*
 * Copyright 2024 Ethan Silver, Jacob Lambert, Severn Lortie, Sam
 * Emard-Thibault.
 *
 * Ok so here's what I've got right now:
 * - init() is called during the <Plugin> section of the XML file.
 * - open() is called during the <device> section(s) of the XML file.
 * - read() is called when some PowerAPI read function is called.
 * - write() is called when some PowerAPI write function is called.
 * - close() is called when the device is done.
 * - final() is called when everything is done.
 *
 * There are other functions, but they're either not important or I've put
 * comments explaining them.
 */
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include "pwr_dev.h"
#include "pwr_powermetrics.h"

#define PWRMETRICS_CORE_MODE 0
#define PWRMETRICS_NODE_MODE 1
#define MAX_CORES 12
// This value must be adjusted based on powermetrics output.
#define MAX_C_STATES 4

typedef struct {
  double freq;                   // Total package power.
  double c_states[MAX_C_STATES]; // System average frequency.
} pwr_metrics_core_t;
#define PWR_METCORE(X) ((pwr_metrics_core_t *)(X))

typedef struct {
  int num_cpus;
  double pkg_pwr;
  double avg_freq;
  pwr_metrics_core_t cores[MAX_CORES];
} pwr_metrics_node_t;
#define PWR_METNODE(X) ((pwr_metrics_node_t *)(X))

typedef struct {
  int link;
  pid_t metrics_pid;
} pwr_metrics_data_t;
#define PWR_METDATA(X) ((pwr_metrics_data_t *)(X))

typedef struct {
  int num;
  int type;
  void *obj; // This can either be a node or a core.
  pwr_metrics_data_t *data;
} pwr_metrics_fd_t;
#define PWR_METFD(X) ((pwr_metrics_fd_t *)(X))
static double getTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000000000) + (tv.tv_usec * 1000);
}

static double getTimeSec() { return getTime() / 1000000000.0; }

static plugin_devops_t devOps = {.open = powermetrics_open,
                                 .close = powermetrics_close,
                                 .read = powermetrics_read,
                                 .write = powermetrics_write,
                                 .readv = powermetrics_readv,
                                 .writev = powermetrics_writev,
                                 .time = powermetrics_time,
                                 .clear = powermetrics_clear};

static plugin_dev_t dev = {
    .init = powermetrics_init,
    .final = powermetrics_final,
};

/**
 * Initialize the PowerMetrics plugin. This is where the open code should go,
 * creating the process.
 */
plugin_devops_t *powermetrics_init(const char *initstr) {
  DBGP("initstr='%s'\n", initstr);
  plugin_devops_t *ops = (plugin_devops_t *)malloc(sizeof(*ops));
  *ops = devOps;
  ops->private_data = malloc(sizeof(pwr_metrics_data_t));
  bzero(ops->private_data, sizeof(pwr_metrics_data_t));

  int link[2];
  pid_t fork_pid;

  assert(pipe(link) != -1);
  assert((fork_pid = fork()) != -1);

  // Child process.
  if (fork_pid == 0) {
    dup2(link[1], STDOUT_FILENO); // Link STDOUT to the link.
    // Close both ends of the pipe, since we don't need them.
    close(link[0]);
    close(link[1]);
    int succ = execlp("/usr/bin/powermetrics", "powermetrics", "--samplers",
                      "cpu_power", "thermal", "--hide-cpu-duty-cycle", "-i1",
                      (char *)NULL);
    assert(succ != -1);
  } else {
    // Close the input end of the pipe.
    close(link[1]);
    PWR_METDATA(ops->private_data)->link =
        link[0]; // Save the pipe link so the process can be accessed.
    PWR_METDATA(ops->private_data)->metrics_pid = fork_pid;
    printf("child PID: %d\n", fork_pid);
    return ops;
  }
}

int powermetrics_final(plugin_devops_t *ops) {
  pwr_metrics_data_t *info = PWR_METDATA(ops->private_data);
  close(info->link);
  printf("child PID killing: %d\n", info->metrics_pid);
  kill(info->metrics_pid, SIGTERM);
  DBGP("\n");
  free(info);
  free(ops);
  return 0;
}

pwr_fd_t powermetrics_open(plugin_devops_t *ops, const char *openstr) {
  pwr_metrics_fd_t *tmp = PWR_METFD(malloc(sizeof(pwr_metrics_fd_t)));
  // This is a node.
  if (strstr(openstr, "node") != NULL) {
    tmp->obj = malloc(sizeof(pwr_metrics_node_t));
    tmp->data = PWR_METDATA(ops->private_data);
    tmp->type = PWRMETRICS_NODE_MODE;
    sscanf(openstr, "node%d", &tmp->num);
    return tmp;
  }
  // This is a core.
  else if (strstr(openstr, "core") != NULL) {
    tmp->obj = malloc(sizeof(pwr_metrics_core_t));
    tmp->data = PWR_METDATA(ops->private_data);
    tmp->type = PWRMETRICS_CORE_MODE;
    sscanf(openstr, "core%d", &tmp->num);
    return tmp;
  } else {
    fprintf(stderr, "Invalid config string %s", openstr);
    return NULL;
  }
}

int powermetrics_close(pwr_fd_t fd) {
  DBGP("\n");
  free(fd);
  return 0;
}

int _read_core_data(int link, pwr_metrics_core_t *tmp, int num) {
  char output[4096], *substr_ind;
  double frac_nom_freq;
  struct timeval ts, ts2;
  char template_str[1024];

  snprintf(template_str, 1024, "Core %d C-state residency: ", num);

  read(link, output, sizeof(output));
  substr_ind = strstr(output, template_str);
  gettimeofday(&ts, NULL);
  gettimeofday(&ts2, NULL);

  printf("CORE HERE\n");

  int i = 0;
  while ((ts.tv_usec + 2000) > ts2.tv_usec && substr_ind == NULL) {
    printf("CORE HERE %d\n", i++);
    read(link, output, sizeof(output));
    substr_ind = strstr(output, template_str);
    gettimeofday(&ts2, NULL);
    usleep(100);
  }

  if (substr_ind == NULL) {
    fprintf(stderr, "Could not read from PowerMetrics after 2ms.\n");
    return PWR_RET_FAILURE;
  }

  sscanf(substr_ind + 7,
         "C-state residency: %lf%% (C3: %lf%% C6: %lf%% C7: %lf%% )",
         &tmp->c_states[0], &tmp->c_states[1], &tmp->c_states[2],
         &tmp->c_states[3]);
  substr_ind =
      strstr(substr_ind, "CPU Average frequency as fraction of nominal");
  assert(substr_ind != NULL);
  sscanf(substr_ind,
         "CPU Average frequency as fraction of nominal: %lf%% (%lf Mhz)",
         &frac_nom_freq, &tmp->freq);
  return PWR_RET_SUCCESS;
}

int _wait_and_read(int link, char *buf, int len, const char *const str) {
  char *substr_ind;
  struct timeval ts, ts2;
  read(link, buf, len);
  substr_ind = strstr(buf, str);
  gettimeofday(&ts, NULL);
  gettimeofday(&ts2, NULL);
  while (ts.tv_usec + 2000 > ts2.tv_usec && substr_ind == NULL) {
    read(link, buf, len);
    substr_ind = strstr(buf, "Intel energy model derived package power");
    gettimeofday(&ts2, NULL);
    usleep(100);
  }
  if (substr_ind == NULL) {
    fprintf(stderr, "Could not read from PowerMetrics after 2ms.\n");
    return PWR_RET_FAILURE;
  } else {
    return PWR_RET_SUCCESS;
  }
}

int _read_node_data(int link, pwr_metrics_node_t *tmp) {
  char output[4096], *substr_ind;
  double frac_nom_freq;
  char template_str[1024];
  int n_cores = 0;
  read(link, output, sizeof(output));
  substr_ind = strstr(output, template_str);

  int res = _wait_and_read(link, output, sizeof(output), template_str);

  if (res == PWR_RET_FAILURE) {
    return PWR_RET_FAILURE;
  }

  substr_ind = strstr(output, "Intel energy model derived package power");
  sscanf(substr_ind,
         "Intel energy model derived package power (CPUs+GT+SA): %lf",
         &tmp->pkg_pwr);
  substr_ind =
      strstr(output, "System Average frequency as fraction of nominal");
  assert(substr_ind != NULL);
  sscanf(substr_ind,
         "System Average frequency as fraction of nominal: %lf%% (%lf Mhz)",
         &frac_nom_freq, &tmp->avg_freq);
  snprintf(template_str, 1024, "Core %d C-state residency: ", n_cores);
  while ((substr_ind = strstr(substr_ind, template_str)) != NULL) {
    snprintf(template_str, 1024, "Core %d C-state residency", n_cores);
    sscanf(substr_ind,
           "C-state residency: %lf%% (C3: %lf%% C6: %lf%% C7: %lf%% )",
           &tmp->cores[n_cores].c_states[0], &tmp->cores[n_cores].c_states[1],
           &tmp->cores[n_cores].c_states[2], &tmp->cores[n_cores].c_states[3]);
    substr_ind =
        strstr(substr_ind, "CPU Average frequency as fraction of nominal");
    assert(substr_ind != NULL);
    sscanf(substr_ind,
           "CPU Average frequency as fraction of nominal: %lf%% (%lf Mhz)",
           &frac_nom_freq, &tmp->cores[n_cores].freq);
    n_cores++;
  }
  tmp->num_cpus = n_cores + 1;
  return PWR_RET_SUCCESS;
}

/**
 * Read a Power attribute value.
 */
int powermetrics_read(pwr_fd_t fd, PWR_AttrName type, void *val,
                      unsigned int len, PWR_Time *ts) {
  pwr_metrics_fd_t *info = PWR_METFD(fd);
  struct timeval tv;
  double cstates = 0;
  if (info->type == PWRMETRICS_NODE_MODE) {
    pwr_metrics_node_t *node = PWR_METNODE(info->obj);
    assert(_read_node_data(info->data->link, node) == PWR_RET_SUCCESS);

    switch (type) {
    case PWR_ATTR_FREQ:
      *((double *)val) = node->avg_freq;
      break;
    case PWR_ATTR_POWER:
      *((double *)val) = node->pkg_pwr;
      break;
    case PWR_ATTR_CSTATE:
      // This is a bad metric but it's better than failing!
      for (int i = 0; i < node->num_cpus; i++) {
        cstates += node->cores[i].c_states[0];
      }
      *((double *)val) = cstates;
      break;
    default:
      fprintf(stderr, "Warning: unknown attr (%u) requested\n", type);
      break;
    }
  } else if (info->type == PWRMETRICS_CORE_MODE) {
    pwr_metrics_core_t *core = PWR_METCORE(info->obj);
    assert(_read_core_data(info->data->link, core, info->num) ==
           PWR_RET_SUCCESS);

    switch (type) {
    case PWR_ATTR_FREQ:
      *((double *)val) = core->freq;
      break;
    case PWR_ATTR_CSTATE:
      *((double *)val) = core->c_states[0];
      break;
    default:
      fprintf(stderr, "Warning: unknown attr (%u) requested\n", type);
      break;
    }
  }
  return PWR_RET_SUCCESS;
}

int powermetrics_write(pwr_fd_t fd, PWR_AttrName type, void *ptr,
                       unsigned int len) {
  return PWR_RET_FAILURE;
}

int powermetrics_readv(pwr_fd_t fd, unsigned int arraysize,
                       const PWR_AttrName attrs[], void *buf, PWR_Time ts[],
                       int status[]) {
  int i;
  for (i = 0; i < arraysize; i++) {
    status[i] =
        powermetrics_read(fd, attrs[i], ((double *)buf + i), arraysize, &ts[i]);
  }
  return PWR_RET_SUCCESS;
}

int powermetrics_writev(pwr_fd_t fd, unsigned int arraysize,
                        const PWR_AttrName attrs[], void *buf, int status[]) {
  int i;
  DBGP("num attributes %d\n", arraysize);
  for (i = 0; i < arraysize; i++) {
    status[i] = PWR_RET_FAILURE;
  }
  return PWR_RET_FAILURE;
}

int powermetrics_time(pwr_fd_t fd, PWR_Time *timestamp) {
  DBGP("\n");

  // Get time.
  time((time_t *)timestamp);
  return 0;
}

int powermetrics_clear(pwr_fd_t fd) {
  DBGP("\n");
  return 0;
}

plugin_dev_t *getDev() { return &dev; }

/**
 * The plugin name.
 */
static int powermetrics_getPluginName(size_t len, char *buf) {
  strncpy(buf, "PowerMetrics", len);
  return 0;
}

/**
 * The supported objects.
 */
static int powermetrics_readObjs(int i, PWR_ObjType *ptr) {
  DBGP("\n");
  ptr[0] = PWR_OBJ_NODE;
  ptr[1] = PWR_OBJ_CORE;
  return 0;
}

/**
 * The number of supported attributes.
 *
 * This is the length of the array passed to powermetrics_readObjs.
 */
static int powermetrics_numObjs() {
  DBGP("\n");
  return 2;
}

/**
 * The supported attributes.
 */
static int powermetrics_readAttrs(PWR_ObjType type, int i, PWR_AttrName *ptr) {
  DBGP("\n");
  ptr[0] = PWR_ATTR_CSTATE;
  ptr[1] = PWR_ATTR_POWER;
  ptr[2] = PWR_ATTR_FREQ;
  return 0;
}

/**
 * The number of supported attributes.
 *
 * This is the length of the array passed to powermetrics_readAttrs.
 */
static int powermetrics_numAttrs(PWR_ObjType type) {
  DBGP("\n");
  return 3;
}

// This is where I stop knowing what they're talking about. -Ethan

/**
 * This is left as the default since we don't have multiple instances.
 */
static int powermetrics_getDevName(PWR_ObjType type, size_t len, char *buf) {
  strncpy(buf, "powermetrics", len);
  DBGP("type=%d name=`%s`\n", type, buf);
  return 0;
}

// Create the device initialized string for the specified dev. The name
// was returned the the framework by powermetrics_getDevName()
static int powermetrics_getDevInitStr(const char *devName, size_t len,
                                      char *buf) {
  strncpy(buf, "powermetrics", len);
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
    .readObjs = powermetrics_readObjs,
    .numAttrs = powermetrics_numAttrs,
    .readAttrs = powermetrics_readAttrs,
    .getDevName = powermetrics_getDevName,
    .getDevOpenStr = powermetrics_getDevOpenStr,
    .getDevInitStr = powermetrics_getDevInitStr,
    .getPluginName = powermetrics_getPluginName,
};

plugin_meta_t *getMeta() { return &meta; }
