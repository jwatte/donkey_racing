#if !defined(filethread_h)
#define filehthread_h

#define FILE_OPEN 1
#define FILE_WRITE 2
#define FILE_CLOSE 3

#include <stdlib.h>
#include <vector>

struct FileResult {
    int name;
    int type;
    int result;
};

bool start_filethread();
bool stop_filethread();
bool new_file(int name, char const *path);
bool write_file(int name, void const *data, size_t size);
bool write_file_vec(int name, std::vector<char> &data);
bool flush_file(int name);
bool close_file(int name);
size_t get_results(FileResult *fr, size_t maxNum);

#endif  //  filethread_h

