#ifndef CONFIG_H
#define CONFIG_H

// -----------------------------------------------------------------------------
// Buffer Sizes
// -----------------------------------------------------------------------------
#define LLM_REQUEST_BUF_SIZE    12288   // 12KB for outgoing JSON
#define LLM_RESPONSE_BUF_SIZE   16384   // 16KB for incoming JSON
#define CHANNEL_RX_BUF_SIZE     512     // Input line buffer
#define CHANNEL_TX_BUF_SIZE     1024    // Output response buffer for serial/web relay
#define TOOL_RESULT_BUF_SIZE    512     // Tool execution result

// -----------------------------------------------------------------------------
// Conversation History
// -----------------------------------------------------------------------------
#define MAX_HISTORY_TURNS       12      // User/assistant pairs to keep
#define MAX_MESSAGE_LEN         1024    // Max length per message in history

// -----------------------------------------------------------------------------
// Agent Loop
// -----------------------------------------------------------------------------
#define MAX_TOOL_ROUNDS         5       // Max tool call iterations per request

// -----------------------------------------------------------------------------
// FreeRTOS Tasks
// -----------------------------------------------------------------------------
#define AGENT_TASK_STACK_SIZE   8192
#define CHANNEL_TASK_STACK_SIZE 4096
#define CRON_TASK_STACK_SIZE    4096
#define AGENT_TASK_PRIORITY     5
#define CHANNEL_TASK_PRIORITY   5
#define CRON_TASK_PRIORITY      4

// -----------------------------------------------------------------------------
// Queues
// -----------------------------------------------------------------------------
#define INPUT_QUEUE_LENGTH      8
#define OUTPUT_QUEUE_LENGTH     8
#define TELEGRAM_OUTPUT_QUEUE_LENGTH 4

// -----------------------------------------------------------------------------
// LLM Backend Configuration
// -----------------------------------------------------------------------------
typedef enum {
    LLM_BACKEND_ANTHROPIC = 0,
    LLM_BACKEND_OPENAI = 1,
    LLM_BACKEND_OPENROUTER = 2,
} llm_backend_t;

#define LLM_API_URL_ANTHROPIC   "https://api.anthropic.com/v1/messages"
#define LLM_API_URL_OPENAI      "https://api.openai.com/v1/chat/completions"
#define LLM_API_URL_OPENROUTER  "https://openrouter.ai/api/v1/chat/completions"

#define LLM_DEFAULT_MODEL_ANTHROPIC   "claude-sonnet-4-5"
#define LLM_DEFAULT_MODEL_OPENAI      "gpt-5.2"
#define LLM_DEFAULT_MODEL_OPENROUTER  "minimax/minimax-m2.5"

#define LLM_API_KEY_MAX_LEN       511
#define LLM_API_KEY_BUF_SIZE      (LLM_API_KEY_MAX_LEN + 1)
#define LLM_AUTH_HEADER_BUF_SIZE  (sizeof("Bearer ") - 1 + LLM_API_KEY_MAX_LEN + 1)

#define LLM_MAX_TOKENS          1024
#define HTTP_TIMEOUT_MS         30000   // 30 seconds for API calls

// -----------------------------------------------------------------------------
// System Prompt
// (Media capability suffix is appended after feature gates are defined below)
// -----------------------------------------------------------------------------
#define SYSTEM_PROMPT_BASE \
    "You are zclaw, an AI agent running on an ESP32 microcontroller. " \
    "You have 400KB of RAM and run on bare metal with FreeRTOS. " \
    "You can control GPIO pins, store persistent memories, and set schedules. " \
    "Be concise - you're on a tiny chip. " \
    "Use your tools to control hardware, remember things, and automate tasks. " \
    "Users can create custom tools with create_tool. When you call a custom tool, " \
    "you'll receive an action to execute - carry it out using your built-in tools."

// -----------------------------------------------------------------------------
// Board Feature Gates (set via Kconfig / sdkconfig.board)
// -----------------------------------------------------------------------------
#ifdef CONFIG_ZCLAW_HAS_CAMERA
#define ZCLAW_HAS_CAMERA        1
#else
#define ZCLAW_HAS_CAMERA        0
#endif

#ifdef CONFIG_ZCLAW_HAS_MICROPHONE
#define ZCLAW_HAS_MICROPHONE    1
#else
#define ZCLAW_HAS_MICROPHONE    0
#endif

#ifdef CONFIG_ZCLAW_HAS_PSRAM
#define ZCLAW_HAS_PSRAM         1
#else
#define ZCLAW_HAS_PSRAM         0
#endif

// System prompt with media capability suffix
#if ZCLAW_HAS_CAMERA && ZCLAW_HAS_MICROPHONE
#define SYSTEM_PROMPT SYSTEM_PROMPT_BASE \
    " You have a camera and microphone. Use capture_photo to take photos and " \
    "visually analyze the environment. Use record_audio to capture sound."
