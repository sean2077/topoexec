#include "topoexec/c_api/topoexec.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  if (TOPOEXEC_C_API_VERSION != 0) {
    fprintf(stderr, "unexpected C API version\n");
    return 1;
  }

  topoexec_runtime_t* runtime = topoexec_runtime_create();
  topoexec_graph_builder_t* builder = topoexec_graph_builder_create("c_api_smoke");
  if (runtime == NULL || builder == NULL) {
    fprintf(stderr, "failed to create C API handles\n");
    return 2;
  }
  if (topoexec_graph_builder_add_event_loop_lane(builder, "main") != TOPOEXEC_STATUS_OK) {
    fprintf(stderr, "lane error: %s\n", topoexec_graph_builder_last_error(builder));
    return 3;
  }
  if (topoexec_graph_builder_add_noop_component(builder, "noop", "main") != TOPOEXEC_STATUS_OK) {
    fprintf(stderr, "component error: %s\n", topoexec_graph_builder_last_error(builder));
    return 4;
  }

  topoexec_result_t* result = topoexec_runtime_run(runtime, builder, 1u);
  if (result == NULL || !topoexec_result_ok(result)) {
    fprintf(stderr, "run failed: %s\n", topoexec_runtime_last_error(runtime));
    return 5;
  }

  int saw_execution_count = 0;
  for (size_t index = 0; index < topoexec_result_metric_count(result); ++index) {
    topoexec_metric_sample_t metric;
    if (topoexec_result_metric_at(result, index, &metric) != TOPOEXEC_STATUS_OK) {
      fprintf(stderr, "metric iteration failed\n");
      return 6;
    }
    if (metric.name != NULL && strcmp(metric.name, "runtime.component.execution_count") == 0 &&
        metric.component_id != NULL && strcmp(metric.component_id, "noop") == 0 && metric.value >= 1.0) {
      saw_execution_count = 1;
    }
  }
  if (!saw_execution_count) {
    fprintf(stderr, "missing execution_count metric\n");
    return 7;
  }

  topoexec_result_destroy(result);
  topoexec_graph_builder_destroy(builder);
  topoexec_runtime_destroy(runtime);
  printf("c_api_smoke=runtime.component.execution_count\n");
  return 0;
}
