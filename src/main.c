#define GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "mcp_protocol.h"
#include "http_client.h"
#include "../include/constants.h"

int main() {
    // Initialize API key
    char *api_key = get_api_key();
    if (!api_key) {
        (void)fprintf(stderr, "Error: PERPLEXITY_API_KEY environment variable is required\n");
        return 1;
    }

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    (void)fprintf(stderr, "Perplexity MCP Server v%s with Intelligent Model Routing\n", SERVER_VERSION);
    (void)fprintf(stderr, "Tools: ask (fast), research (smart), reason (detailed), deep_research (forced)\n");

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
