#ifndef TEXT_STREAM_H
#define TEXT_STREAM_H

struct text_stream {
    char *buf;
    char *cursor;
};

void text_stream_init(struct text_stream *stream, char *input_string);

char text_stream_peek(struct text_stream *stream);

void text_stream_advance(struct text_stream *stream);

int text_stream_eof(struct text_stream *stream);

#endif
