#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "../include/types.h"

// HTTP client functions
HTTPResponse *init_http_response(void);
void free_http_response(HTTPResponse *response);
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

// Get global API key
extern char *get_api_key(void);

#endif
