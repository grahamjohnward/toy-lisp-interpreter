#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "text_stream.h"

static void do_read(struct text_stream *stream);

#define BUFFER_SIZE 1024

void text_stream_init_str(struct text_stream *stream, char *input_string)
{
    stream->type = TEXT_STREAM_TYPE_STRING;
    stream->buf = input_string;
    stream->cursor = input_string;
    stream->fd = -1;
    stream->last_read_len = -1;
    stream->eof = 0;
}

void text_stream_init_fd(struct text_stream *stream, int fd)
{
    stream->type = TEXT_STREAM_TYPE_FD;
    stream->buf = malloc(BUFFER_SIZE);
    stream->cursor = stream->buf;
    stream->fd = fd;
    stream->last_read_len = 0;
    stream->eof = 0;
}

void text_stream_free(struct text_stream *stream)
{
    if (stream->type == TEXT_STREAM_TYPE_FD)
        free(stream->buf);
}

char text_stream_peek(struct text_stream *stream)
{
    assert(!text_stream_eof(stream));
    switch (stream->type) {
    case TEXT_STREAM_TYPE_FD:
        if (stream->cursor - stream->buf == stream->last_read_len)
            do_read(stream);
    case TEXT_STREAM_TYPE_STRING:
        return *stream->cursor;
    default:
        abort();
    }
}

void text_stream_advance(struct text_stream *stream)
{
    stream->cursor++;
}

int text_stream_eof(struct text_stream *stream)
{
    switch (stream->type) {
    case TEXT_STREAM_TYPE_STRING:
        return !*stream->cursor;
    case TEXT_STREAM_TYPE_FD:
        return stream->eof;
    default:
        abort();
    }
}

static void do_read(struct text_stream *stream)
{
    if (stream->type != TEXT_STREAM_TYPE_FD)
        abort();
    int bytes_read = read(stream->fd, stream->buf, BUFFER_SIZE);
    if (bytes_read < 0) {
        perror("read");
        abort();
    } else if (bytes_read == 0) {
        stream->eof = 1;
    } else {
        stream->cursor = stream->buf;
        stream->last_read_len = bytes_read;
    }
}
