#ifndef STRING_BUFFER_H
#define STRING_BUFFER_H

#include <stddef.h>

struct string_buffer_link {
    char *string;
    struct string_buffer_link *next;
    size_t len;
};

struct string_buffer {
    struct string_buffer_link *head;
    struct string_buffer_link *tail;
    size_t len;
};

void string_buffer_append(struct string_buffer *sb, char *string);

void string_buffer_init(struct string_buffer *sb);

/* Caller is responsible for freeing returned memory */
char *string_buffer_to_string(struct string_buffer *sb);

void string_buffer_free_links(struct string_buffer *sb);

#endif
