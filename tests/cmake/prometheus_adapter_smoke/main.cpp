#include "topoexec/adapters/prometheus.hpp"

#include <iostream>

int main() {
  topoexec::adapters::prometheus::TextExporterPreview exporter;
  topoexec::RuntimeMetricSample metric;
  metric.name = "runtime.component.execution_count";
  metric.value = 1.0;
  metric.component_id = "component";

  const auto status = exporter.on_metric(metric);
  if (!status.ok()) {
    std::cerr << status.message() << "\n";
    return 1;
  }

  const auto text = exporter.render_text();
  if (text.find("runtime_component_execution_count_total{component_id=\"component\"} 1") == std::string::npos) {
    std::cerr << "Prometheus preview text mapping failed\n";
    return 2;
  }
  if (topoexec::adapters::prometheus::kPrometheusPreviewContractVersion != "0") {
    std::cerr << "unexpected Prometheus preview contract version\n";
    return 3;
  }

  std::cout << "prometheus_adapter_smoke=runtime_component_execution_count_total\n";
  return 0;
}
