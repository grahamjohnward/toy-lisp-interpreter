#include <stdio.h>

#include "text_stream.h"

void text_stream_init(struct text_stream *stream, char *input_string)
{
    stream->buf = input_string;
    stream->cursor = input_string;
}

char text_stream_peek(struct text_stream *stream)
{
    return *stream->cursor;
}

void text_stream_advance(struct text_stream *stream)
{
    stream->cursor++;
}

int text_stream_eof(struct text_stream *stream)
{
    return text_stream_peek(stream) == 0;
}