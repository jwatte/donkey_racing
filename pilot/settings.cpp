#include "settings.h"
#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


static std::map<std::string, std::string> gMap;

static std::string fn(char const *appname) {
    char const *home = getenv("HOME");
    if (!home) home = "/var/tmp";
    std::string s(home);
    s += "/";
    s += appname;
    s += ".ini";
    return s;
}

int load_settings(char const *appname) {
    std::string path(fn(appname));
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        perror(path.c_str());
        return 0;
    }
    char line[2048];
    int lineno = 1;
    while (!feof(f) && !ferror(f)) {
        line[0] = 0;
        if (!fgets(line, 2048, f)) {
            break;
        }
        line[2047] = 0;
        if (line[0] != '#') {
            char *end = &line[strlen(line)];
            while (end != line && ((end[-1] == 10) || (end[-1] == 13))) {
                --end;
            }
            *end = 0;
            char const *eq = strchr(line, '=');
            if (!eq) {
                fprintf(stderr, "%s:%d: bad setting: %s\n", path.c_str(), lineno, line);
            } else {
                gMap[std::string((char const *)line, eq)] = std::string(eq+1, (char const *)end);
            }
        }
        ++lineno;
    }
    fclose(f);
    return gMap.size();
}

int save_settings(char const *appname) {
    std::string path(fn(appname));
    std::string pathtmp = path + ".tmp";
    FILE *f = fopen(pathtmp.c_str(), "wb");
    if (!f) {
        perror(pathtmp.c_str());
        return 0;
    }
    for (auto const &ptr : gMap) {
        fprintf(f, "%s=%s\n", ptr.first.c_str(), ptr.second.c_str());
    }
    fclose(f);
    unlink(path.c_str());
    if (rename(pathtmp.c_str(), path.c_str()) < 0) {
        perror(path.c_str());
        return 0;
    }
    return gMap.size();
}


char const *get_setting(char const *name, char const *dflt) {
    auto const &ptr(gMap.find(std::string(name)));
    if (ptr == gMap.end()) {
        return dflt;
    }
    return (*ptr).second.c_str();
}

long get_setting_int(char const *name, int dflt) {
    auto const &ptr(gMap.find(std::string(name)));
    if (ptr == gMap.end()) {
        return dflt;
    }
    char const *beg = (*ptr).second.c_str();
    char *end = NULL;
    long l = strtol(beg, &end, 10);
    if (!end || end == beg) {
        return dflt;
    }
    return l;
}

double get_setting_float(char const *name, double dflt) {
    auto const &ptr(gMap.find(std::string(name)));
    if (ptr == gMap.end()) {
        return dflt;
    }
    char const *beg = (*ptr).second.c_str();
    char *end = NULL;
    double d = strtod(beg, &end);
    if (!end || end == beg) {
        return dflt;
    }
    return d;
}

int set_setting(char const *name, char const *value) {
    if (!name[0] || name[0] == '#' || strchr(name, ' ') || strchr(name, '\r') || strchr(name, '\n')) {
        return 0;
    }
    if (strchr(value, '\n') || strchr(value, '\r')) {
        return 0;
    }
    gMap[std::string(name)] = std::string(value);
    return gMap.size();
}

int set_setting_long(char const *name, long value) {
    char buf[24];
    sprintf(buf, "%ld", value);
    return set_setting(name, buf);
}

int set_setting_float(char const *name, double value) {
    char buf[24];
    sprintf(buf, "%g", value);
    return set_setting(name, buf);
}

int remove_setting(char const *name) {
    auto ptr(gMap.find(std::string(name)));
    if (ptr == gMap.end()) {
        return 0;
    }
    gMap.erase(ptr);
    return 1;
}

int has_setting(char const *name) {
    auto ptr(gMap.find(std::string(name)));
    if (ptr == gMap.end()) {
        return 0;
    }
    return 1;
}

void iterate_settings(int (*func)(char const *name, char const *value, void *cookie), void *cookie) {
    auto ptr(gMap.begin()), end(gMap.end());
    while (ptr != end) {
        char const *name = (*ptr).first.c_str();
        char const *value = (*ptr).second.c_str();
        ++ptr;
        if (!func(name, value, cookie)) {
            break;
        }
    }
}



