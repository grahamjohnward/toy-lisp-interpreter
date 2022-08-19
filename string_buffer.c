#include "string_buffer.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void string_buffer_append(struct string_buffer *sb, char *string)
{
    struct string_buffer_link *link = malloc(sizeof(struct string_buffer_link));
    /* Update links */
    link->next = NULL;
    if (sb->tail)
        sb->tail->next = link;
    sb->tail = link;
    if (!sb->head)
        sb->head = link;
    /* Update lengths */
    link->len = strlen(string);
    sb->len += link->len;
    /* Copy string */
    link->string = malloc(link->len + 1);
    strcpy(link->string, string);
}

void string_buffer_init(struct string_buffer *sb)
{
    sb->head = NULL;
    sb->tail = NULL;
    sb->len = 0;
}

/* Caller is responsible for freeing returned memory */
char *string_buffer_to_string(struct string_buffer *sb)
{
    char *result = malloc(sb->len + 1);
    char *cur = result;
    for (struct string_buffer_link *link = sb->head; link; link = link->next) {
        strcpy(cur, link->string);
        cur += link->len;
    }
    return result;
}

static void string_buffer_free_link(struct string_buffer_link *link)
{
    if (link->next)
        string_buffer_free_link(link->next);
    free(link->string);
    free(link);
}

void string_buffer_free_links(struct string_buffer *sb)
{
    if (sb->head)
        string_buffer_free_link(sb->head);
}
