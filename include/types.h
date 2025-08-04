#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

// HTTP response structure for curl operations
typedef struct {
    char *memory;
    size_t size;
} HTTPResponse;

// Individual chat message
typedef struct {
    char *role;
    char *content;
} ChatMessage;

// Array of chat messages
typedef struct {
    ChatMessage *messages;
    int count;
} MessageArray;

#endif
