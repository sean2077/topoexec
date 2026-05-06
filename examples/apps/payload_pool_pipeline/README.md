# Payload Pool Pipeline

Reference-app slice for in-process payload ownership:

```text
source.metadata --copy-------> copy_sink
source.shared   --shared_view--> shared_sink
camera.frame    --loaned_view--> frame_sink
```

Run after building examples:

```bash
./build/topoexec_app_payload_pool_pipeline
```

Stable output:

```text
copy_payload_copy_count=1
loaned_payload_copy_count=0
loaned_address_preserved=true
pool_detached_count=1
```

The app compares a copied text payload with shared/loaned paths. The loaned frame
comes from `BufferPool`, is detached into a runtime `FrameView`, and preserves
its buffer address through `loaned_view`. This is in-process ownership evidence,
not external shared memory.
