#include "topoexec/common/logging.hpp"
#include "topoexec/common/metrics.hpp"
#include "topoexec/common/trace.hpp"

#include <gtest/gtest.h>
#include <thread>

TEST(Common, MetricsSnapshotIncludesCountersGaugesAndHistograms) {
  topoexec::MetricRegistry registry;
  registry.counter("requests").add(2.0);
  registry.gauge("queue_depth").set(3.0);
  registry.histogram("latency_ms").observe(10.0);
  registry.histogram("latency_ms").observe(30.0);

  const auto samples = registry.snapshot();
  EXPECT_GE(samples.size(), 6u);
}

TEST(Common, StructuredLoggerStoresJsonSerializableRecords) {
  topoexec::MemoryLogSink sink;
  topoexec::StructuredLogger logger("test");
  logger.attach_sink(&sink);

  EXPECT_TRUE(logger.log_once(topoexec::LogLevel::kInfo, "boot", "started", "runtime"));
  EXPECT_FALSE(logger.log_once(topoexec::LogLevel::kInfo, "boot", "started", "runtime"));
  {
    topoexec::ScopeTimer timer(logger, "scope");
  }

  const auto records = sink.records();
  ASSERT_GE(records.size(), 2u);
  EXPECT_NE(topoexec::to_json_line(records.front()).find("\"component\":\"test\""), std::string::npos);
}

TEST(Common, TraceCollectorRecordsScopedSpan) {
  topoexec::TraceCollector collector;
  {
    topoexec::ScopedSpan span(collector, topoexec::TraceId::generate(), "work");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto spans = collector.spans();
  ASSERT_EQ(spans.size(), 1u);
  EXPECT_EQ(spans.front().name, "work");
  EXPECT_GT(spans.front().duration().count(), 0);
}
