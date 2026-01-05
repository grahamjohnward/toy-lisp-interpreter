#include "interp.h"
#include "lisp.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct interpreter_settings {
    size_t heap_size;
    char *image;
    int use_vm;
    char *vmboot;
};

static struct option options[] = {
    { "heap-size", optional_argument, 0, 1 },
    { "image", optional_argument, 0, 2 },
    { "use-vm", no_argument, 0, 3 },
    { "vmboot", optional_argument, 0, 4 },
    { 0, 0, 0, 0 }
};

static size_t parse_heap_size(char *arg)
{
    size_t len = strlen(arg);
    char unit = arg[len - 1];
    char *size_str = arg;
    if (unit < '0' || unit > '9') {
        size_str = alloca(len);
        strncpy(size_str, arg, len - 1);
        size_str[len - 1] = '\0';
    } else {
        unit = 0;
    }
    char *endptr;
    errno = 0;
    size_t heap_size = strtol(size_str, &endptr, 10);
    if (errno) {
        perror("Heap size");
        exit(1);
    } else if (*endptr != '\0') {
        printf("Bad heap size %s\n", arg);
        exit(1);
    }
    if (unit) {
        if (unit == 'k' || unit == 'K') {
            heap_size *= 1024;
        } else if (unit == 'm' || unit == 'M') {
            heap_size *= 1024 * 1024;
        } else if (unit == 'g' || unit == 'G') {
            heap_size *= 1024 * 1024 * 1024;
        } else {
            printf("Bad heap size unit: %c\n", unit);
            exit(1);
        }
    }
    return heap_size;
}

static int parse_args(int argc, char **argv, struct interpreter_settings *settings)
{
    settings->heap_size = 1024 * 1024; /* default */
    settings->image = NULL;
    settings->use_vm = 0;
    int c;
    while (1) {
        int option_index;
        c = getopt_long_only(argc, argv, "+", options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 1:
            settings->heap_size = parse_heap_size(optarg);
            break;
        case 2:
            settings->image = malloc(strlen(optarg));
            strcpy(settings->image, optarg);
            break;
        case 3:
            settings->use_vm = 1;
            break;
        case 4:
            settings->vmboot = malloc(strlen(optarg) + 1);
            strcpy(settings->vmboot, optarg);
            break;
        default:
            abort();
        }
    }
    return optind;
}

int main(int argc, char **argv)
{
    struct interpreter_settings settings;
    int i = parse_args(argc, argv, &settings);
    if (settings.image)
        init_interpeter_from_image(settings.image);
    else
        init_interpreter2(settings.heap_size, settings.use_vm);
    if (settings.use_vm) {
        int fd = open(settings.vmboot, O_RDONLY);
        if (fd < 0) {
            perror(settings.vmboot);
            abort();
        }
        struct text_stream ts;
        text_stream_init_fd(&ts, fd);
        lisp_object_t bootcode = parse1(&ts);
        close(fd);
        text_stream_free(&ts);
        interp->vm.registers.code_vector = bootcode;
        lisp_object_t arglist = allocate_vector(LispInt(argc - i));
        for (int argidx = 0; i < argc; i++, argidx++) {
            lisp_object_t string = allocate_string(strlen(argv[i]) + 1, argv[i]);
            svref_set(arglist, LispInt(argidx), string);
        }
        vm_inst_push(&interp->vm, arglist);
        vm_run(&interp->vm);
    } else {
        for (; i < argc; i++)
            load_str(argv[i]);
    }
    free_interpreter();
    if (settings.image)
        free(settings.image);
    if (settings.vmboot)
        free(settings.vmboot);
    return 0;
}
