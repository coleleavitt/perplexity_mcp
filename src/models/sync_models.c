#define _GNU_SOURCE  // Enable GNU extensions
#include "sync_models.h"
#include "../http_client.h"
#include "../../include/constants.h"
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Perform sync chat completion request
static char *perform_sync_chat_completion(MessageArray *msg_array, const char *model) {
    if (!msg_array || !model) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    HTTPResponse *response = init_http_response();

    // Build JSON payload
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", model);
    cJSON *messages_json = cJSON_CreateArray();

    for (int i = 0; i < msg_array->count; i++) {
        if (msg_array->messages[i].role && msg_array->messages[i].content) {
            cJSON *msg_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(msg_obj, "role", msg_array->messages[i].role);
            cJSON_AddStringToObject(msg_obj, "content", msg_array->messages[i].content);
            cJSON_AddItemToArray(messages_json, msg_obj);
        }
    }
    cJSON_AddItemToObject(root, "messages", messages_json);

    char *data = cJSON_Print(root);

    struct curl_slist *headers = NULL;
    char auth_header[1024];
    (void)snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", get_api_key());

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    char *answer = NULL;
    if (res != CURLE_OK) {
        (void)fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200) {
            cJSON *json_res = cJSON_Parse(response->memory);
            if (json_res) {
                cJSON *choices = cJSON_GetObjectItem(json_res, "choices");
                if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                    cJSON *first = cJSON_GetArrayItem(choices, 0);
                    cJSON *msg = cJSON_GetObjectItem(first, "message");
                    cJSON *content = cJSON_GetObjectItem(msg, "content");
                    if (cJSON_IsString(content)) {
                        answer = strdup(content->valuestring);
                    }
                }
                cJSON_Delete(json_res);
            }
        } else {
            (void)fprintf(stderr, "HTTP response code: %ld\n", http_code);
            if (response->memory) {
                (void)fprintf(stderr, "Response body: %s\n", response->memory);
            }
        }
    }

    free(data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    cJSON_Delete(root);
    free_http_response(response);

    return answer;
}

char *execute_sonar_pro(MessageArray *msg_array) {
    return perform_sync_chat_completion(msg_array, "sonar-pro");
}

char *execute_sonar_reasoning_pro(MessageArray *msg_array) {
    return perform_sync_chat_completion(msg_array, "sonar-reasoning-pro");
}
