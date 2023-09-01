<!-- Copyright (c) 2023 Golioth, Inc. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Clear Ostentus memory when app starts

### Changed

- Merge changes from [`golioth/reference-design-template@template_v1.1.0`](https://github.com/golioth/reference-design-template/tree/template_v1.1.0)
- Change sensor readings logging level from `LOG_INF` to `LOG_DBG`
- Upgrade `golioth/golioth-zephyr-boards` dependency to [`v1.0.1`](https://github.com/golioth/golioth-zephyr-boards/tree/v1.0.1)

### Removed

- Remove `clang-format` from pre-commit hooks
- Remove logging of JSON data sent to Golioth

## [1.6.0] - 2023-07-31

### Fixed

- An incorrect `time` value is no longer reported when fake GPS is enabled.

### Changed

- Fake GPS is now configured using the Golioth Settings service (it previously used the LightDB State service).
- Reverted back to LightDB State examples.

## [1.5.0] - 2023-07-26

### Added
- Add a `CHANGELOG.md` file to track changes moving forward.
- Add `get_network_info` RPC.
- Log modem firmware version.

### Fixed

* Add missing copyright & license info.
* Fix `CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN` and `CONFIG_MINIMAL_LIBC_MALLOC_ARENA_SIZE` build warnings.

### Changed

- Merge changes from [`golioth/reference-design-template@21d3a47`](https://github.com/golioth/reference-design-template/commit/21d3a4794628fad5c4ede64d3fa30087d7283ac7)
- Upgrade dependencies:
  - [`golioth/golioth-zephyr-sdk@f01824d`](https://github.com/golioth/golioth-zephyr-sdk/commit/f01824d8f0943463ee07cb493103a63221599c79)
  - [`golioth/golioth-zephyr-boards@0a0a27d`](https://github.com/golioth/golioth-zephyr-boards/commit/0a0a27dc2facc4245be0d15b9b36ce526cbf9262)
