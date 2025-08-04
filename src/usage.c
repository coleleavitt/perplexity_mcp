#define _GNU_SOURCE  // Add this line at the very top
#include "../include/usage.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pricing constants (per 1M tokens, except search queries per 1K)
static const double PRICING_TABLE[][5] = {
    // [input, output, citation, search_per_1k, reasoning]
    [0] = {1.0, 1.0, 0.0, 5.0, 0.0},    // sonar
    [1] = {3.0, 15.0, 0.0, 5.0, 0.0},   // sonar-pro
    [2] = {1.0, 5.0, 0.0, 5.0, 0.0},    // sonar-reasoning
    [3] = {2.0, 8.0, 0.0, 5.0, 0.0},    // sonar-reasoning-pro
    [4] = {2.0, 8.0, 2.0, 5.0, 3.0}     // sonar-deep-research
};

static int get_model_index(const char *model) {
    if (strcmp(model, "sonar") == 0) return 0;
    if (strcmp(model, "sonar-pro") == 0) return 1;
    if (strcmp(model, "sonar-reasoning") == 0) return 2;
    if (strcmp(model, "sonar-reasoning-pro") == 0) return 3;
    if (strcmp(model, "sonar-deep-research") == 0) return 4;
    return -1; // Unknown model
}

UsageInfo *parse_usage_from_response(const char *response_json) {
    cJSON *json = cJSON_Parse(response_json);
    if (!json) return NULL;

    cJSON *usage = cJSON_GetObjectItem(json, "usage");
    if (!usage || !cJSON_IsObject(usage)) {
        cJSON_Delete(json);
        return NULL;
    }

    UsageInfo *info = calloc(1, sizeof(UsageInfo));

    // Parse standard fields
    cJSON *prompt = cJSON_GetObjectItem(usage, "prompt_tokens");
    if (cJSON_IsNumber(prompt)) info->prompt_tokens = (int)prompt->valuedouble;

    cJSON *completion = cJSON_GetObjectItem(usage, "completion_tokens");
    if (cJSON_IsNumber(completion)) info->completion_tokens = (int)completion->valuedouble;

    cJSON *total = cJSON_GetObjectItem(usage, "total_tokens");
    if (cJSON_IsNumber(total)) info->total_tokens = (int)total->valuedouble;

    // Parse deep research specific fields
    cJSON *citation = cJSON_GetObjectItem(usage, "citation_tokens");
    if (cJSON_IsNumber(citation)) info->citation_tokens = (int)citation->valuedouble;

    cJSON *search_queries = cJSON_GetObjectItem(usage, "num_search_queries");
    if (cJSON_IsNumber(search_queries)) info->num_search_queries = (int)search_queries->valuedouble;

    cJSON *reasoning = cJSON_GetObjectItem(usage, "reasoning_tokens");
    if (cJSON_IsNumber(reasoning)) info->reasoning_tokens = (int)reasoning->valuedouble;

    // Parse search context size
    cJSON *search_context = cJSON_GetObjectItem(usage, "search_context_size");
    if (cJSON_IsString(search_context)) {
        info->search_context_size = strdup(search_context->valuestring);
    }

    cJSON_Delete(json);
    return info;
}

CostInfo *calculate_cost(UsageInfo *usage, const char *model) {
    if (!usage || !model) return NULL;

    int model_idx = get_model_index(model);
    if (model_idx < 0) return NULL;

    CostInfo *cost = calloc(1, sizeof(CostInfo));

    // Calculate costs based on pricing table
    cost->input_cost = (usage->prompt_tokens / 1000000.0) * PRICING_TABLE[model_idx][0];
    cost->output_cost = (usage->completion_tokens / 1000000.0) * PRICING_TABLE[model_idx][1];
    cost->citation_cost = (usage->citation_tokens / 1000000.0) * PRICING_TABLE[model_idx][2];
    cost->reasoning_cost = (usage->reasoning_tokens / 1000000.0) * PRICING_TABLE[model_idx][4];

    // Search queries cost (per 1K requests)
    cost->search_cost = (usage->num_search_queries / 1000.0) * PRICING_TABLE[model_idx][3];

    // Add base search context fee for sonar models
    if (model_idx <= 3) { // sonar, sonar-pro, sonar-reasoning, sonar-reasoning-pro
        cost->search_cost += 0.005; // $5 per 1K = $0.005 per request
    }

    cost->total_cost = cost->input_cost + cost->output_cost + cost->citation_cost +
                      cost->reasoning_cost + cost->search_cost;

    return cost;
}

void log_usage_and_cost(const char *model, UsageInfo *usage, CostInfo *cost) {
    fprintf(stderr, "=== Usage & Cost Report ===\n");
    fprintf(stderr, "Model: %s\n", model);
    fprintf(stderr, "Tokens - Input: %d, Output: %d, Total: %d\n",
            usage->prompt_tokens, usage->completion_tokens, usage->total_tokens);

    if (usage->citation_tokens > 0) {
        fprintf(stderr, "Citation tokens: %d\n", usage->citation_tokens);
    }
    if (usage->reasoning_tokens > 0) {
        fprintf(stderr, "Reasoning tokens: %d\n", usage->reasoning_tokens);
    }
    if (usage->num_search_queries > 0) {
        fprintf(stderr, "Search queries: %d\n", usage->num_search_queries);
    }
    if (usage->search_context_size) {
        fprintf(stderr, "Search context: %s\n", usage->search_context_size);
    }

    fprintf(stderr, "Costs - Input: $%.6f, Output: $%.6f", cost->input_cost, cost->output_cost);
    if (cost->citation_cost > 0) fprintf(stderr, ", Citation: $%.6f", cost->citation_cost);
    if (cost->reasoning_cost > 0) fprintf(stderr, ", Reasoning: $%.6f", cost->reasoning_cost);
    if (cost->search_cost > 0) fprintf(stderr, ", Search: $%.6f", cost->search_cost);
    fprintf(stderr, "\nTotal Cost: $%.6f\n", cost->total_cost);
    fprintf(stderr, "========================\n");
}

void free_usage_info(UsageInfo *usage) {
    if (usage) {
        if (usage->search_context_size) free(usage->search_context_size);
        free(usage);
    }
}

void free_cost_info(CostInfo *cost) {
    if (cost) free(cost);
}
