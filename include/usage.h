#ifndef USAGE_H
#define USAGE_H

// Usage tracking structure
typedef struct {
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
    int citation_tokens;        // Deep Research only
    int num_search_queries;     // Deep Research only
    int reasoning_tokens;       // Deep Research only
    char *search_context_size;  // "low", "medium", "high"
} UsageInfo;

// Cost calculation structure
typedef struct {
    double input_cost;
    double output_cost;
    double citation_cost;
    double search_cost;
    double reasoning_cost;
    double total_cost;
} CostInfo;

// Function declarations
UsageInfo *parse_usage_from_response(const char *response_json);
CostInfo *calculate_cost(UsageInfo *usage, const char *model);
void log_usage_and_cost(const char *model, const UsageInfo *usage, const CostInfo *cost);
void free_usage_info(UsageInfo *usage);
void free_cost_info(CostInfo *cost);

#endif
