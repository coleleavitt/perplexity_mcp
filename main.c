#define _GNU_SOURCE  // Enable strdup and other GNU extensions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 65536
#define MAX_LINE_SIZE 8192
#define API_URL "https://api.perplexity.ai/chat/completions"

typedef struct {
    char *memory;
    size_t size;
} HTTPResponse;

typedef struct {
    char *role;
    char *content;
} ChatMessage;

typedef struct {
    ChatMessage *messages;
    int count;
} MessageArray;

// Global API key
static char *perplexity_api_key = NULL;

// HTTP response callback
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    HTTPResponse *response = (HTTPResponse *)userp;

    char *ptr = realloc(response->memory, response->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, realsize);
    response->size += realsize;
    response->memory[response->size] = 0;

    return realsize;
}

// Initialize HTTP response structure
HTTPResponse *init_http_response() {
    HTTPResponse *response = (HTTPResponse *)malloc(sizeof(HTTPResponse));
    response->memory = (char *)malloc(1); // start with empty
    response->size = 0;
    return response;
}

// Free HTTP response
void free_http_response(HTTPResponse *response) {
    if (response) {
        if (response->memory) free(response->memory);
        free(response);
    }
}

// Free messages
void free_message_array(MessageArray *msg_array) {
    if (!msg_array) return;
    for (int i = 0; i < msg_array->count; i++) {
        if (msg_array->messages[i].role) free(msg_array->messages[i].role);
        if (msg_array->messages[i].content) free(msg_array->messages[i].content);
    }
    free(msg_array->messages);
    free(msg_array);
}

// Parse messages array from JSON
MessageArray *parse_messages(cJSON *messages_json) {
    if (!cJSON_IsArray(messages_json)) return NULL;

    int count = cJSON_GetArraySize(messages_json);
    MessageArray *msg_array = (MessageArray *)malloc(sizeof(MessageArray));
    msg_array->messages = (ChatMessage *)malloc(sizeof(ChatMessage) * count);
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

// Perform chat completion request
char *perform_chat_completion(MessageArray *msg_array, const char *model) {
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
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", perplexity_api_key);

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);        // 30 second timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 10 second connect timeout

    CURLcode res = curl_easy_perform(curl);

    char *answer = NULL;
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
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
            fprintf(stderr, "HTTP response code: %ld\n", http_code);
            if (response->memory) {
                fprintf(stderr, "Response body: %s\n", response->memory);
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

// Send JSON-RPC formatted response
void send_response(int id, const char *result, int error, const char *error_msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);

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
        cJSON_AddNumberToObject(error_obj, "code", -32603);
        cJSON_AddStringToObject(error_obj, "message", error_msg);
        cJSON_AddItemToObject(root, "error", error_obj);
    }

    char *output = cJSON_Print(root);
    printf("%s\n", output);
    fflush(stdout);

    free(output);
    cJSON_Delete(root);
}

// Handle initialize request
void handle_initialize(int id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "protocolVersion", "2025-06-18");

    // capabilities
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateObject();
    cJSON_AddBoolToObject(tools, "listChanged", 1);
    cJSON_AddItemToObject(capabilities, "tools", tools);
    cJSON_AddItemToObject(result, "capabilities", capabilities);

    // server info
    cJSON *server_info = cJSON_CreateObject();
    cJSON_AddStringToObject(server_info, "name", "perplexity-mcp-server");
    cJSON_AddStringToObject(server_info, "version", "0.2.0");
    cJSON_AddItemToObject(result, "serverInfo", server_info);

    cJSON_AddStringToObject(result, "instructions", "Perplexity MCP Server with Ask, Research, and Reason tools");

    cJSON_AddItemToObject(root, "result", result);

    char *output = cJSON_Print(root);
    printf("%s\n", output);
    fflush(stdout);

    free(output);
    cJSON_Delete(root);
}

