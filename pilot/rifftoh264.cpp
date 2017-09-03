#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


void read_file_header(FILE *f) {
    char hdr[12];
    if (12 != fread(hdr, 1, 12, f)) {
        perror("\nshort read");
        exit(1);
    }
    if (strncmp(hdr, "RIFF", 4)) {
        perror("\nnot a RIFF file\n");
        exit(1);
    }
    if (strncmp(&hdr[8], "h264", 4)) {
        fprintf(stderr, "\nnot a h264 file\n");
        exit(1);
    }
}

bool read_block_header(FILE *in, char *type, size_t *size, size_t *alignedsize, long *pos) {
    unsigned char hdr[8];
    long l = fread(hdr, 1, 8, in);
    if ((l == 0) && feof(in)) {
        return false;
    }
    if (l != 8) {
        perror("\nshort read");
        exit(1);
    }
    memcpy(type, hdr, 4);
    type[4] = 0;
    *size = ((size_t)hdr[4]) | ((size_t)hdr[5] << 8) | ((size_t)hdr[6] << 16) | ((size_t)hdr[7] << 24);
    *alignedsize = (*size + 3) & (size_t)~3;
    *pos += 8;
    return true;
}

#define COPY_SIZE 65536
char buf[COPY_SIZE];

void copy_block(FILE *in, size_t size, size_t align, FILE *out, long *pos) {
    while (align > 0) {
        size_t to_read = COPY_SIZE;
        if (to_read > align) {
            to_read = align;
        }
        size_t did_read = fread(buf, 1, to_read, in);
        if (did_read != to_read) {
            perror("\nshort read");
            fprintf(stderr, "expected %lu bytes at offset %ld (actual %ld); got %lu\n",
                    (unsigned long)to_read, *pos, ftell(in), (unsigned long)did_read);
            exit(1);
        }
        align -= did_read;
        *pos += did_read;
        size_t to_write = did_read;
        if (to_write > size) {
            to_write = size;
        }
        if (to_write > 0) {
            size_t did_write = fwrite(buf, 1, to_write, out);
            if (did_write != to_write) {
                perror("\nshort write");
                exit(1);
            }
            size -= did_write;
        }
    }
}

void skip_block(FILE *in, size_t align, long *pos) {
    if (in != stdin) {
        int res = fseek(in, align, 1);
        if (res != 0) {
            perror("\nfseek() failed");
            exit(1);
        }
        *pos += align;
        return;
    }
    /* For stdin, we have to actually read through the data 
     * and discard it.
     */
    while (align > 0) {
        size_t to_read = COPY_SIZE;
        if (to_read > align) {
            to_read = align;
        }
        size_t did_read = fread(buf, 1, to_read, in);
        if (did_read != to_read) {
            perror("\nshort read");
            exit(1);
        }
        align -= did_read;
        *pos += did_read;
    }
}


int main(int argc, char const *argv[]) {
    if (argc > 1 && argv[1][0] == '-') {
        fprintf(stderr, "usage: rifftoh264 [inputname.riff [outputname.h264]]\n");
        fprintf(stderr, "Uses stdin/stdout when not specified.\n");
        fprintf(stderr, "Note: only extracts h264 data found in RIFF files\n");
        fprintf(stderr, "created by 'pilot'; not a generic conversion utility.\n");
        exit(1);
    }
    FILE *in = (argv[1] ? fopen(argv[1], "rb") : stdin);
    if (!in) {
        perror(argv[1]);
        exit(1);
    }
    FILE *out = (argv[1] && argv[2] ? fopen(argv[2], "wb") : stdout);
    if (!out) {
        perror(argv[2]);
        exit(1);
    }
    long end = 0;
    if (in != stdin) {
        fseek(in, 0, 2);
        end = ftell(in);
        fseek(in, 0, 0);
    }
    read_file_header(in);
    size_t size, alignedsize;
    char type[5];
    long pos = 12;
    long lastpos = 0;
    while (((end == 0) || (pos < end)) && read_block_header(in, type, &size, &alignedsize, &pos)) {
        if (!strcmp(type, "h264")) {
            copy_block(in, size, alignedsize, out, &pos);
        } else {
            skip_block(in, alignedsize, &pos);
        }
        if (end != 0 && (pos - lastpos > 300000)) {
            lastpos = pos;
            char dashes[26];
            for (int i = 0; i != 25; ++i) {
                dashes[i] = (pos / (end / 25)) > i ? '=' : ' ';
            }
            dashes[25] = 0;
            fprintf(stderr, "[%s] (%03d/100)\r", dashes, (int)(pos / (end / 100)));
            fflush(stderr);
        }
    }
    if (end != 0) {
        fprintf(stderr, "\n");
    }
    if (in != stdin) {
        fclose(in);
    }
    if (out != stdout) {
        fclose(out);
    }
    return 0;
}

