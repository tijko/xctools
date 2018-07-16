#include <string.h>
#include <errno.h>
#include <unistd.h>


#define ERROR(call)                                      \
    do {                                                 \
        printf("Error %s: %s\n", call, strerror(errno)); \
        exit(0);                                         \
    } while ( 1 )                                        \


// XXX is there an "ideal" size for a circular buffer?

#define CB_MAX 1024

struct cb {
    char buffer[1024];
    int head;
    int tail;
    size_t capacity;
    size_t current_capacity;
};

struct cb_sample {
    int file_count;
    char **filelist;
    char *dirname;
};
