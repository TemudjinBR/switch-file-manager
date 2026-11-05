#include <cerrno>
#include <sys/stat.h>
// Reproduz a restrição de fsFsRenameFile: não sobrescrever destino existente.
extern "C" int __real_rename(const char*, const char*);
extern "C" int __wrap_rename(const char* source, const char* target) {
    struct stat info{};
    if (lstat(target, &info) == 0) { errno = EEXIST; return -1; }
    if (errno != ENOENT) return -1;
    return __real_rename(source, target);
}
