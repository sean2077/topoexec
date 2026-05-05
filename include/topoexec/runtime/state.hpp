#pragma once

// Public API category: experimental state/config snapshot surface. This API
// keeps mutable graph-level state explicit and epoch-boundary committed.

#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/payload.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace topoexec {

struct RuntimeStateWriteResult {
  bool accepted{false};
  std::string reason;
};

struct RuntimeStateSnapshot {
  std::string name_space;
  std::uint64_t epoch{0};
  std::map<std::string, RuntimePayloadPtr> values;

  bool contains(const std::string& key) const;
  RuntimePayloadPtr get(const std::string& key) const;
};

struct RuntimeStateStoreMetrics {
  std::uint64_t epoch{0};
  std::size_t namespace_count{0};
  std::size_t current_value_count{0};
  std::size_t staged_write_count{0};
  std::size_t committed_write_count{0};
  std::size_t rejected_write_count{0};
  std::size_t snapshot_read_count{0};
};

class RuntimeStateStore {
public:
  RuntimeStateWriteResult seed_value(const std::string& name_space, const std::string& key, const std::string& writer,
                                     RuntimePayloadPtr payload);
  RuntimeStateWriteResult stage_write(const std::string& name_space, const std::string& key, const std::string& writer,
                                      RuntimePayloadPtr payload);
  std::size_t commit_epoch_boundary();
  RuntimeStateSnapshot snapshot(const std::string& name_space) const;
  RuntimePayloadPtr read(const std::string& name_space, const std::string& key) const;
  RuntimeStateStoreMetrics metrics() const;

private:
  struct Entry {
    RuntimePayloadPtr current;
    RuntimePayloadPtr pending;
    std::string writer;
    std::uint64_t version{0};
  };

  RuntimeStateWriteResult validate_write_locked(const std::string& name_space, const std::string& key,
                                                const std::string& writer, const RuntimePayloadPtr& payload);
  std::size_t current_value_count_locked() const;

  mutable std::mutex mutex_;
  std::map<std::string, std::map<std::string, Entry>> namespaces_;
  mutable RuntimeStateStoreMetrics metrics_;
};

struct ConfigSnapshotUpdateResult {
  bool accepted{false};
  std::string reason;
};

struct ConfigSnapshotStoreMetrics {
  std::uint64_t epoch{0};
  std::size_t component_config_count{0};
  std::size_t staged_update_count{0};
  std::size_t committed_update_count{0};
  std::size_t immediate_update_count{0};
  std::size_t rejected_update_count{0};
  std::size_t snapshot_read_count{0};
};

class ConfigSnapshotStore {
public:
  void set_graph_config(ConfigView config);
  void set_component_config(const std::string& component_id, ConfigView config);

  ConfigView graph_config() const;
  ConfigView component_config(const std::string& component_id) const;

  ConfigSnapshotUpdateResult stage_component_config_update(const std::string& component_id, ConfigView config,
                                                           bool apply_on_epoch_boundary = true);
  std::size_t commit_epoch_boundary();
  ConfigSnapshotStoreMetrics metrics() const;

private:
  mutable std::mutex mutex_;
  ConfigView graph_config_;
  std::map<std::string, ConfigView> component_configs_;
  std::map<std::string, ConfigView> pending_component_configs_;
  mutable ConfigSnapshotStoreMetrics metrics_;
};

} // namespace topoexec
