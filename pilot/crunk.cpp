#include "crunk.h"
#include <ctype.h>
#include <string.h>


bool read_crunk_block(FILE *f, std::string &okey, std::string &oinfo, float *&ovalue)
{
    ovalue = nullptr;

    char line[1025] = { 0 };
    /* read key line */
    if (ferror(f) || feof(f) || (0 == fgets(line, 1025, f))) {
        return false;
    }
    line[1024] = 0;
    char *s = line;
    while (*s && isspace(*s)) {
        ++s;
    }
    char *e = s + strlen(s);
    while (e > s && isspace(e[-1])) {
        --e;
    }
    *e = 0;
    okey = s;

    /* read shape */
    if (ferror(f) || feof(f) || (0 == fgets(line, 1025, f))) {
        fprintf(stderr, "read_crunk_block(): error getting shape\n");
        return false;
    }
    line[1024] = 0;
    oinfo = line;

    /* read blob size */
    if (ferror(f) || feof(f) || (0 == fgets(line, 1025, f))) {
        fprintf(stderr, "read_crunk_block(): error getting blob size\n");
        return false;
    }
    line[1024] = 0;
    oinfo = line;
    int sz = 0;
    if (sscanf(line, " %d", &sz) < 1) {
        fprintf(stderr, "read_crunk_block(): blob size not found\n");
        return false;
    }
    if (sz < 4 || sz > 1000000) {
        fprintf(stderr, "read_crunk_block(): blob size out of range\n");
        return false;
    }

    /* read blob */
    int n = (sz + 3) / 4;
    ovalue = new float[n+1];
    if (fread(ovalue, 1, sz+1, f) != (unsigned long)(sz+1)) {
        fprintf(stderr, "read_crunk_block(): blob data not found\n");
delret:
        delete[] ovalue;
        ovalue = nullptr;
        return false;
    }

    /* read terminating dash */
    if (ferror(f) || feof(f) || (0 == fgets(line, 1025, f))) {
        fprintf(stderr, "read_crunk_block(): terminator not found\n");
        goto delret;
    }
    if (line[0] != '-') {
        fprintf(stderr, "read_crunk_block(): terminator bad format\n");
        goto delret;
    }
    
    return true;
}


