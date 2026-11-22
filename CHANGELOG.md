# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- README with a short description of the project.
- Apache License 2.0.
- Developer guide to the Home Assistant API for remote controls
  (`docs/dev/home-assistant-api.md`). It covers authentication for REST and
  WebSocket with the error responses, reading dashboards and entity states,
  mapping dashboard actions to service calls, and webhooks. Every use case has
  a C example for ESP-IDF.
- Bruno collection in `test/bruno` with one request for each use case in the
  API guide. The token is read from `test/bruno/.env`, which is not under
  version control.
- OpenSpec change proposals for the firmware build (`setup-firmware-build`),
  the first-start setup with configuration upload (`add-initial-setup`), and
  the remote control UI with status bar (`add-remote-ui`).
