#define GNU_SOURCE
#include "json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "../include/types.h"  // For MessageArray, ChatMessage

// Constants for magic numbers
#define JSONRPC_INTERNAL_ERROR (-32603)

// Parse messages array from JSON
MessageArray *parse_messages(const cJSON *messages_json) {
    if (!cJSON_IsArray(messages_json)) {
        return NULL;
    }

    int count = cJSON_GetArraySize(messages_json);
    MessageArray *msg_array = (MessageArray *)malloc(sizeof(MessageArray));

    // FIX: Use calloc instead of malloc to initialize memory to zero
    // This prevents "garbage value" warnings when accessing uninitialized fields
    msg_array->messages = (ChatMessage *)calloc((size_t)count, sizeof(ChatMessage));
    msg_array->count = count;

    for (int i = 0; i < count; i++) {
        cJSON *msg = cJSON_GetArrayItem(messages_json, i);
        cJSON *role = cJSON_GetObjectItem(msg, "role");
        cJSON *content = cJSON_GetObjectItem(msg, "content");

        if (!cJSON_IsString(role) || !cJSON_IsString(content)) {
            msg_array->messages[i].role = NULL;
            msg_array->messages[i].content = NULL;
        } else {
            msg_array->messages[i].role = strdup(role->valuestring);
            msg_array->messages[i].content = strdup(content->valuestring);
        }
    }

    return msg_array;
}

// Free messages
void free_message_array(MessageArray *msg_array) {
    if (!msg_array) {
        return;
    }

    for (int i = 0; i < msg_array->count; i++) {
        if (msg_array->messages[i].role) {
            free(msg_array->messages[i].role);
        }
        if (msg_array->messages[i].content) {
            free(msg_array->messages[i].content);
        }
    }
    free(msg_array->messages);
    free(msg_array);
}

// Send JSON-RPC formatted response
void send_response(int request_id, const char *result, int error, const char *error_msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", request_id);

    if (!error) {
        cJSON *result_obj = cJSON_CreateObject();
        cJSON *content_array = cJSON_CreateArray();
        cJSON *content_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(content_obj, "type", "text");
        cJSON_AddStringToObject(content_obj, "text", result);
        cJSON_AddItemToArray(content_array, content_obj);
        cJSON_AddItemToObject(result_obj, "content", content_array);
        cJSON_AddBoolToObject(result_obj, "isError", 0);
        cJSON_AddItemToObject(root, "result", result_obj);
    } else {
        cJSON *error_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(error_obj, "code", JSONRPC_INTERNAL_ERROR);
        cJSON_AddStringToObject(error_obj, "message", error_msg);
        cJSON_AddItemToObject(root, "error", error_obj);
    }

    char *output = cJSON_Print(root);
    printf("%s\n", output);
    (void)fflush(stdout);  // Fixed: removed redundant void cast

    free(output);
    cJSON_Delete(root);
}
