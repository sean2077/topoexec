#pragma once

// API stability: experimental. Metrics helpers are public but exporter/schema contracts are not stable yet.

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace topoexec {

using MetricTags = std::map<std::string, std::string>;

struct MetricSample {
  std::string name;
  double value{0.0};
  MetricTags tags;
};

class Counter {
public:
  void add(double delta = 1.0);
  double value() const;

private:
  mutable std::mutex mutex_;
  double value_{0.0};
};

class Gauge {
public:
  void set(double value);
  double value() const;

private:
  mutable std::mutex mutex_;
  double value_{0.0};
};

class Histogram {
public:
  void observe(double value);
  std::size_t count() const;
  double min() const;
  double max() const;
  double average() const;
  double percentile(double ratio) const;

private:
  mutable std::mutex mutex_;
  std::vector<double> values_;
};

class MetricRegistry {
public:
  Counter& counter(const std::string& name);
  Gauge& gauge(const std::string& name);
  Histogram& histogram(const std::string& name);
  std::vector<MetricSample> snapshot() const;

private:
  mutable std::mutex mutex_;
  std::map<std::string, Counter> counters_;
  std::map<std::string, Gauge> gauges_;
  std::map<std::string, Histogram> histograms_;
};

} // namespace topoexec
