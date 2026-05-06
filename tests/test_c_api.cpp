#include "topoexec/c_api/topoexec.h"

#include <gtest/gtest.h>

#include <cstring>

TEST(CApiPreview, RunsNoopGraphAndIteratesMetrics) {
  auto* runtime = topoexec_runtime_create();
  auto* builder = topoexec_graph_builder_create("c_api_unit");
  ASSERT_NE(runtime, nullptr);
  ASSERT_NE(builder, nullptr);

  EXPECT_EQ(topoexec_graph_builder_add_event_loop_lane(builder, "main"), TOPOEXEC_STATUS_OK);
  EXPECT_EQ(topoexec_graph_builder_add_noop_component(builder, "noop", "main"), TOPOEXEC_STATUS_OK);

  auto* result = topoexec_runtime_run(runtime, builder, 1u);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(topoexec_result_ok(result), 1);
  EXPECT_EQ(topoexec_result_error_count(result), 0u);

  bool saw_execution_count = false;
  for (std::size_t index = 0; index < topoexec_result_metric_count(result); ++index) {
    topoexec_metric_sample_t metric{};
    ASSERT_EQ(topoexec_result_metric_at(result, index, &metric), TOPOEXEC_STATUS_OK);
    if (std::strcmp(metric.name, "runtime.component.execution_count") == 0 &&
        std::strcmp(metric.component_id, "noop") == 0) {
      saw_execution_count = metric.value >= 1.0;
    }
  }
  EXPECT_TRUE(saw_execution_count);

  topoexec_result_destroy(result);
  topoexec_graph_builder_destroy(builder);
  topoexec_runtime_destroy(runtime);
}

TEST(CApiPreview, ReportsBuilderErrorsWithoutExceptions) {
  auto* builder = topoexec_graph_builder_create("c_api_error");
  ASSERT_NE(builder, nullptr);

  EXPECT_EQ(topoexec_graph_builder_add_noop_component(builder, "noop", nullptr), TOPOEXEC_STATUS_ERROR);
  EXPECT_STREQ(topoexec_graph_builder_last_error(builder), "lane_id is required");

  topoexec_graph_builder_destroy(builder);
}
