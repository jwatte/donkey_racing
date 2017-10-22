#include "cleanup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/statvfs.h>

#include <map>
#include <string>

static std::map<std::string, std::map<time_t, std::string>> filesToClean;

bool cleanup_temp_folder(char const *f, time_t before) {
    if (!f) {
        return false;
    }
    struct statvfs vfss = { 0 };
    if (statvfs(f, &vfss) < 0) {
        perror(f);
        return false;
    }
    unsigned long long freespace = (unsigned long long)vfss.f_bavail;
    freespace = freespace * (unsigned long long)vfss.f_bsize;
    fprintf(stderr, "%s: %llu MB free\n", f, freespace / 1024 / 1024);
    if (freespace > 1024*1024*1024) {
        //  more than a gig available? Don't clean up.
        return true;
    }
    DIR *d = opendir(f);
    if (!d) {
        return false;
    }
    struct dirent *dent;
    while ((dent = readdir(d)) != nullptr) {
        struct stat st;
        std::string name(dent->d_name);
        if (0 == stat((f + ("/" + name)).c_str(), &st)) {
            if (S_ISREG(st.st_mode)) {
                char const *ext = strrchr(name.c_str(), '.');
                if (!ext) ext = ".";
                filesToClean[ext][st.st_mtime] = name;
            }
        }
    }
    closedir(d);
    /* keep 20 of each extension, and keep everything newer than the cutoff */
    int ndeleted = 0;
    for (auto const &ext : filesToClean) {
        size_t nthere = ext.second.size();
        if (nthere < 20) {
            continue;
        }
        for (auto const &tfn : ext.second) {
            if (tfn.first >= before) {
                break;
            }
            if (nthere <= 20) {
                break;
            }
            ++ndeleted;
            --nthere;
            std::string path(f + ("/" + tfn.second));
            if (unlink(path.c_str()) < 0) {
                perror(path.c_str());
            } else {
                fprintf(stderr, "cleanup: %s\n", path.c_str());
            }
        }
    }
    fprintf(stderr, "cleaned up %d files from %s\n", ndeleted, f);
    return true;
}
