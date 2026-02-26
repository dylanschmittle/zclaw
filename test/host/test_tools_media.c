/*
 * Host tests for media capture module (tools_media.c)
 * Tests: base64 encoding, pending image state, JSON vision integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_esp.h"
#include "tools_media.h"
#include "json_util.h"
#include "mock_llm.h"
#include "cJSON.h"

#define TEST(name) static int test_##name(void)
#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: '%s' != '%s' (line %d)\n", (a), (b), __LINE__); \
        return 1; \
    } \
} while(0)

// --- Base64 encoding tests ---

TEST(base64_empty)
{
    size_t out_len = 0;
    char *b64 = media_test_base64_encode(NULL, 0, &out_len);
    ASSERT(b64 != NULL);
    ASSERT(out_len == 0);
    ASSERT_STR_EQ(b64, "");
    free(b64);
    return 0;
}

TEST(base64_one_byte)
{
    uint8_t data[] = {0x4D};  // 'M'
    size_t out_len = 0;
    char *b64 = media_test_base64_encode(data, 1, &out_len);
    ASSERT(b64 != NULL);
    ASSERT(out_len == 4);
    ASSERT_STR_EQ(b64, "TQ==");
    free(b64);
    return 0;
}

TEST(base64_two_bytes)
{
    uint8_t data[] = {0x4D, 0x61};  // "Ma"
    size_t out_len = 0;
    char *b64 = media_test_base64_encode(data, 2, &out_len);
    ASSERT(b64 != NULL);
    ASSERT(out_len == 4);
    ASSERT_STR_EQ(b64, "TWE=");
    free(b64);
    return 0;
}

TEST(base64_three_bytes)
{
    uint8_t data[] = {0x4D, 0x61, 0x6E};  // "Man"
    size_t out_len = 0;
    char *b64 = media_test_base64_encode(data, 3, &out_len);
    ASSERT(b64 != NULL);
    ASSERT(out_len == 4);
    ASSERT_STR_EQ(b64, "TWFu");
    free(b64);
    return 0;
}

TEST(base64_hello)
{
    const char *input = "Hello, World!";
    size_t out_len = 0;
    char *b64 = media_test_base64_encode((const uint8_t *)input, strlen(input), &out_len);
    ASSERT(b64 != NULL);
    ASSERT_STR_EQ(b64, "SGVsbG8sIFdvcmxkIQ==");
    free(b64);
    return 0;
}

// --- Pending image state tests ---

TEST(pending_initially_empty)
{
    media_release_pending();
    ASSERT(!media_has_pending_image());

    const char *b64 = NULL;
    size_t len = 0;
    const char *tool_id = NULL;
    ASSERT(!media_get_pending_image(&b64, &len, &tool_id));
    return 0;
}

TEST(pending_inject_and_retrieve)
{
    media_release_pending();

    const char *test_b64 = "dGVzdA==";
    media_test_inject_image(test_b64, strlen(test_b64));
    media_set_pending_tool_id("toolu_123");

    ASSERT(media_has_pending_image());

    const char *b64 = NULL;
    size_t len = 0;
    const char *tool_id = NULL;
    ASSERT(media_get_pending_image(&b64, &len, &tool_id));
    ASSERT(b64 != NULL);
    ASSERT_STR_EQ(b64, "dGVzdA==");
    ASSERT(len == strlen("dGVzdA=="));
    ASSERT_STR_EQ(tool_id, "toolu_123");

    media_release_pending();
    ASSERT(!media_has_pending_image());
    return 0;
}

TEST(pending_release_clears_state)
{
    const char *test_b64 = "AAAA";
    media_test_inject_image(test_b64, 4);
    media_set_pending_tool_id("tool_456");
    ASSERT(media_has_pending_image());

    media_release_pending();
    ASSERT(!media_has_pending_image());

    const char *b64 = NULL;
    ASSERT(!media_get_pending_image(&b64, NULL, NULL));
    return 0;
}

TEST(pending_double_release_safe)
{
    media_release_pending();
    media_release_pending();
    ASSERT(!media_has_pending_image());
    return 0;
}

// --- JSON vision integration tests ---

TEST(json_anthropic_image_in_tool_result)
{
    mock_llm_set_backend(LLM_BACKEND_ANTHROPIC, "claude-test");

    // Set up pending image
    const char *test_b64 = "/9j/fake_jpeg_data";
    media_test_inject_image(test_b64, strlen(test_b64));
    media_set_pending_tool_id("toolu_photo_001");

    // Build history: assistant tool_use + user tool_result
    conversation_msg_t history[2];
    memset(history, 0, sizeof(history));

    // Tool use from assistant
    strncpy(history[0].role, "assistant", sizeof(history[0].role));
    strncpy(history[0].content, "{}", sizeof(history[0].content));
    history[0].is_tool_use = true;
    strncpy(history[0].tool_id, "toolu_photo_001", sizeof(history[0].tool_id));
    strncpy(history[0].tool_name, "capture_photo", sizeof(history[0].tool_name));

    // Tool result from user
    strncpy(history[1].role, "user", sizeof(history[1].role));
    strncpy(history[1].content, "Photo captured (1234 bytes JPEG)", sizeof(history[1].content));
    history[1].is_tool_result = true;
    strncpy(history[1].tool_id, "toolu_photo_001", sizeof(history[1].tool_id));

    char *json = json_build_request("test prompt", history, 2, NULL, NULL, 0);
    ASSERT(json != NULL);

    // Verify the JSON contains image block
    cJSON *root = cJSON_Parse(json);
    ASSERT(root != NULL);

    cJSON *messages = cJSON_GetObjectItem(root, "messages");
    ASSERT(messages != NULL);
    ASSERT(cJSON_GetArraySize(messages) == 2);

    // Second message should be tool_result with image
    cJSON *tr_msg = cJSON_GetArrayItem(messages, 1);
    cJSON *content = cJSON_GetObjectItem(tr_msg, "content");
    ASSERT(content != NULL && cJSON_IsArray(content));

    cJSON *tr_block = cJSON_GetArrayItem(content, 0);
    cJSON *tr_type = cJSON_GetObjectItem(tr_block, "type");
    ASSERT_STR_EQ(tr_type->valuestring, "tool_result");

    // tool_result should have multi-content with image + text
    cJSON *tr_content = cJSON_GetObjectItem(tr_block, "content");
    ASSERT(tr_content != NULL && cJSON_IsArray(tr_content));
    ASSERT(cJSON_GetArraySize(tr_content) == 2);

    // First content item: image
    cJSON *img_item = cJSON_GetArrayItem(tr_content, 0);
    cJSON *img_type = cJSON_GetObjectItem(img_item, "type");
    ASSERT_STR_EQ(img_type->valuestring, "image");

    cJSON *source = cJSON_GetObjectItem(img_item, "source");
    ASSERT(source != NULL);
    cJSON *src_type = cJSON_GetObjectItem(source, "type");
    ASSERT_STR_EQ(src_type->valuestring, "base64");
    cJSON *media_type = cJSON_GetObjectItem(source, "media_type");
    ASSERT_STR_EQ(media_type->valuestring, "image/jpeg");
    cJSON *data = cJSON_GetObjectItem(source, "data");
    ASSERT(data != NULL && cJSON_IsString(data));
    ASSERT_STR_EQ(data->valuestring, "/9j/fake_jpeg_data");

    // Second content item: text
    cJSON *text_item = cJSON_GetArrayItem(tr_content, 1);
    cJSON *text_type = cJSON_GetObjectItem(text_item, "type");
    ASSERT_STR_EQ(text_type->valuestring, "text");

    cJSON_Delete(root);
    free(json);
    media_release_pending();
    return 0;
}

TEST(json_openai_image_as_user_message)
{
    mock_llm_set_backend(LLM_BACKEND_OPENAI, "gpt-test");

    // Set up pending image
    const char *test_b64 = "/9j/fake_jpeg";
    media_test_inject_image(test_b64, strlen(test_b64));
    media_set_pending_tool_id("call_photo_002");

    // Build history: assistant tool_use + user tool_result
    conversation_msg_t history[2];
    memset(history, 0, sizeof(history));

    strncpy(history[0].role, "assistant", sizeof(history[0].role));
    strncpy(history[0].content, "{}", sizeof(history[0].content));
    history[0].is_tool_use = true;
    strncpy(history[0].tool_id, "call_photo_002", sizeof(history[0].tool_id));
    strncpy(history[0].tool_name, "capture_photo", sizeof(history[0].tool_name));

    strncpy(history[1].role, "user", sizeof(history[1].role));
    strncpy(history[1].content, "Photo captured", sizeof(history[1].content));
    history[1].is_tool_result = true;
    strncpy(history[1].tool_id, "call_photo_002", sizeof(history[1].tool_id));

    char *json = json_build_request("test prompt", history, 2, NULL, NULL, 0);
    ASSERT(json != NULL);

    cJSON *root = cJSON_Parse(json);
    ASSERT(root != NULL);

    cJSON *messages = cJSON_GetObjectItem(root, "messages");
    ASSERT(messages != NULL);

    // OpenAI: system + assistant(tool_calls) + tool(result) + user(vision)
    ASSERT(cJSON_GetArraySize(messages) == 4);

    // Last message should be the vision user message
    cJSON *vision_msg = cJSON_GetArrayItem(messages, 3);
    cJSON *role = cJSON_GetObjectItem(vision_msg, "role");
    ASSERT_STR_EQ(role->valuestring, "user");

    cJSON *v_content = cJSON_GetObjectItem(vision_msg, "content");
    ASSERT(v_content != NULL && cJSON_IsArray(v_content));
    ASSERT(cJSON_GetArraySize(v_content) == 2);

    // First item: image_url
    cJSON *img_item = cJSON_GetArrayItem(v_content, 0);
    cJSON *img_type = cJSON_GetObjectItem(img_item, "type");
    ASSERT_STR_EQ(img_type->valuestring, "image_url");

    cJSON *img_url_obj = cJSON_GetObjectItem(img_item, "image_url");
    ASSERT(img_url_obj != NULL);
    cJSON *url = cJSON_GetObjectItem(img_url_obj, "url");
    ASSERT(url != NULL && cJSON_IsString(url));
    // Should start with data:image/jpeg;base64,
    ASSERT(strstr(url->valuestring, "data:image/jpeg;base64,/9j/fake_jpeg") != NULL);

    cJSON_Delete(root);
    free(json);
    media_release_pending();
    return 0;
}

TEST(json_no_image_when_no_pending)
{
    mock_llm_set_backend(LLM_BACKEND_ANTHROPIC, "claude-test");
    media_release_pending();

    conversation_msg_t history[2];
    memset(history, 0, sizeof(history));

    strncpy(history[0].role, "assistant", sizeof(history[0].role));
    strncpy(history[0].content, "{}", sizeof(history[0].content));
    history[0].is_tool_use = true;
    strncpy(history[0].tool_id, "toolu_normal", sizeof(history[0].tool_id));
    strncpy(history[0].tool_name, "get_time", sizeof(history[0].tool_name));

    strncpy(history[1].role, "user", sizeof(history[1].role));
    strncpy(history[1].content, "Current time: 12:00", sizeof(history[1].content));
    history[1].is_tool_result = true;
    strncpy(history[1].tool_id, "toolu_normal", sizeof(history[1].tool_id));

    char *json = json_build_request("test prompt", history, 2, NULL, NULL, 0);
    ASSERT(json != NULL);

    // Should NOT contain any image block
    ASSERT(strstr(json, "image") == NULL);
    ASSERT(strstr(json, "base64") == NULL);

    free(json);
    return 0;
}

// --- Test runner ---

int test_tools_media_all(void)
{
    int failures = 0;

    printf("\nMedia Capture Tests:\n");

    printf("  base64_empty... ");
    if (test_base64_empty() == 0) printf("OK\n"); else failures++;

    printf("  base64_one_byte... ");
    if (test_base64_one_byte() == 0) printf("OK\n"); else failures++;

    printf("  base64_two_bytes... ");
    if (test_base64_two_bytes() == 0) printf("OK\n"); else failures++;

    printf("  base64_three_bytes... ");
    if (test_base64_three_bytes() == 0) printf("OK\n"); else failures++;

    printf("  base64_hello... ");
    if (test_base64_hello() == 0) printf("OK\n"); else failures++;

    printf("  pending_initially_empty... ");
    if (test_pending_initially_empty() == 0) printf("OK\n"); else failures++;

    printf("  pending_inject_and_retrieve... ");
    if (test_pending_inject_and_retrieve() == 0) printf("OK\n"); else failures++;

    printf("  pending_release_clears_state... ");
    if (test_pending_release_clears_state() == 0) printf("OK\n"); else failures++;

    printf("  pending_double_release_safe... ");
    if (test_pending_double_release_safe() == 0) printf("OK\n"); else failures++;

    printf("  json_anthropic_image_in_tool_result... ");
    if (test_json_anthropic_image_in_tool_result() == 0) printf("OK\n"); else failures++;

    printf("  json_openai_image_as_user_message... ");
    if (test_json_openai_image_as_user_message() == 0) printf("OK\n"); else failures++;

    printf("  json_no_image_when_no_pending... ");
    if (test_json_no_image_when_no_pending() == 0) printf("OK\n"); else failures++;

    return failures;
}
