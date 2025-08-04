#define _GNU_SOURCE  // Enable GNU extensions
#include "model_router.h"
#include "sync_models.h"
#include "async_models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Analyze query complexity to determine if deep research is needed
int is_complex_research_query(const char *content) {
    if (!content) return 0;

    // Convert to lowercase for analysis
    char *lower_content = strdup(content);
    for (char *p = lower_content; *p; p++) {
        *p = tolower(*p);
    }

    // Simple patterns that indicate basic questions (don't need deep research)
    const char *simple_patterns[] = {
        "how many", "what is", "calculate", "solve", "math", "arithmetic",
        "add", "subtract", "multiply", "divide", "plus", "minus",
        "apples", "oranges", "basic", "simple", "quick question",
        NULL
    };

    // Complex patterns that indicate research-worthy queries
    const char *complex_patterns[] = {
        "analysis", "comprehensive", "report", "industry", "market",
        "economic", "policy", "framework", "strategy", "trends",
        "future", "projection", "forecast", "impact", "implications",
        "comparison", "evaluate", "assess", "study", "research",
        "technological", "regulatory", "commercial viability",
        "net-zero", "carbon emissions", "heavy industry",
        NULL
    };

    int simple_score = 0;
    int complex_score = 0;
    int content_length = strlen(content);

    // Check for simple patterns
    for (int i = 0; simple_patterns[i]; i++) {
        if (strstr(lower_content, simple_patterns[i])) {
            simple_score += 2;
        }
    }

    // Check for complex patterns
    for (int i = 0; complex_patterns[i]; i++) {
        if (strstr(lower_content, complex_patterns[i])) {
            complex_score += 3;
        }
    }

    // Length-based scoring (longer queries often need more research)
    if (content_length > 200) complex_score += 2;
    if (content_length > 500) complex_score += 3;

    free(lower_content);

    // Return 1 if complex research is needed, 0 for simple queries
    return (complex_score > simple_score + 1);
}

// Main routing function
char *route_and_execute(MessageArray *msg_array, const char *tool_name, int force_async) {
    if (!msg_array || !tool_name) return NULL;

    if (strcmp(tool_name, "perplexity_ask") == 0) {
        return execute_sonar_pro(msg_array);
    } else if (strcmp(tool_name, "perplexity_research") == 0) {
        (void)fprintf(stderr, "Starting intelligent research analysis...\n");

        // Check if query needs deep research (unless forced)
        if (!force_async) {
            char *last_user_content = NULL;
            for (int i = msg_array->count - 1; i >= 0; i--) {
                if (msg_array->messages[i].role && msg_array->messages[i].content &&
                    strcmp(msg_array->messages[i].role, "user") == 0) {
                    last_user_content = msg_array->messages[i].content;
                    break;
                }
            }

            if (last_user_content && !is_complex_research_query(last_user_content)) {
                (void)fprintf(stderr, "Query appears simple, using sonar-pro instead of deep research\n");
                return execute_sonar_pro(msg_array);
            }
        }

        return execute_sonar_deep_research(msg_array);
    } else if (strcmp(tool_name, "perplexity_reason") == 0) {
        return execute_sonar_reasoning_pro(msg_array);
    } else if (strcmp(tool_name, "perplexity_deep_research") == 0) {
        (void)fprintf(stderr, "Starting forced deep research analysis...\n");
        return execute_sonar_deep_research(msg_array);
    }

    return NULL;
}
