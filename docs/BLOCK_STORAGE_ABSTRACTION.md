# Block/Storage Abstraction

AegisOS v41 introduces a small kernel block registry. The purpose is to stop treating the v40 image only as a build artifact and start giving the kernel a storage model.

The first registered device is:

```text
aegis-flash0
```

It exposes the v40 regions as partitions:

```text
AEGIS_BOOT
AEGIS_ROOT
AEGIS_CONFIG
```

`AEGIS_CONFIG` is marked writable and persistent so later milestones can attach configfs and first-boot provisioning to it.

v41 is still a proof abstraction, not the final disk driver stack. v43+ can build service messaging on top of it, while v46 can connect router services to persistent configuration.
