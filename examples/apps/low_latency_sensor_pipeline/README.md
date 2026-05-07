# Low-latency Sensor Pipeline

Reference-app slice for a camera/sensor-style path:

```text
source --latest/overwrite--> preprocessor --latest/overwrite--> detector --latest/overwrite--> tracker
```

Run after building examples:

```bash
./build/topoexec_app_low_latency_sensor_pipeline
```

Stable output:

```text
tracker_latest=track:detection:preprocessed:frame-3
source_latest_overwrite_count=2
pipeline_payload_copy_count=0
```

The source bursts three frames before downstream work consumes them. The first
edge keeps only the newest frame, so the tracker sees frame 3 and the source edge
reports two explicit overwrites. All edges use `shared_view`; no external camera or
zero-copy adapter is implied.
