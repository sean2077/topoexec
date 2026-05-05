#include "topoexec/common/metrics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace topoexec {

void Counter::add(double delta) {
  std::lock_guard lock(mutex_);
  value_ += delta;
}

double Counter::value() const {
  std::lock_guard lock(mutex_);
  return value_;
}

void Gauge::set(double value) {
  std::lock_guard lock(mutex_);
  value_ = value;
}

double Gauge::value() const {
  std::lock_guard lock(mutex_);
  return value_;
}

void Histogram::observe(double value) {
  std::lock_guard lock(mutex_);
  values_.push_back(value);
}

std::size_t Histogram::count() const {
  std::lock_guard lock(mutex_);
  return values_.size();
}

double Histogram::min() const {
  std::lock_guard lock(mutex_);
  if (values_.empty()) {
    return 0.0;
  }
  return *std::min_element(values_.begin(), values_.end());
}

double Histogram::max() const {
  std::lock_guard lock(mutex_);
  if (values_.empty()) {
    return 0.0;
  }
  return *std::max_element(values_.begin(), values_.end());
}

double Histogram::average() const {
  std::lock_guard lock(mutex_);
  if (values_.empty()) {
    return 0.0;
  }
  double total = 0.0;
  for (const auto value : values_) {
    total += value;
  }
  return total / static_cast<double>(values_.size());
}

double Histogram::percentile(double ratio) const {
  std::lock_guard lock(mutex_);
  if (values_.empty()) {
    return 0.0;
  }
  if (ratio < 0.0 || ratio > 1.0) {
    throw std::out_of_range("histogram percentile ratio must be between 0 and 1");
  }
  auto values = values_;
  std::sort(values.begin(), values.end());
  const auto position = ratio * static_cast<double>(values.size() - 1u);
  const auto lower = static_cast<std::size_t>(std::floor(position));
  const auto upper = static_cast<std::size_t>(std::ceil(position));
  if (lower == upper) {
    return values[lower];
  }
  const auto fraction = position - static_cast<double>(lower);
  return values[lower] + ((values[upper] - values[lower]) * fraction);
}

Counter& MetricRegistry::counter(const std::string& name) {
  std::lock_guard lock(mutex_);
  return counters_[name];
}

Gauge& MetricRegistry::gauge(const std::string& name) {
  std::lock_guard lock(mutex_);
  return gauges_[name];
}

Histogram& MetricRegistry::histogram(const std::string& name) {
  std::lock_guard lock(mutex_);
  return histograms_[name];
}

std::vector<MetricSample> MetricRegistry::snapshot() const {
  std::lock_guard lock(mutex_);
  std::vector<MetricSample> samples;
  samples.reserve(counters_.size() + gauges_.size() + histograms_.size() * 7u);
  for (const auto& [name, counter] : counters_) {
    samples.push_back({name, counter.value(), {}});
  }
  for (const auto& [name, gauge] : gauges_) {
    samples.push_back({name, gauge.value(), {}});
  }
  for (const auto& [name, histogram] : histograms_) {
    samples.push_back({name + ".count", static_cast<double>(histogram.count()), {}});
    samples.push_back({name + ".min", histogram.min(), {}});
    samples.push_back({name + ".max", histogram.max(), {}});
    samples.push_back({name + ".avg", histogram.average(), {}});
    samples.push_back({name + ".p50", histogram.percentile(0.50), {}});
    samples.push_back({name + ".p95", histogram.percentile(0.95), {}});
    samples.push_back({name + ".p99", histogram.percentile(0.99), {}});
  }
  return samples;
}

} // namespace topoexec
