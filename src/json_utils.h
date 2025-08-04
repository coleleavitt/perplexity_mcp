#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <cjson/cJSON.h>
#include "../include/types.h"

// Message parsing functions
MessageArray *parse_messages(cJSON *messages_json);
void free_message_array(MessageArray *msg_array);

// JSON-RPC response functions
void send_response(int id, const char *result, int error, const char *error_msg);

#endif