#elif ZCLAW_HAS_CAMERA
#define SYSTEM_PROMPT SYSTEM_PROMPT_BASE \
    " You have a camera. Use capture_photo to take photos and visually analyze " \
    "the environment."
#elif ZCLAW_HAS_MICROPHONE
#define SYSTEM_PROMPT SYSTEM_PROMPT_BASE \
    " You have a microphone. Use record_audio to capture sound."
#else
#define SYSTEM_PROMPT SYSTEM_PROMPT_BASE
#endif

// -----------------------------------------------------------------------------
// Media Capture Defaults
// -----------------------------------------------------------------------------
#define MEDIA_AUDIO_DEFAULT_MS  3000    // Default recording duration

// -----------------------------------------------------------------------------
// Camera Configuration (OV2640 DVP)
// -----------------------------------------------------------------------------
#if ZCLAW_HAS_CAMERA

#ifdef CONFIG_ZCLAW_CAM_PIN_PWDN
#define CAM_PIN_PWDN    CONFIG_ZCLAW_CAM_PIN_PWDN
#else
#define CAM_PIN_PWDN    (-1)
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_RESET
#define CAM_PIN_RESET   CONFIG_ZCLAW_CAM_PIN_RESET
#else
#define CAM_PIN_RESET   (-1)
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_XCLK
#define CAM_PIN_XCLK    CONFIG_ZCLAW_CAM_PIN_XCLK
#else
#define CAM_PIN_XCLK    10
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_SIOD
#define CAM_PIN_SIOD    CONFIG_ZCLAW_CAM_PIN_SIOD
#else
#define CAM_PIN_SIOD    40
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_SIOC
#define CAM_PIN_SIOC    CONFIG_ZCLAW_CAM_PIN_SIOC
#else
#define CAM_PIN_SIOC    39
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D7
#define CAM_PIN_D7      CONFIG_ZCLAW_CAM_PIN_D7
#else
#define CAM_PIN_D7      48
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D6
#define CAM_PIN_D6      CONFIG_ZCLAW_CAM_PIN_D6
#else
#define CAM_PIN_D6      11
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D5
#define CAM_PIN_D5      CONFIG_ZCLAW_CAM_PIN_D5
#else
#define CAM_PIN_D5      12
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D4
#define CAM_PIN_D4      CONFIG_ZCLAW_CAM_PIN_D4
#else
#define CAM_PIN_D4      14
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D3
#define CAM_PIN_D3      CONFIG_ZCLAW_CAM_PIN_D3
#else
#define CAM_PIN_D3      16
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D2
#define CAM_PIN_D2      CONFIG_ZCLAW_CAM_PIN_D2
#else
#define CAM_PIN_D2      18
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D1
#define CAM_PIN_D1      CONFIG_ZCLAW_CAM_PIN_D1
#else
#define CAM_PIN_D1      17
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_D0
#define CAM_PIN_D0      CONFIG_ZCLAW_CAM_PIN_D0
#else
#define CAM_PIN_D0      15
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_VSYNC
#define CAM_PIN_VSYNC   CONFIG_ZCLAW_CAM_PIN_VSYNC
#else
#define CAM_PIN_VSYNC   38
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_HREF
#define CAM_PIN_HREF    CONFIG_ZCLAW_CAM_PIN_HREF
#else
#define CAM_PIN_HREF    47
#endif
#ifdef CONFIG_ZCLAW_CAM_PIN_PCLK
#define CAM_PIN_PCLK    CONFIG_ZCLAW_CAM_PIN_PCLK
#else
#define CAM_PIN_PCLK    13
#endif

#define CAM_XCLK_FREQ_HZ   20000000    // 20 MHz XCLK for OV2640
#define CAM_JPEG_QUALITY    12          // 0-63, lower = better quality
#define CAM_FB_COUNT        1           // Frame buffers (1 = single capture)

#endif // ZCLAW_HAS_CAMERA

// -----------------------------------------------------------------------------
// Microphone Configuration (I2S PDM)
// -----------------------------------------------------------------------------
#if ZCLAW_HAS_MICROPHONE

#ifdef CONFIG_ZCLAW_MIC_PIN_CLK
#define MIC_PIN_CLK     CONFIG_ZCLAW_MIC_PIN_CLK
#else
#define MIC_PIN_CLK     42
#endif
#ifdef CONFIG_ZCLAW_MIC_PIN_DATA
#define MIC_PIN_DATA    CONFIG_ZCLAW_MIC_PIN_DATA
#else
#define MIC_PIN_DATA    41
#endif

#define MIC_SAMPLE_RATE     16000       // 16 kHz for speech
#define MIC_SAMPLE_BITS     16          // 16-bit samples
#define MIC_CHANNEL_NUM     1           // Mono
#define MIC_RECORD_SECS_MAX 10          // Max recording length

