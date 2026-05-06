#pragma once

/* API stability: experimental. C API preview v0 is an unstable FFI design surface. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOPOEXEC_C_API_VERSION 0

typedef struct topoexec_runtime topoexec_runtime_t;
typedef struct topoexec_graph_builder topoexec_graph_builder_t;
typedef struct topoexec_result topoexec_result_t;

typedef enum topoexec_status_code {
  TOPOEXEC_STATUS_OK = 0,
  TOPOEXEC_STATUS_ERROR = 1,
} topoexec_status_code_t;

typedef struct topoexec_metric_sample {
  const char* name;
  double value;
  const char* component_id;
  const char* lane;
  const char* channel_id;
} topoexec_metric_sample_t;

topoexec_runtime_t* topoexec_runtime_create(void);
void topoexec_runtime_destroy(topoexec_runtime_t* runtime);
const char* topoexec_runtime_last_error(const topoexec_runtime_t* runtime);

topoexec_graph_builder_t* topoexec_graph_builder_create(const char* graph_name);
void topoexec_graph_builder_destroy(topoexec_graph_builder_t* builder);
const char* topoexec_graph_builder_last_error(const topoexec_graph_builder_t* builder);
topoexec_status_code_t topoexec_graph_builder_add_event_loop_lane(topoexec_graph_builder_t* builder,
                                                                  const char* lane_id);
topoexec_status_code_t topoexec_graph_builder_add_noop_component(topoexec_graph_builder_t* builder,
                                                                 const char* component_id, const char* lane_id);

topoexec_result_t* topoexec_runtime_run(topoexec_runtime_t* runtime, const topoexec_graph_builder_t* builder,
                                        size_t tick_iterations);
void topoexec_result_destroy(topoexec_result_t* result);
int topoexec_result_ok(const topoexec_result_t* result);
const char* topoexec_result_error_at(const topoexec_result_t* result, size_t index);
size_t topoexec_result_error_count(const topoexec_result_t* result);
size_t topoexec_result_metric_count(const topoexec_result_t* result);
topoexec_status_code_t topoexec_result_metric_at(const topoexec_result_t* result, size_t index,
                                                 topoexec_metric_sample_t* out_metric);

#ifdef __cplusplus
}
#endif
