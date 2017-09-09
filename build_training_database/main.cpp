#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <list>
#include <string>

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

struct dump_chunk {
    char type[4];
    std::string filename;
};

std::list<dump_chunk> dumpChunks;
std::list<std::string> filenameArgs;
std::string datasetName;

void check_header(FILE *f, char const *name) {
    file_header fh;
    if ((12 != fread(&fh, 1, 12, f)) || strncmp(fh.type, "RIFF", 4) || strncmp(fh.subtype, "h264", 4)) {
        fprintf(stderr, "%s: not a h264 RIFF file\n", name);
        exit(1);
    }
}

bool generate_requested_file(char const *type, char const *output) {
    return false;
}

bool generate_dataset(char const *output) {
    avcodec_register_all();
    auto codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "avcodec_find_decoder(): h264 not found\n");
        return false;
    }
    auto ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        fprintf(stderr, "avcodec_alloc_context3(): failed to allocate\n");
        return false;
    }
    if (avcodec_open2(ctx, codec, NULL) < 0) {
        fprintf(stderr, "avcodec_open2(): failed to open\n");
        return false;
    }
    auto frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "av_frame_alloc(): alloc failed\n");
        return false;
    }
    auto parser = av_parser_init(AV_CODEC_ID_H264);
    if (!parser) {
        fprintf(stderr, "av_parser_init(): h264 failed\n");
        return false;
    }
    return false;
}

int main(int argc, char const *argv[]) {
    if (argc == 1 || argv[0][0] == '-') {
usage:
        fprintf(stderr, "usage: mktrain [options] file1.riff file2.riff ...\n");
        fprintf(stderr, "--dump type:filename\n");
        fprintf(stderr, "--dataset name.lmdb\n");
        exit(1);
    }
    uint64_t h264Offset = 0;
    uint64_t pdtsOffset = 0;
    uint64_t timeOffset = 0;
    for (int i = 1; i != argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "--dataset")) {
                ++i;
                if (!argv[i]) {
                    fprintf(stderr, "--dataset requires filename.lmdb\n");
                    exit(1);
                }
                datasetName = argv[i];
                continue;
            }
            if (!strcmp(argv[i], "--dump")) {
                ++i;
                if (!argv[i]) {
                    fprintf(stderr, "--dump requires type:filename\n");
                    exit(1);
                }
                if (strlen(argv[i]) < 6 || argv[i][4] != ':') {
                    fprintf(stderr, "--dump bad argument: %s\n", argv[i]);
                    exit(1);
                }
                dump_chunk dc;
                memcpy(dc.type, argv[i], 4);
                dc.filename = &argv[i][5];
                dumpChunks.push_back(dc);
                continue;
            }
            fprintf(stderr, "unknown argument: '%s'\n", argv[i]);
            goto usage;
        } else {
            filenameArgs.push_back(argv[i]);
        }
    }
    if (!filenameArgs.size()) {
        fprintf(stderr, "no input files specified\n");
        goto usage;
    }
    for (auto const &path : filenameArgs) {
        char const *cpath = path.c_str();
        FILE *f = fopen(cpath, "rb");
        if (!f) {
            perror(cpath);
            exit(1);
        }
        check_header(f, cpath);
        fseek(f, 0, 2);
        long len = ftell(f);
        fseek(f, 12, 0);
        fprintf(stdout, "%s: %ld bytes\n", cpath, len);
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
            fseek(f, (ch.size + 3) & ~3, 1);
        }
    }

    fprintf(stdout, "%ld h264 chunks size %ld\n%ld pdts chunks size %ld\n%ld time chunks size %ld\n",
            h264Chunks.size(), h264Offset, pdtsChunks.size(), pdtsOffset, timeChunks.size(), timeOffset);
    
    int errors = 0;
    for (auto const &dc : dumpChunks) {
        if (!generate_requested_file(dc.type, dc.filename.c_str())) {
            fprintf(stderr, "Could not generate '%s' of type '%.4s'\n", dc.filename.c_str(), dc.type);
            ++errors;
        }
    }
    if (datasetName.size()) {
        if (!generate_dataset(datasetName.c_str())) {
            fprintf(stderr, "Error generting dataset '%s'\n", datasetName.c_str());
            ++errors;
        }
    }

    return errors;
}