#endif // ZCLAW_HAS_MICROPHONE

// -----------------------------------------------------------------------------
// PSRAM-aware buffer sizes
// When PSRAM is available, use larger buffers for media payloads.
// -----------------------------------------------------------------------------
#if ZCLAW_HAS_PSRAM
#define LLM_REQUEST_BUF_SIZE_PSRAM  65536   // 64KB with PSRAM
#define LLM_RESPONSE_BUF_SIZE_PSRAM 65536   // 64KB with PSRAM
#endif

// -----------------------------------------------------------------------------
// GPIO tool safety range (configurable via Kconfig)
// -----------------------------------------------------------------------------
#ifdef CONFIG_ZCLAW_GPIO_MIN_PIN
#define GPIO_MIN_PIN            CONFIG_ZCLAW_GPIO_MIN_PIN
#else
#define GPIO_MIN_PIN            2
#endif

#ifdef CONFIG_ZCLAW_GPIO_MAX_PIN
#define GPIO_MAX_PIN            CONFIG_ZCLAW_GPIO_MAX_PIN
#else
#define GPIO_MAX_PIN            10
#endif

#ifdef CONFIG_ZCLAW_GPIO_ALLOWED_PINS
#define GPIO_ALLOWED_PINS_CSV   CONFIG_ZCLAW_GPIO_ALLOWED_PINS
#else
#define GPIO_ALLOWED_PINS_CSV   ""
#endif

#if GPIO_MIN_PIN > GPIO_MAX_PIN
#error "GPIO_MIN_PIN must be <= GPIO_MAX_PIN"
#endif

// -----------------------------------------------------------------------------
// NVS (persistent storage)
// -----------------------------------------------------------------------------
#define NVS_NAMESPACE           "zclaw"
#define NVS_NAMESPACE_CRON      "zc_cron"
#define NVS_NAMESPACE_TOOLS     "zc_tools"
#define NVS_NAMESPACE_CONFIG    "zc_config"
#define NVS_MAX_KEY_LEN         15      // NVS limit
#define NVS_MAX_VALUE_LEN       512     // Increased for tool/cron definitions

// -----------------------------------------------------------------------------
// WiFi
// -----------------------------------------------------------------------------
#define WIFI_MAX_RETRY          10
#define WIFI_RETRY_DELAY_MS     1000

// -----------------------------------------------------------------------------
// Telegram
// -----------------------------------------------------------------------------
#define TELEGRAM_API_URL        "https://api.telegram.org/bot"
#define TELEGRAM_POLL_TIMEOUT   30      // Long polling timeout (seconds)
#define TELEGRAM_POLL_INTERVAL  100     // ms between poll attempts on error
#define TELEGRAM_MAX_MSG_LEN    4096    // Max message length
#define START_COMMAND_COOLDOWN_MS 30000 // Debounce repeated Telegram /start bursts

// -----------------------------------------------------------------------------
// Cron / Scheduler
// -----------------------------------------------------------------------------
#define CRON_CHECK_INTERVAL_MS  60000   // Check schedules every minute
#define CRON_MAX_ENTRIES        16      // Max scheduled tasks
#define CRON_MAX_ACTION_LEN     256     // Max action string length

// -----------------------------------------------------------------------------
// Factory Reset
// -----------------------------------------------------------------------------
#define FACTORY_RESET_PIN       9       // Hold low for 5 seconds to reset
#define FACTORY_RESET_HOLD_MS   5000

// -----------------------------------------------------------------------------
// NTP (time sync)
// -----------------------------------------------------------------------------
#define NTP_SERVER              "pool.ntp.org"
#define NTP_SYNC_TIMEOUT_MS     10000
#define DEFAULT_TIMEZONE_POSIX  "UTC0"
#define TIMEZONE_MAX_LEN        64

// -----------------------------------------------------------------------------
// Dynamic Tools
// -----------------------------------------------------------------------------
#define MAX_DYNAMIC_TOOLS       8       // Max user-registered tools
#define TOOL_NAME_MAX_LEN       24
#define TOOL_DESC_MAX_LEN       128

// -----------------------------------------------------------------------------
// Boot Loop Protection
// -----------------------------------------------------------------------------
#define MAX_BOOT_FAILURES       3       // Enter safe mode after N consecutive failures
#define BOOT_SUCCESS_DELAY_MS   30000   // Clear boot counter after this time connected

// -----------------------------------------------------------------------------
// Rate Limiting
// -----------------------------------------------------------------------------
#define RATELIMIT_MAX_PER_HOUR      30      // Max LLM requests per hour
#define RATELIMIT_MAX_PER_DAY       200     // Max LLM requests per day
#define RATELIMIT_ENABLED           1       // Set to 0 to disable

#endif // CONFIG_H
