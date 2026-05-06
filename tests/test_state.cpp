#include "topoexec/runtime/state.hpp"

#include <gtest/gtest.h>

#include <vector>

TEST(StateStore, StagedWritesBecomeVisibleOnlyAfterEpochBoundary) {
  topoexec::RuntimeStateStore store;

  auto staged = store.stage_write("robot", "pose", "estimator.state",
                                  topoexec::make_shared_payload(topoexec::make_text_payload("p1")));
  ASSERT_TRUE(staged.accepted) << staged.reason;

  auto before = store.snapshot("robot");
  EXPECT_FALSE(before.contains("pose"));

  EXPECT_EQ(store.commit_epoch_boundary(), 1u);

  auto after = store.snapshot("robot");
  ASSERT_TRUE(after.contains("pose"));
  EXPECT_EQ(*after.get("pose"), "p1");

  const auto metrics = store.metrics();
  EXPECT_EQ(metrics.staged_write_count, 1u);
  EXPECT_EQ(metrics.committed_write_count, 1u);
  EXPECT_EQ(metrics.current_value_count, 1u);
  EXPECT_EQ(metrics.snapshot_read_count, 2u);
}

TEST(StateStore, SnapshotsAreImmutableAndSingleWriterIsEnforced) {
  topoexec::RuntimeStateStore store;
  ASSERT_TRUE(store
                  .seed_value("robot", "mode", "planner.state",
                              topoexec::make_shared_payload(topoexec::make_text_payload("hold")))
                  .accepted);

  auto snapshot = store.snapshot("robot");
  ASSERT_TRUE(snapshot.contains("mode"));
  EXPECT_EQ(*snapshot.get("mode"), "hold");

  auto staged = store.stage_write("robot", "mode", "planner.state",
                                  topoexec::make_shared_payload(topoexec::make_text_payload("go")));
  ASSERT_TRUE(staged.accepted) << staged.reason;
  EXPECT_EQ(store.commit_epoch_boundary(), 1u);

  EXPECT_EQ(*snapshot.get("mode"), "hold");
  auto updated = store.snapshot("robot");
  ASSERT_TRUE(updated.contains("mode"));
  EXPECT_EQ(*updated.get("mode"), "go");

  const auto rejected = store.stage_write("robot", "mode", "other.state",
                                          topoexec::make_shared_payload(topoexec::make_text_payload("stop")));
  EXPECT_FALSE(rejected.accepted);
  EXPECT_NE(rejected.reason.find("already has writer planner.state"), std::string::npos);
  EXPECT_EQ(store.metrics().rejected_write_count, 1u);
}

TEST(ConfigSnapshotStore, ComponentConfigUpdatesRespectEpochBoundary) {
  topoexec::ConfigSnapshotStore store;
  topoexec::ConfigView graph_config;
  graph_config.values["profile"] = "alpha";
  store.set_graph_config(graph_config);

  topoexec::ConfigView initial;
  initial.values["gain"] = "1";
  store.set_component_config("controller", initial);

  topoexec::ConfigView next;
  next.values["gain"] = "2";
  const auto update = store.stage_component_config_update("controller", next);
  ASSERT_TRUE(update.accepted) << update.reason;
  EXPECT_NE(update.transaction_id, 0u);
  ASSERT_EQ(store.pending_component_config_updates().size(), 1u);

  EXPECT_EQ(store.graph_config().values.at("profile"), "alpha");
  EXPECT_EQ(store.component_config("controller").values.at("gain"), "1");

  EXPECT_EQ(store.commit_epoch_boundary(), 1u);

  EXPECT_EQ(store.component_config("controller").values.at("gain"), "2");
  const auto transaction = store.last_transaction();
  EXPECT_EQ(transaction.transaction_id, update.transaction_id);
  EXPECT_EQ(transaction.version, 1u);
  EXPECT_EQ(transaction.epoch, 1u);
  EXPECT_EQ(transaction.applied_components, std::vector<std::string>({"controller"}));
  const auto metrics = store.metrics();
  EXPECT_EQ(metrics.version, 1u);
  EXPECT_EQ(metrics.last_transaction_id, update.transaction_id);
  EXPECT_EQ(metrics.staged_update_count, 1u);
  EXPECT_EQ(metrics.committed_update_count, 1u);
  EXPECT_EQ(metrics.component_config_count, 1u);
  EXPECT_EQ(metrics.snapshot_read_count, 3u);
}

TEST(ConfigSnapshotStore, PendingConfigUpdatesCanRollbackWithoutChangingActiveSnapshot) {
  topoexec::ConfigSnapshotStore store;
  topoexec::ConfigView initial;
  initial.values["gain"] = "1";
  store.set_component_config("controller", initial);

  topoexec::ConfigView next;
  next.values["gain"] = "2";
  const auto update = store.stage_component_config_update("controller", next);
  ASSERT_TRUE(update.accepted) << update.reason;

  EXPECT_EQ(store.rollback_pending_updates(), 1u);
  EXPECT_TRUE(store.pending_component_config_updates().empty());
  EXPECT_EQ(store.component_config("controller").values.at("gain"), "1");
  EXPECT_EQ(store.commit_epoch_boundary(), 0u);
  EXPECT_EQ(store.component_config("controller").values.at("gain"), "1");

  const auto metrics = store.metrics();
  EXPECT_EQ(metrics.version, 0u);
  EXPECT_EQ(metrics.last_transaction_id, 0u);
  EXPECT_EQ(metrics.rolled_back_update_count, 1u);
  EXPECT_EQ(metrics.rejected_update_count, 1u);
  EXPECT_EQ(metrics.committed_update_count, 0u);
}
