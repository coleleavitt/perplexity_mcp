#include "mcp_protocol.h"
#include "json_utils.h"
#include "models/model_router.h"
#include "../include/constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>


// Handle initialize request
void handle_initialize(int id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(root, "id", id);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "protocolVersion", PROTOCOL_VERSION);

    // capabilities
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateObject();
    cJSON_AddBoolToObject(tools, "listChanged", 1);
    cJSON_AddItemToObject(capabilities, "tools", tools);
    cJSON_AddItemToObject(result, "capabilities", capabilities);

    // server info
    cJSON *server_info = cJSON_CreateObject();
    cJSON_AddStringToObject(server_info, "name", SERVER_NAME);
    cJSON_AddStringToObject(server_info, "version", SERVER_VERSION);
    cJSON_AddItemToObject(result, "serverInfo", server_info);

    cJSON_AddStringToObject(result, "instructions", "Perplexity MCP Server with intelligent model routing: Ask (fast), Research (smart async), Reason (detailed)");

    cJSON_AddItemToObject(root, "result", result);

    char *output = cJSON_Print(root);
    printf("%s\n", output);
    (void)fflush(stdout);

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
    cJSON_AddStringToObject(tool1, "description", "Fast search and Q&A using Sonar Pro model with real-time web search and citations");
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
    cJSON_AddStringToObject(tool2, "description", "Comprehensive research and analysis with intelligent routing - uses fast models for simple queries, deep research for complex topics (auto-detects complexity)");
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
    cJSON_AddStringToObject(tool3, "description", "Advanced reasoning and multi-step analysis using Sonar Reasoning Pro with chain-of-thought processing");
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

    // Tool: perplexity_deep_research (for forcing deep research)
    cJSON *tool4 = cJSON_CreateObject();
    cJSON_AddStringToObject(tool4, "name", "perplexity_deep_research");
    cJSON_AddStringToObject(tool4, "description", "Force deep research analysis using Sonar Deep Research model for comprehensive reports (2-5 minutes, use only for complex research tasks)");
    cJSON *input_schema4 = cJSON_CreateObject();
    cJSON_AddStringToObject(input_schema4, "type", "object");
    cJSON *props4 = cJSON_CreateObject();
    cJSON *msgs_prop4 = cJSON_CreateObject();
    cJSON_AddStringToObject(msgs_prop4, "type", "array");
    cJSON_AddItemToObject(props4, "messages", msgs_prop4);
    cJSON_AddItemToObject(input_schema4, "properties", props4);
    cJSON *required4 = cJSON_CreateArray();
    cJSON_AddItemToArray(required4, cJSON_CreateString("messages"));
    cJSON_AddItemToObject(input_schema4, "required", required4);
    cJSON_AddItemToObject(tool4, "inputSchema", input_schema4);
    cJSON_AddItemToArray(tools_arr, tool4);

    cJSON_AddItemToObject(result, "tools", tools_arr);
    cJSON_AddItemToObject(root, "result", result);

    char *output = cJSON_Print(root);
    printf("%s\n", output);
    (void)fflush(stdout);

    free(output);
    cJSON_Delete(root);
}

// Handle tools/call request
void handle_tools_call(int id, const char *tool_name, const cJSON *arguments) {
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

    int force_async = (strcmp(tool_name, "perplexity_deep_research") == 0) ? 1 : 0;
    char *result = route_and_execute(msg_array, tool_name, force_async);

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
        (void)fprintf(stderr, "Invalid JSON input\n");
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
