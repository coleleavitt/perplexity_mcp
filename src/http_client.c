#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/types.h"  // For HTTPResponse

// Global API key
static char *perplexity_api_key = NULL;

// HTTP response callback
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    HTTPResponse *response = (HTTPResponse *)userp;

    char *ptr = realloc(response->memory, response->size + realsize + 1);
    if (!ptr) {
        (void)fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, realsize);
    response->size += realsize;
    response->memory[response->size] = 0;

    return realsize;
}

// Initialize HTTP response structure
HTTPResponse *init_http_response(void) {
    HTTPResponse *response = (HTTPResponse *)malloc(sizeof(HTTPResponse));
    response->memory = (char *)malloc(1);
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

// Get API key (to be called from main)
char *get_api_key(void) {
    if (!perplexity_api_key) {
        perplexity_api_key = getenv("PERPLEXITY_API_KEY");
    }
    return perplexity_api_key;
}
