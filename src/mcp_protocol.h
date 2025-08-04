#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

#include <cjson/cJSON.h>

// MCP protocol handlers
void handle_initialize(int id);
void handle_tools_list(int id);
void handle_tools_call(int id, const char *tool_name, cJSON *arguments);

// Main request processor
void process_request(const char *line);

#endif
