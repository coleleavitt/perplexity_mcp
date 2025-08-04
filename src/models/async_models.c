#define _GNU_SOURCE  // Enable GNU extensions
#include "async_models.h"
#include "../http_client.h"
#include "../include/usage.h"  // Add this include
#include "../../include/constants.h"
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Submit async request
static char *submit_async_request(MessageArray *msg_array, const char *model) {
    if (!msg_array || !model) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    HTTPResponse *response = init_http_response();

    // Build nested JSON payload for async API
    cJSON *root = cJSON_CreateObject();
    cJSON *request_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(request_obj, "model", model);

    // Add reasoning_effort for deep research model
    if (strcmp(model, "sonar-deep-research") == 0) {
        cJSON_AddStringToObject(request_obj, "reasoning_effort", "medium");
    }

    cJSON *messages_json = cJSON_CreateArray();
    for (int i = 0; i < msg_array->count; i++) {
        if (msg_array->messages[i].role && msg_array->messages[i].content) {
            cJSON *msg_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(msg_obj, "role", msg_array->messages[i].role);
            cJSON_AddStringToObject(msg_obj, "content", msg_array->messages[i].content);
            cJSON_AddItemToArray(messages_json, msg_obj);
        }
    }
    cJSON_AddItemToObject(request_obj, "messages", messages_json);
    cJSON_AddItemToObject(root, "request", request_obj);

    char *data = cJSON_Print(root);

    struct curl_slist *headers = NULL;
    char auth_header[1024];
    (void)snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", get_api_key());

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, ASYNC_API_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    char *request_id = NULL;

    if (res == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200) {
            cJSON *json_res = cJSON_Parse(response->memory);
            if (json_res) {
                cJSON *id = cJSON_GetObjectItem(json_res, "id");
                if (cJSON_IsString(id)) {
                    request_id = strdup(id->valuestring);
                }
                cJSON_Delete(json_res);
            }
        } else {
            (void)fprintf(stderr, "HTTP response code: %ld\n", http_code);
            if (response->memory) {
                (void)fprintf(stderr, "Response body: %s\n", response->memory);
            }
        }
    } else {
        (void)fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    free(data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    cJSON_Delete(root);
    free_http_response(response);

    return request_id;
}

// Check async request status and get result
static char *get_async_result(const char *request_id) {
    if (!request_id) return NULL;

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    HTTPResponse *response = init_http_response();
    char url[512];
    (void)snprintf(url, sizeof(url), "%s/%s", ASYNC_API_URL, request_id);

    struct curl_slist *headers = NULL;
    char auth_header[1024];
    (void)snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", get_api_key());
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    char *result = NULL;

    if (res == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200) {
            cJSON *json_res = cJSON_Parse(response->memory);
            if (json_res) {
                cJSON *status = cJSON_GetObjectItem(json_res, "status");
                if (cJSON_IsString(status)) {
                    if (strcmp(status->valuestring, "COMPLETED") == 0) {
                        cJSON *response_obj = cJSON_GetObjectItem(json_res, "response");
                        if (response_obj) {
                            cJSON *choices = cJSON_GetObjectItem(response_obj, "choices");
                            if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                                cJSON *first = cJSON_GetArrayItem(choices, 0);
                                cJSON *msg = cJSON_GetObjectItem(first, "message");
                                cJSON *content = cJSON_GetObjectItem(msg, "content");
                                if (cJSON_IsString(content)) {
                                    result = strdup(content->valuestring);
                                }
                            }

                            // NEW: Parse and log usage/cost for completed async requests
                            UsageInfo *usage = parse_usage_from_response(response->memory);
                            if (usage) {
                                CostInfo *cost = calculate_cost(usage, "sonar-deep-research");
                                if (cost) {
                                    log_usage_and_cost("sonar-deep-research", usage, cost);
                                    free_cost_info(cost);
                                }
                                free_usage_info(usage);
                            }
                        }
                    } else if (strcmp(status->valuestring, "FAILED") == 0) {
                        cJSON *error_msg = cJSON_GetObjectItem(json_res, "error_message");
                        if (cJSON_IsString(error_msg)) {
                            result = strdup(error_msg->valuestring);
                        } else {
                            result = strdup("Request failed with unknown error");
                        }
                    }
                    // For IN_PROGRESS or CREATED, return NULL to continue polling
                }
                cJSON_Delete(json_res);
            }
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free_http_response(response);

    return result;
}

char *execute_sonar_deep_research(MessageArray *msg_array) {
    char *request_id = submit_async_request(msg_array, "sonar-deep-research");
    if (!request_id) {
        return NULL;
    }

    (void)fprintf(stderr, "Submitted async research request: %s\n", request_id);

    // Reduced polling for better user experience
    int poll_interval = 3; // Start with 3 seconds
    int max_polls = 40;    // Maximum 40 polls (about 3-4 minutes)

    for (int i = 0; i < max_polls; i++) {
        sleep(poll_interval);
        char *result = get_async_result(request_id);

        if (result) {
            free(request_id);
            return result;
        }

        // Exponential backoff up to 8 seconds
        if (poll_interval < 8) {
            poll_interval = (poll_interval * 4) / 3; // 3, 4, 5, 6, 8, 8...
            if (poll_interval > 8) poll_interval = 8;
        }

        (void)fprintf(stderr, "Waiting for research completion... (%d/%d)\n", i + 1, max_polls);
    }

    free(request_id);
    return strdup("Research request timed out. Try using perplexity_ask for simpler questions.");
}
