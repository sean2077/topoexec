#include "topoexec/runtime/task_executor.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct RunSample {
  double deterministic_elapsed_ms{0.0};
  double threaded_elapsed_ms{0.0};
};

struct Totals {
  std::size_t submitted{0};
  std::size_t completed{0};
  std::size_t rejected{0};
  std::size_t failed{0};
  std::size_t max_inflight{0};
};

struct Args {
  std::size_t tasks{32};
  std::size_t runs{5};
  std::string format{"text"};
};

double elapsed_ms_since(std::chrono::steady_clock::time_point started) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

double percentile(std::vector<double> values, double ratio) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(ratio * static_cast<double>(values.size() - 1u));
  return values[index];
}

std::string require_value(int argc, char** argv, int& index) {
  if (index + 1 >= argc) {
    throw std::runtime_error(std::string("missing value for ") + argv[index]);
  }
  ++index;
  return argv[index];
}

std::size_t parse_size(std::string value, const std::string& option) {
  const auto parsed = std::stoull(value);
  if (parsed == 0u) {
    throw std::runtime_error(option + " must be positive");
  }
  return static_cast<std::size_t>(parsed);
}

Args parse_args(int argc, char** argv) {
  Args args;
  for (int index = 1; index < argc; ++index) {
    const std::string option = argv[index];
    if (option == "--tasks") {
      args.tasks = parse_size(require_value(argc, argv, index), option);
    } else if (option == "--runs") {
      args.runs = parse_size(require_value(argc, argv, index), option);
    } else if (option == "--format") {
      args.format = require_value(argc, argv, index);
      if (args.format != "text" && args.format != "json") {
        throw std::runtime_error("--format must be text or json");
      }
    } else if (option == "-h" || option == "--help") {
      std::cout << "Usage: topoexec_bench_task_executor [--tasks N] [--runs N] [--format text|json]\n";
      std::exit(0);
    } else {
      throw std::runtime_error("unknown option: " + option);
    }
  }
  return args;
}

void accumulate(Totals& totals, const topoexec::TaskExecutorMetrics& metrics) {
  totals.submitted += metrics.submitted_count;
  totals.completed += metrics.completed_count;
  totals.rejected += metrics.rejected_count;
  totals.failed += metrics.failed_count;
  totals.max_inflight = std::max(totals.max_inflight, metrics.max_inflight_count);
}

std::size_t worker_count() {
  const auto reported = std::thread::hardware_concurrency();
  if (reported == 0u) {
    return 2u;
  }
  return std::min<std::size_t>(4u, reported);
}

RunSample run_once(std::size_t task_count, Totals& deterministic_totals, Totals& threaded_totals) {
  RunSample sample;

  topoexec::TaskExecutorConfig deterministic_config;
  deterministic_config.max_inflight = task_count;
  deterministic_config.queue_capacity = 0;
  topoexec::DeterministicTaskExecutor deterministic(deterministic_config);

  auto started = std::chrono::steady_clock::now();
  for (std::size_t index = 0; index < task_count; ++index) {
    const auto submitted = deterministic.submit(
        [index]() { return topoexec::make_text_payload("deterministic-" + std::to_string(index)); });
    if (!submitted.accepted) {
      throw std::runtime_error("deterministic task rejected: " + submitted.reason);
    }
  }
  auto completions = deterministic.run_ready();
  sample.deterministic_elapsed_ms = elapsed_ms_since(started);
  if (completions.size() != task_count) {
    throw std::runtime_error("deterministic task completion count mismatch");
  }
  accumulate(deterministic_totals, deterministic.metrics());

  topoexec::ThreadedTaskExecutorConfig threaded_config;
  threaded_config.max_workers = worker_count();
  threaded_config.max_inflight = threaded_config.max_workers;
  threaded_config.queue_capacity = task_count;
  threaded_config.overflow = "reject";
  topoexec::ThreadedTaskExecutor threaded(threaded_config);

  started = std::chrono::steady_clock::now();
  for (std::size_t index = 0; index < task_count; ++index) {
    const auto submitted =
        threaded.submit([index]() { return topoexec::make_text_payload("threaded-" + std::to_string(index)); });
    if (!submitted.accepted) {
      throw std::runtime_error("threaded task rejected: " + submitted.reason);
    }
  }
  if (!threaded.wait_for_idle(std::chrono::seconds(5))) {
    throw std::runtime_error("threaded task executor did not become idle");
  }
  completions = threaded.run_ready();
  sample.threaded_elapsed_ms = elapsed_ms_since(started);
  if (completions.size() != task_count) {
    throw std::runtime_error("threaded task completion count mismatch");
  }
  accumulate(threaded_totals, threaded.metrics());
  threaded.shutdown();

  return sample;
}

