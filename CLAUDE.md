# CLAUDE.md — AI Assistant Guide for zclaw

## Project Overview

zclaw is a minimal AI personal assistant firmware for ESP32 microcontrollers, written in C. It runs on boards with a strict all-in firmware budget of **≤ 888 KiB** and provides an LLM-powered agent that can control GPIO, schedule tasks, and persist state across reboots.

- **Language**: C (C99)
- **Framework**: ESP-IDF v5.4 + FreeRTOS
- **Targets**: ESP32-C3 (default), ESP32-S3, ESP32-C6
- **Version**: see `VERSION` file (currently 2.0.3)
- **License**: MIT

## Repository Structure

```
zclaw/
├── main/                   # All application source code (~45 .c/.h files)
├── test/
│   ├── host/               # Host-based unit tests (C + Python, no hardware needed)
│   └── api/                # Live provider API tests (manual/local)
├── scripts/                # Build, flash, test, and utility scripts
├── docs/                   # Static documentation assets (images)
├── docs-site/              # Full HTML documentation site (zclaw.dev)
├── .github/workflows/      # CI: host tests, size guard, stack guard, target matrix
├── CMakeLists.txt          # ESP-IDF root project config
├── partitions.csv          # Custom 4MB flash partition table with OTA
├── sdkconfig.defaults      # Default ESP-IDF build config (esp32c3)
├── sdkconfig.qemu.defaults # QEMU emulator overrides
├── sdkconfig.secure        # Flash encryption config
├── sdkconfig.test          # Device test build config
├── install.sh              # Interactive dev environment setup
├── VERSION                 # Release version string
└── CLAUDE.md               # This file
```

## Key Commands

### Build & Flash (requires ESP-IDF v5.4)
```bash
./scripts/build.sh              # Build firmware
./scripts/flash.sh              # Flash to connected device
./scripts/flash-secure.sh       # Flash with encryption
./scripts/provision.sh          # Provision WiFi/LLM credentials to NVS
./scripts/monitor.sh            # Serial monitor
```

### Testing
```bash
./scripts/test.sh host          # Run host unit tests (C + Python, no hardware)
./scripts/test.sh device        # Build device test firmware
./scripts/test.sh all           # Host tests + device test build
```

Host tests compile with **gcc** and require the **cJSON** library:
- macOS: `brew install cjson`
- Ubuntu: `apt install libcjson-dev`

Host C tests run with AddressSanitizer enabled by default (`ASAN=0` to disable).

### Other Useful Scripts
```bash
./scripts/emulate.sh            # Run in QEMU with stubbed LLM
./scripts/web-relay.sh          # Hosted relay + mobile chat UI
./scripts/benchmark.sh          # Latency benchmarking
./scripts/size.sh               # Check binary size
./scripts/check-binary-size.sh  # Enforce 888 KiB budget (used in CI)
./scripts/check-stack-usage.sh  # Verify function stack limits (used in CI)
./scripts/clean.sh              # Clean build artifacts
./scripts/bump-version.sh       # Bump version
```

## Architecture

### FreeRTOS Task Model
```
main()
├── WiFi + NTP initialization
├── NVS flash setup
├── Boot guard check
├── Channel task   — serial/USB input/output
├── Agent task     — message processing + LLM agentic loop
└── Cron task      — scheduled action executor
```

### Data Flow
```
User → Channel/Telegram → Input Queue → Agent → LLM API / Tools → Output Queue → Channel/Telegram
```

### Core Modules (in `main/`)

| Module | Files | Purpose |
|--------|-------|---------|
| Entry point | `main.c` | WiFi, NVS, event loop init, task creation |
| Agent loop | `agent.c/h` | Agentic LLM loop with tool-use, conversation history |
| LLM client | `llm.c/h`, `llm_auth.c/h` | HTTP client for Anthropic/OpenAI/OpenRouter |
| Tools | `tools.c/h`, `tools_gpio.c`, `tools_i2c.c`, `tools_memory.c`, `tools_cron.c`, `tools_system.c`, `tools_common.c/h` | Tool registry, dispatch, and built-in tool handlers |
| User tools | `user_tools.c/h` | Custom user-defined tools (stored in NVS) |
| Channel | `channel.c/h` | Serial/USB input/output abstraction |
| Telegram | `telegram.c/h`, `telegram_update.c/h` | Telegram bot with long polling |
| Storage | `memory.c/h`, `memory_keys.c/h`, `nvs_keys.h` | NVS persistent storage abstraction |
| Scheduler | `cron.c/h`, `cron_utils.c/h` | Periodic, daily, and one-shot schedules |
| JSON | `json_util.c/h` | cJSON wrapper for conversation/tool JSON |
| Config | `config.h` | All compile-time tunables (buffer sizes, limits, defaults) |
| Messages | `messages.h` | Queue payload type definitions |
| Rate limit | `ratelimit.c/h` | Hourly/daily LLM request caps |
| Security | `security.c/h` | Sensitive key redaction, security utils |
| Text buffer | `text_buffer.c/h` | Safe append-to-buffer utility |
| OTA | `ota.c/h` | Over-the-air update mechanism |
| Boot guard | `boot_guard.c/h` | Boot loop protection (safe mode after 3 failures) |

## Coding Conventions

### Style
- **C99 standard** (`-std=c99`)
- 4-space indentation
- `snake_case` for functions and variables
- `UPPER_CASE` for macros and constants
- `type_name_t` suffix for typedefs (structs, enums)
- Prefix static module-level variables with `s_` (e.g., `s_input_queue`, `s_history`)

