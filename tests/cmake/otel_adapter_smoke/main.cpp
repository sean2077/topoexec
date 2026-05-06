#include "topoexec/adapters/otel.hpp"

#include <iostream>

int main() {
  topoexec::adapters::otel::ExporterPreview exporter;
  topoexec::RuntimeMetricSample metric;
  metric.name = "runtime.component.execution_count";
  metric.value = 1.0;
  metric.component_id = "component";

  const auto status = exporter.on_metric(metric);
  if (!status.ok()) {
    std::cerr << status.message() << "\n";
    return 1;
  }

  const auto records = exporter.metric_records();
  if (records.size() != 1u || records.front().instrument_kind != "counter" || records.front().unit != "count") {
    std::cerr << "OTel preview metric mapping failed\n";
    return 2;
  }
  if (topoexec::adapters::otel::kOtelPreviewContractVersion != "0") {
    std::cerr << "unexpected OTel preview contract version\n";
    return 3;
  }

  std::cout << "otel_adapter_smoke=" << records.front().name << "\n";
  return 0;
}
