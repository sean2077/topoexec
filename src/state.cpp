#include "topoexec/runtime/state.hpp"

#include <utility>

namespace topoexec {

bool RuntimeStateSnapshot::contains(const std::string& key) const {
  return values.count(key) != 0u;
}

RuntimePayloadPtr RuntimeStateSnapshot::get(const std::string& key) const {
  const auto found = values.find(key);
  if (found == values.end()) {
    return nullptr;
  }
  return found->second;
}

RuntimeStateWriteResult RuntimeStateStore::validate_write_locked(const std::string& name_space, const std::string& key,
                                                                 const std::string& writer,
                                                                 const RuntimePayloadPtr& payload) {
  if (name_space.empty()) {
    ++metrics_.rejected_write_count;
    return {false, "state namespace must not be empty"};
  }
  if (key.empty()) {
    ++metrics_.rejected_write_count;
    return {false, "state key must not be empty"};
  }
  if (writer.empty()) {
    ++metrics_.rejected_write_count;
    return {false, "state writer must be an explicit component.port"};
  }
  if (payload == nullptr) {
    ++metrics_.rejected_write_count;
    return {false, "state payload must not be null"};
  }
  const auto namespace_found = namespaces_.find(name_space);
  if (namespace_found == namespaces_.end()) {
    return {true, {}};
  }
  const auto entry_found = namespace_found->second.find(key);
  if (entry_found == namespace_found->second.end() || entry_found->second.writer.empty() ||
      entry_found->second.writer == writer) {
    return {true, {}};
  }
  ++metrics_.rejected_write_count;
  return {false, "state key " + name_space + "." + key + " already has writer " + entry_found->second.writer};
}

RuntimeStateWriteResult RuntimeStateStore::seed_value(const std::string& name_space, const std::string& key,
                                                      const std::string& writer, RuntimePayloadPtr payload) {
  std::lock_guard lock(mutex_);
  auto validation = validate_write_locked(name_space, key, writer, payload);
  if (!validation.accepted) {
    return validation;
  }
  auto& entry = namespaces_[name_space][key];
  entry.current = std::move(payload);
  entry.pending.reset();
  entry.writer = writer;
  ++entry.version;
  return {true, {}};
}

RuntimeStateWriteResult RuntimeStateStore::stage_write(const std::string& name_space, const std::string& key,
                                                       const std::string& writer, RuntimePayloadPtr payload) {
  std::lock_guard lock(mutex_);
  auto validation = validate_write_locked(name_space, key, writer, payload);
  if (!validation.accepted) {
    return validation;
  }
  auto& entry = namespaces_[name_space][key];
  entry.pending = std::move(payload);
  entry.writer = writer;
  ++metrics_.staged_write_count;
  return {true, {}};
}

std::size_t RuntimeStateStore::commit_epoch_boundary() {
  std::lock_guard lock(mutex_);
  std::size_t committed = 0;
  for (auto& [name_space, values] : namespaces_) {
    (void)name_space;
    for (auto& [key, entry] : values) {
      (void)key;
      if (entry.pending == nullptr) {
        continue;
      }
      entry.current = std::move(entry.pending);
      entry.pending.reset();
      ++entry.version;
      ++committed;
    }
  }
  ++metrics_.epoch;
  metrics_.committed_write_count += committed;
  metrics_.namespace_count = namespaces_.size();
  metrics_.current_value_count = current_value_count_locked();
  return committed;
}

RuntimeStateSnapshot RuntimeStateStore::snapshot(const std::string& name_space) const {
  std::lock_guard lock(mutex_);
  ++metrics_.snapshot_read_count;
  RuntimeStateSnapshot snapshot;
  snapshot.name_space = name_space;
  snapshot.epoch = metrics_.epoch;
  const auto found = namespaces_.find(name_space);
  if (found == namespaces_.end()) {
    return snapshot;
  }
  for (const auto& [key, entry] : found->second) {
    if (entry.current != nullptr) {
      snapshot.values[key] = entry.current;
    }
  }
  return snapshot;
}

RuntimePayloadPtr RuntimeStateStore::read(const std::string& name_space, const std::string& key) const {
  return snapshot(name_space).get(key);
}

RuntimeStateStoreMetrics RuntimeStateStore::metrics() const {
  std::lock_guard lock(mutex_);
  auto metrics = metrics_;
  metrics.namespace_count = namespaces_.size();
  metrics.current_value_count = current_value_count_locked();
  return metrics;
}

std::size_t RuntimeStateStore::current_value_count_locked() const {
  std::size_t count = 0;
  for (const auto& [name_space, values] : namespaces_) {
    (void)name_space;
    for (const auto& [key, entry] : values) {
      (void)key;
      if (entry.current != nullptr) {
        ++count;
      }
    }
  }
  return count;
}

void ConfigSnapshotStore::set_graph_config(ConfigView config) {
  std::lock_guard lock(mutex_);
  graph_config_ = std::move(config);
}

void ConfigSnapshotStore::set_component_config(const std::string& component_id, ConfigView config) {
  std::lock_guard lock(mutex_);
  component_configs_[component_id] = std::move(config);
  metrics_.component_config_count = component_configs_.size();
}

ConfigView ConfigSnapshotStore::graph_config() const {
  std::lock_guard lock(mutex_);
  ++metrics_.snapshot_read_count;
  return graph_config_;
}

ConfigView ConfigSnapshotStore::component_config(const std::string& component_id) const {
  std::lock_guard lock(mutex_);
  ++metrics_.snapshot_read_count;
  const auto found = component_configs_.find(component_id);
  if (found == component_configs_.end()) {
    return {};
  }
  return found->second;
}

ConfigSnapshotUpdateResult ConfigSnapshotStore::stage_component_config_update(const std::string& component_id,
                                                                              ConfigView config,
                                                                              bool apply_on_epoch_boundary) {
  std::lock_guard lock(mutex_);
  if (component_id.empty()) {
    ++metrics_.rejected_update_count;
    return {false, "component id must not be empty"};
  }
  if (apply_on_epoch_boundary) {
    if (pending_transaction_id_ == 0u) {
      pending_transaction_id_ = next_transaction_id_++;
    }
    pending_component_configs_[component_id] = std::move(config);
    ++metrics_.staged_update_count;
    return {true, {}, pending_transaction_id_};
  }
  component_configs_[component_id] = std::move(config);
  metrics_.component_config_count = component_configs_.size();
  ++metrics_.immediate_update_count;
  ++metrics_.version;
  last_transaction_.transaction_id = next_transaction_id_++;
  last_transaction_.version = metrics_.version;
  last_transaction_.epoch = metrics_.epoch;
  last_transaction_.applied_at = std::chrono::system_clock::now();
  last_transaction_.applied_components = {component_id};
  metrics_.last_transaction_id = last_transaction_.transaction_id;
  return {true, {}, last_transaction_.transaction_id};
}

std::map<std::string, ConfigView> ConfigSnapshotStore::pending_component_config_updates() const {
  std::lock_guard lock(mutex_);
  return pending_component_configs_;
}

std::size_t ConfigSnapshotStore::rollback_pending_updates() {
  std::lock_guard lock(mutex_);
  const auto rolled_back = pending_component_configs_.size();
  pending_component_configs_.clear();
  pending_transaction_id_ = 0;
  metrics_.rolled_back_update_count += rolled_back;
  metrics_.rejected_update_count += rolled_back;
  return rolled_back;
}

std::size_t ConfigSnapshotStore::commit_epoch_boundary() {
  std::lock_guard lock(mutex_);
  const auto committed = pending_component_configs_.size();
  std::vector<std::string> applied_components;
  applied_components.reserve(committed);
  for (auto& [component_id, config] : pending_component_configs_) {
    applied_components.push_back(component_id);
    component_configs_[component_id] = std::move(config);
  }
  pending_component_configs_.clear();
  ++metrics_.epoch;
  metrics_.committed_update_count += committed;
  metrics_.component_config_count = component_configs_.size();
  if (committed != 0u) {
    ++metrics_.version;
    last_transaction_.transaction_id = pending_transaction_id_;
    last_transaction_.version = metrics_.version;
    last_transaction_.epoch = metrics_.epoch;
    last_transaction_.applied_at = std::chrono::system_clock::now();
    last_transaction_.applied_components = std::move(applied_components);
    metrics_.last_transaction_id = last_transaction_.transaction_id;
    pending_transaction_id_ = 0;
  }
  return committed;
}

ConfigTransactionInfo ConfigSnapshotStore::last_transaction() const {
  std::lock_guard lock(mutex_);
  return last_transaction_;
}

ConfigSnapshotStoreMetrics ConfigSnapshotStore::metrics() const {
  std::lock_guard lock(mutex_);
  auto metrics = metrics_;
  metrics.component_config_count = component_configs_.size();
  return metrics;
}

} // namespace topoexec