void print_json(const Args& args, const std::vector<RunSample>& samples, const Totals& deterministic_totals,
                const Totals& threaded_totals) {
  std::vector<double> deterministic_elapsed;
  std::vector<double> threaded_elapsed;
  deterministic_elapsed.reserve(samples.size());
  threaded_elapsed.reserve(samples.size());
  for (const auto& sample : samples) {
    deterministic_elapsed.push_back(sample.deterministic_elapsed_ms);
    threaded_elapsed.push_back(sample.threaded_elapsed_ms);
  }

  std::cout << "{\n";
  std::cout << "  \"ok\": true,\n";
  std::cout << "  \"case\": \"task_executor\",\n";
  std::cout << "  \"benchmark_schema\": 2,\n";
  std::cout << "  \"params\": {\"tasks\": " << args.tasks << ", \"runs\": " << args.runs << "},\n";
  std::cout << "  \"deterministic\": {\n";
  std::cout << "    \"completed\": " << deterministic_totals.completed << ",\n";
  std::cout << "    \"rejected\": " << deterministic_totals.rejected << ",\n";
  std::cout << "    \"failed\": " << deterministic_totals.failed << ",\n";
  std::cout << "    \"p50_run_elapsed_ms\": " << percentile(deterministic_elapsed, 0.50) << ",\n";
  std::cout << "    \"p95_run_elapsed_ms\": " << percentile(deterministic_elapsed, 0.95) << "\n";
  std::cout << "  },\n";
  std::cout << "  \"threaded\": {\n";
  std::cout << "    \"completed\": " << threaded_totals.completed << ",\n";
  std::cout << "    \"rejected\": " << threaded_totals.rejected << ",\n";
  std::cout << "    \"failed\": " << threaded_totals.failed << ",\n";
  std::cout << "    \"max_inflight\": " << threaded_totals.max_inflight << ",\n";
  std::cout << "    \"p50_run_elapsed_ms\": " << percentile(threaded_elapsed, 0.50) << ",\n";
  std::cout << "    \"p95_run_elapsed_ms\": " << percentile(threaded_elapsed, 0.95) << "\n";
  std::cout << "  }\n";
  std::cout << "}\n";
}

void print_text(const Args& args, const Totals& deterministic_totals, const Totals& threaded_totals) {
  std::cout << "ok\n";
  std::cout << "case: task_executor\n";
  std::cout << "benchmark_schema: 2\n";
  std::cout << "tasks: " << args.tasks << "\n";
  std::cout << "runs: " << args.runs << "\n";
  std::cout << "deterministic_completed: " << deterministic_totals.completed << "\n";
  std::cout << "threaded_completed: " << threaded_totals.completed << "\n";
  std::cout << "threaded_max_inflight: " << threaded_totals.max_inflight << "\n";
}

} // namespace

int main(int argc, char** argv) {
  try {
    const auto args = parse_args(argc, argv);
    std::vector<RunSample> samples;
    samples.reserve(args.runs);
    Totals deterministic_totals;
    Totals threaded_totals;
    for (std::size_t run = 0; run < args.runs; ++run) {
      samples.push_back(run_once(args.tasks, deterministic_totals, threaded_totals));
    }

    if (args.format == "json") {
      print_json(args, samples, deterministic_totals, threaded_totals);
    } else {
      print_text(args, deterministic_totals, threaded_totals);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
}
