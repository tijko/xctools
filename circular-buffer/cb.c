#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "cb.h"

// do some randomized inserts (printing for debug indices)

void free_sample(struct cb_sample *sample)
{
    int count = sample->file_count;
    char **filelist = sample->filelist;

    for (int i=0; i < count; i++)
        free(filelist[i]);

    free(filelist);
    free(sample);
}

struct cb_sample *get_sample(char *dirname)
{
    struct cb_sample *sample = malloc(sizeof *sample);
    sample->dirname = dirname;
    int count = 0;

    DIR *dirh = opendir(dirname);

    if (!dirh)
        ERROR("opendir");

    char **filelist = NULL;

    struct dirent *current;

    while ((current = readdir(dirh))) {
        
        if (current->d_type != DT_REG) 
            continue;

        filelist = realloc(filelist, (sizeof(filelist) * count) + 1); 

        filelist[count++] = strdup(current->d_name);
    }

    sample->filelist = filelist;
    sample->file_count = count;
    closedir(dirh);

    return sample;
}

int write_buffer(struct cb *cbuf, char *data, int len)
{
    /* Always adjust head (possibly tail) */

    int capacity = cbuf->capacity;
    int current = cbuf->current_capacity;

    if (len > capacity)
        return -1;

    cbuf->current_capacity = current + len > capacity ? capacity : current + len;

    int tail = cbuf->tail;
    int head = cbuf->head;
   
    int tail_gap = tail >= head ? tail - head : (capacity - head) + tail;

    if ((len + head) > capacity) {
        int n = (len + head) - capacity;
        memcpy(&(cbuf->buffer[head]), data, n);
        len -= n;
        head = 0;
    }

    memcpy(&(cbuf->buffer[head]), data, len);
    cbuf->head = (cbuf->head + len) % capacity;

    if (tail_gap < len) 
        cbuf->tail = (cbuf->head + 1) % capacity;    

    return 0;
}

char *read_buffer(struct cb *cbuf, int len)
{
    /* Always just adjust tail */

    int capacity = cbuf->capacity;

    if (len > capacity)
        return NULL;

    int tail = cbuf->tail;

    len = len > cbuf->current_capacity ? cbuf->current_capacity : len;

    char *rbuf = malloc(sizeof(char) * len);
    int n_wrap = 0;
    // possible wrap...
    if ((len + tail) > capacity) {
        memcpy(&(cbuf->buffer[tail]), rbuf, capacity - tail);
        len -= tail;
        tail = 0;
        n_wrap = capacity - tail;
    }

    memcpy(rbuf + n_wrap, &(cbuf->buffer[tail]), len);
    cbuf->tail = (tail + len);

    return rbuf;
}

char *pick_random_file(struct cb_sample *sample)
{
    printf("Random file-no...\n");
    int random = rand() % sample->file_count;
    printf("Between <0> & <%d>...<%d>\n", sample->file_count, random);
    char *fname = sample->filelist[random];
    printf("Picked file: %s\n", fname);

    size_t fpath_len = strlen(fname) + strlen(sample->dirname);
    char *fpath = malloc(fpath_len + 3);
    snprintf(fpath, fpath_len + 2, "%s/%s", sample->dirname, fname);
    
    struct stat st;
    stat(fpath, &st);
    printf("%s\n", fpath);
    printf("Size: %ld\n", st.st_size);
    char *filedata = malloc(sizeof(char) * st.st_size + 1);

    int r_fd = open(fpath, O_RDONLY);
    if (r_fd < 0)
        ERROR("open");

    int rbytes = read(r_fd, filedata, st.st_size);
    close(r_fd);

    if (rbytes < 0)
        ERROR("read");

    free(fpath);

    return filedata;
}

static inline void create_seed(void)
{
    int seed = time(NULL);
    printf("Seed: %d\n", seed);
    srand48(seed);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Pass a valid directory...\n");
        exit(0);
    }

    struct cb cbuffer;
    memset(&cbuffer, 0, sizeof cbuffer);
    cbuffer.capacity = 1024;
    cbuffer.head = 1;

    printf("Building file-list...%s\n", argv[1]);
    struct cb_sample *sample = get_sample(argv[1]);
    printf("Sample created...\n");
    create_seed();

    // make 5 random inserts...
    for (int i=0; i < 5; i++) {
        int wbytes = rand() % 1024;
        char *insert_file = pick_random_file(sample);
        printf("Writing: %d\n", wbytes);
        /* inspect test
        char *tmp = malloc(sizeof(char) * wbytes + 1);
        memcpy(tmp, insert_file, wbytes);
        tmp[wbytes] = '\0';
        printf("%s\n", tmp);
        free(tmp);
        */
        write_buffer(&cbuffer, insert_file, wbytes);
        free(insert_file);
        // read from buffer or inspect...
    }

    int rbytes = rand() % 1024;
    printf("Reading: %d\n", rbytes);
    char *r1 = read_buffer(&cbuffer, rbytes);
    printf("%s\n", r1);

    free_sample(sample);
 
    return 0;
}