### Module Pattern
Every module follows this pattern:
- Public API in header: `module_init()`, `module_start()`, `module_action()`
- Private state as `static` variables at file scope
- Logging tag: `static const char *TAG = "module_name";`
- ESP-IDF logging macros: `ESP_LOGE()`, `ESP_LOGW()`, `ESP_LOGI()`, `ESP_LOGD()`

### Error Handling
- Use `esp_err_t` return for initialization/system functions
- Use `bool` return for tool handlers and simple checks
- Tool handlers write error messages into the `result` buffer on failure
- Always log errors with `ESP_LOGE(TAG, ...)`

### Tool Handler Signature
All tool handlers match:
```c
bool handler_name(const cJSON *input, char *result, size_t result_len);
```
They return `true` on success, `false` on failure, and write output into `result`.

### Function Naming
- Public: `module_action()` — e.g., `memory_set()`, `agent_start()`, `cron_delete()`
- Tool handlers: `tools_<name>_handler()` — e.g., `tools_gpio_write_handler()`
- Private/static: descriptive names — e.g., `history_add()`, `update_time_window()`

## Testing

### Host C Tests (`test/host/`)
- Custom assertion macros (no external test framework)
- `TEST(name)` macro defines test functions returning `int` (0 = pass, 1 = fail)
- `ASSERT(cond)` macro with file/line on failure
- Test runner aggregates all suites in `test_runner.c`
- Mock files for ESP-IDF, FreeRTOS, LLM, tools, etc.
- Compiled with strict warnings: `-Wall -Wextra -Werror -Wshadow -Wformat=2`

### Test Suites
- `test_json.c` — JSON parsing
- `test_tools_parse.c` — Tool input parsing
- `test_json_util_integration.c` — JSON utility integration
- `test_runtime_utils.c` — Runtime utilities
- `test_memory_keys.c` — NVS key management
- `test_telegram_update.c` — Telegram message parsing
- `test_agent.c` — Agent agentic loop
- `test_tools_gpio_policy.c` — GPIO safety guardrails
- `test_llm_auth.c` — API authentication

### Host Python Tests
- `test_web_relay.py` — Web relay functionality
- `test_install_provision_scripts.py` — Provisioning scripts
- `test_qemu_live_llm_bridge.py` — QEMU LLM bridge
- `test_api_provider_harness.py` — API provider integration

Run with: `python3 -m unittest -q <test_file>.py` (from `test/host/`)

## CI/CD (GitHub Actions)

Four workflows run on every push and pull request:

1. **Host Tests** (`host-tests.yml`) — Runs `./scripts/test.sh host` on ubuntu-latest
2. **Firmware Size Guard** (`firmware-size-guard.yml`) — Builds firmware and enforces ≤ 888 KiB (909312 bytes) via `check-binary-size.sh`
3. **Firmware Stack Guard** (`firmware-stack-guard.yml`) — Validates function stack usage limits for critical functions (e.g., `cron.c:check_entries` ≤ 1024B, `telegram.c:telegram_poll` ≤ 2048B)
4. **Firmware Target Matrix** (`firmware-target-matrix.yml`) — Builds for all three targets: esp32c3, esp32s3, esp32c6

## Critical Constraints

### Binary Size Budget
The firmware binary must stay **≤ 888 KiB** (909312 bytes) all-in. This includes zclaw app logic plus ESP-IDF runtime, WiFi/networking, TLS/crypto, and cert bundle. The CI size guard enforces this on every PR.

**Impact on development:**
- Avoid adding unnecessary dependencies
- Keep features minimal and focused
- Use `-Os` size optimization (set in `sdkconfig.defaults`)
- WPA3 and IPv6 are disabled for size
- Logging level capped at INFO

### Stack Usage
FreeRTOS tasks have fixed stack sizes (4-8 KB). The CI stack guard monitors critical functions. Avoid deep recursion or large stack-allocated buffers.

### Memory
~400 KB RAM total. Buffer sizes are carefully tuned in `config.h`. Changes to buffer sizes affect overall memory pressure.

## Configuration

### Compile-Time (`config.h`)
All tunables are `#define` constants: buffer sizes, queue depths, rate limits, GPIO bounds, LLM endpoints/models, task stack sizes, etc.

### Build-Time (`sdkconfig.defaults`, `Kconfig.projbuild`)
ESP-IDF menuconfig options: WiFi, flash size, optimization level, GPIO safety bounds, stub modes for testing.

### Runtime (NVS)
Credentials and settings stored in NVS flash: WiFi SSID/password, LLM API key/backend/model, Telegram token, timezone, rate limit state. Four NVS namespaces: `zclaw`, `zc_cron`, `zc_tools`, `zc_config`.

## LLM Provider Support

Three backends configured in `config.h`:
- **Anthropic** — `claude-sonnet-4-5` (default)
- **OpenAI** — `gpt-5.2`
- **OpenRouter** — `minimax/minimax-m2.5`

Backend and model are configurable at runtime via NVS keys.

## Security Notes

- Flash encryption available (`sdkconfig.secure`, `./scripts/flash-secure.sh`)
- API keys stored in NVS (optionally encrypted)
- Sensitive NVS keys are redacted in logs (`security.c`)
- GPIO safety: allowlist + min/max pin range prevents accidental hardware damage
- Rate limiting: 30 requests/hour, 200/day (persistent across reboots)
- Boot loop protection: safe mode after 3 consecutive boot failures
- Never commit files in `keys/`, `*.pem`, `*.key`, `secrets.h`, or `.env`
