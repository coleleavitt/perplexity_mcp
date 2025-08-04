#ifndef MODEL_ROUTER_H
#define MODEL_ROUTER_H

#include "../../include/types.h"

// Query complexity analysis
int is_complex_research_query(const char *content);

// Main routing function
char *route_and_execute(MessageArray *msg_array, const char *tool_name, int force_async);

#endif
