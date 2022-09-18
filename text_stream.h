#ifndef TEXT_STREAM_H
#define TEXT_STREAM_H

enum text_stream_type {
    TEXT_STREAM_TYPE_STRING,
    TEXT_STREAM_TYPE_FD
};

struct text_stream {
    enum text_stream_type type;
    char *buf;
    char *cursor;
    int fd;
    size_t last_read_len;
    int eof;
};

void text_stream_init_str(struct text_stream *stream, char *input_string);

void text_stream_init_fd(struct text_stream *stream, int fd);

void text_stream_free(struct text_stream *stream);

char text_stream_peek(struct text_stream *stream);

void text_stream_advance(struct text_stream *stream);

int text_stream_eof(struct text_stream *stream);

#endif