// Handle tools/list request
void handle_tools_list(int id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);

    cJSON *result = cJSON_CreateObject();
    cJSON *tools_arr = cJSON_CreateArray();

    // Tool: perplexity_ask
    cJSON *tool1 = cJSON_CreateObject();
    cJSON_AddStringToObject(tool1, "name", "perplexity_ask");
    cJSON_AddStringToObject(tool1, "description", "Chat with Perplexity using the Sonar Pro model for enhanced search");
    cJSON *input_schema1 = cJSON_CreateObject();
    cJSON_AddStringToObject(input_schema1, "type", "object");
    cJSON *props1 = cJSON_CreateObject();
    cJSON *msgs_prop1 = cJSON_CreateObject();
    cJSON_AddStringToObject(msgs_prop1, "type", "array");
    cJSON_AddItemToObject(props1, "messages", msgs_prop1);
    cJSON_AddItemToObject(input_schema1, "properties", props1);
    cJSON *required1 = cJSON_CreateArray();
    cJSON_AddItemToArray(required1, cJSON_CreateString("messages"));
    cJSON_AddItemToObject(input_schema1, "required", required1);
    cJSON_AddItemToObject(tool1, "inputSchema", input_schema1);
    cJSON_AddItemToArray(tools_arr, tool1);

    // Tool: perplexity_research
    cJSON *tool2 = cJSON_CreateObject();
    cJSON_AddStringToObject(tool2, "name", "perplexity_research");
    cJSON_AddStringToObject(tool2, "description", "Perform deep research using Sonar Deep Research model for comprehensive analysis");
    cJSON *input_schema2 = cJSON_CreateObject();
    cJSON_AddStringToObject(input_schema2, "type", "object");
    cJSON *props2 = cJSON_CreateObject();
    cJSON *msgs_prop2 = cJSON_CreateObject();
    cJSON_AddStringToObject(msgs_prop2, "type", "array");
    cJSON_AddItemToObject(props2, "messages", msgs_prop2);
    cJSON_AddItemToObject(input_schema2, "properties", props2);
    cJSON *required2 = cJSON_CreateArray();
    cJSON_AddItemToArray(required2, cJSON_CreateString("messages"));
    cJSON_AddItemToObject(input_schema2, "required", required2);
    cJSON_AddItemToObject(tool2, "inputSchema", input_schema2);
    cJSON_AddItemToArray(tools_arr, tool2);

    // Tool: perplexity_reason
    cJSON *tool3 = cJSON_CreateObject();
    cJSON_AddStringToObject(tool3, "name", "perplexity_reason");
    cJSON_AddStringToObject(tool3, "description", "Use Sonar Reasoning Pro model for complex reasoning and analysis tasks");
    cJSON *input_schema3 = cJSON_CreateObject();
    cJSON_AddStringToObject(input_schema3, "type", "object");
    cJSON *props3 = cJSON_CreateObject();
    cJSON *msgs_prop3 = cJSON_CreateObject();
    cJSON_AddStringToObject(msgs_prop3, "type", "array");
    cJSON_AddItemToObject(props3, "messages", msgs_prop3);
    cJSON_AddItemToObject(input_schema3, "properties", props3);
    cJSON *required3 = cJSON_CreateArray();
    cJSON_AddItemToArray(required3, cJSON_CreateString("messages"));
    cJSON_AddItemToObject(input_schema3, "required", required3);
    cJSON_AddItemToObject(tool3, "inputSchema", input_schema3);
    cJSON_AddItemToArray(tools_arr, tool3);

    cJSON_AddItemToObject(result, "tools", tools_arr);
    cJSON_AddItemToObject(root, "result", result);

    char *output = cJSON_Print(root);
    printf("%s\n", output);
    fflush(stdout);

    free(output);
    cJSON_Delete(root);
}

// Handle tools/call request
void handle_tools_call(int id, const char *tool_name, cJSON *arguments) {
    cJSON *messages_json = cJSON_GetObjectItem(arguments, "messages");
    if (!cJSON_IsArray(messages_json)) {
        send_response(id, NULL, 1, "Missing or invalid 'messages' parameter");
        return;
    }

    MessageArray *msg_array = parse_messages(messages_json);
    if (!msg_array) {
        send_response(id, NULL, 1, "Failed to parse messages");
        return;
    }

    char *result = NULL;

    // Use correct model names from API documentation
    if (strcmp(tool_name, "perplexity_ask") == 0) {
        result = perform_chat_completion(msg_array, "sonar-pro");
    } else if (strcmp(tool_name, "perplexity_research") == 0) {
        result = perform_chat_completion(msg_array, "sonar-deep-research");
    } else if (strcmp(tool_name, "perplexity_reason") == 0) {
        result = perform_chat_completion(msg_array, "sonar-reasoning-pro");
    } else {
        free_message_array(msg_array);
        send_response(id, NULL, 1, "Unknown tool");
        return;
    }

    free_message_array(msg_array);

    if (result) {
        send_response(id, result, 0, NULL);
        free(result);
    } else {
        send_response(id, NULL, 1, "Failed to get response from Perplexity API");
    }
}

// Main dispatcher
void process_request(const char *line) {
    cJSON *json = cJSON_Parse(line);
    if (!json) {
        fprintf(stderr, "Invalid JSON input\n");
        return;
    }

    cJSON *method = cJSON_GetObjectItem(json, "method");
    cJSON *id = cJSON_GetObjectItem(json, "id");
    cJSON *params = cJSON_GetObjectItem(json, "params");

    if (!cJSON_IsString(method) || !cJSON_IsNumber(id)) {
        cJSON_Delete(json);
        return;
    }

    int req_id = (int)id->valuedouble;

    if (strcmp(method->valuestring, "initialize") == 0) {
        handle_initialize(req_id);
    } else if (strcmp(method->valuestring, "tools/list") == 0) {
        handle_tools_list(req_id);
    } else if (strcmp(method->valuestring, "tools/call") == 0) {
        if (!cJSON_IsObject(params)) {
            send_response(req_id, NULL, 1, "Missing parameters");
            cJSON_Delete(json);
            return;
        }

        cJSON *tool_name = cJSON_GetObjectItem(params, "name");
        cJSON *arguments = cJSON_GetObjectItem(params, "arguments");
        if (!cJSON_IsString(tool_name) || !cJSON_IsObject(arguments)) {
            send_response(req_id, NULL, 1, "Invalid tool call parameters");
            cJSON_Delete(json);
            return;
        }

        handle_tools_call(req_id, tool_name->valuestring, arguments);
    } else {
        send_response(req_id, NULL, 1, "Unknown method");
    }

    cJSON_Delete(json);
}

int main() {
    perplexity_api_key = getenv("PERPLEXITY_API_KEY");
    if (!perplexity_api_key) {
        fprintf(stderr, "Error: PERPLEXITY_API_KEY environment variable is required\n");
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    fprintf(stderr, "Perplexity MCP Server v0.2.0 running with Ask, Research, and Reason tools\n");

    char buffer[MAX_LINE_SIZE];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        // Remove newline character
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }

        if (strlen(buffer) > 0) {
            process_request(buffer);
        }
    }

    curl_global_cleanup();
    return 0;
}
