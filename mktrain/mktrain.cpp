#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <list>

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>

}



struct file_header {
    char type[4];
    uint32_t zero;
    char subtype[4];
};

struct chunk_header {
    char type[4];
    uint32_t size;
};

struct stream_chunk {
    FILE *file;
    uint32_t fileoffset;
    uint32_t streamoffset;
    uint32_t size;
};

std::list<stream_chunk> h264Chunks;
std::list<stream_chunk> timeChunks;
std::list<stream_chunk> pdtsChunks;

void check_header(FILE *f, char const *name) {
    file_header fh;
    if ((12 != fread(&fh, 1, 12, f)) || strncmp(fh.type, "RIFF", 4) || strncmp(fh.subtype, "h264", 4)) {
        fprintf(stderr, "%s: not a h264 RIFF file\n", name);
        exit(1);
    }
}

void generate_dataset() {
    avcodec_register_all();
    auto codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "avcodec_find_decoder(): h264 not found\n");
        exit(1);
    }
    auto ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        fprintf(stderr, "avcodec_alloc_context3(): failed to allocate\n");
        exit(1);
    }
    if (avcodec_open2(ctx, codec, NULL) < 0) {
        fprintf(stderr, "avcodec_open2(): failed to open\n");
        exit(1);
    }
    auto frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "av_frame_alloc(): alloc failed\n");
        exit(1);
    }
    auto parser = av_parser_init(AV_CODEC_ID_H264);
    if (!parser) {
        fprintf(stderr, "av_parser_init(): h264 failed\n");
        exit(1);
    }
    return;
}

int main(int argc, char const *argv[]) {
    if (argc == 1 || argv[0][0] == '-') {
        fprintf(stderr, "usage: mktrain file1.riff file2.riff ...\n");
        exit(1);
    }
    uint64_t h264Offset = 0;
    uint64_t pdtsOffset = 0;
    uint64_t timeOffset = 0;
    FILE *omov = fopen("omov.h264", "wb");
    char buf[1024*1024];
    for (int i = 1; i != argc; ++i) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) {
            perror(argv[i]);
            exit(1);
        }
        check_header(f, argv[i]);
        fseek(f, 0, 2);
        long len = ftell(f);
        fseek(f, 12, 0);
        fprintf(stdout, "%s: %ld bytes\n", argv[i], len);
        while (!feof(f) && !ferror(f)) {
            long pos = ftell(f);
            chunk_header ch;
            long rd = fread(&ch, 1, 8, f); 
            if (8 != rd) {
                if (rd != 0) {
                    perror("short read");
                }
                continue;
            }
            stream_chunk sc;
            sc.file = f;
            sc.fileoffset = pos;
            sc.streamoffset = 0;
            sc.size = ch.size;
            if (!strncmp(ch.type, "h264", 4)) {
                sc.streamoffset = h264Offset;
                h264Offset += sc.size;
                h264Chunks.push_back(sc);
            }
            else if (!strncmp(ch.type, "pdts", 4)) {
                sc.streamoffset = pdtsOffset;
                pdtsOffset += sc.size;
                pdtsChunks.push_back(sc);
            }
            else if (!strncmp(ch.type, "time", 4)) {
                sc.streamoffset = timeOffset;
                timeOffset += sc.size;
                timeChunks.push_back(sc);
            }
            if (!strncmp(ch.type, "h264", 4)) {
                fread(buf, 1, ch.size, f);
                fseek(f, (4 - (ch.size & 3))&3, 1);
                fwrite(buf, 1, ch.size, omov);
            } else {
                fseek(f, (ch.size + 3) & ~3, 1);
            }
        }
    }
    fclose(omov);

    fprintf(stdout, "%ld h264 chunks size %ld\n%ld pdts chunks size %ld\n%ld time chunks size %ld\n",
            h264Chunks.size(), h264Offset, pdtsChunks.size(), pdtsOffset, timeChunks.size(), timeOffset);
    
    generate_dataset();

    return 0;
}
