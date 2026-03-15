// POSIX-backed fs::FS / fs::File implementation for the Linux native build.
// Provides the same interface as Arduino's FS.h so that IdentityStore,
// ClientACL, CommonCLI, and RegionMap compile unchanged.
#pragma once

#include "Arduino.h"  // for Stream / Print base classes
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

namespace fs {

// ---------------------------------------------------------------------------
// File
// ---------------------------------------------------------------------------
class File : public Stream {
    FILE* _fp  = nullptr;
    char  _path[256] = {};
public:
    File() = default;
    explicit File(FILE* fp, const char* path = "") : _fp(fp) {
        strncpy(_path, path, sizeof(_path) - 1);
    }

    explicit operator bool() const { return _fp != nullptr; }

    // Stream interface
    size_t write(uint8_t c) override {
        if (!_fp) return 0;
        return fwrite(&c, 1, 1, _fp);
    }
    size_t write(const uint8_t* buf, size_t n) override {
        if (!_fp) return 0;
        return fwrite(buf, 1, n, _fp);
    }
    int available() override {
        return (_fp && !feof(_fp)) ? 1 : 0;
    }
    int read() override {
        if (!_fp) return -1;
        int c = fgetc(_fp);
        return (c == EOF) ? -1 : c;
    }
    int peek() override { return -1; }

    // Bulk read (used by IdentityStore, ClientACL, CommonCLI)
    size_t read(uint8_t* buf, size_t n) {
        if (!_fp) return 0;
        return fread(buf, 1, n, _fp);
    }

    void close() {
        if (_fp) { fclose(_fp); _fp = nullptr; }
    }

    const char* name() const { return _path; }
};

// ---------------------------------------------------------------------------
// FS
// ---------------------------------------------------------------------------
class FS {
    char _root[256] = {};
public:
    FS() = default;

    // Call once (from setup()) to set the data directory prefix.
    void setRoot(const char* root) {
        strncpy(_root, root, sizeof(_root) - 1);
    }

    // Open with optional mode string ("r", "w", "a").
    // path must start with '/'; it is appended to the root.
    File open(const char* path, const char* mode = "r") {
        char full[512];
        snprintf(full, sizeof(full), "%s%s", _root, path);
        FILE* fp = fopen(full, mode);
        return File(fp, full);
    }

    bool exists(const char* path) {
        char full[512];
        snprintf(full, sizeof(full), "%s%s", _root, path);
        struct stat st;
        return stat(full, &st) == 0;
    }

    bool remove(const char* path) {
        char full[512];
        snprintf(full, sizeof(full), "%s%s", _root, path);
        return unlink(full) == 0;
    }

    bool mkdir(const char* path) {
        char full[512];
        snprintf(full, sizeof(full), "%s%s", _root, path);
        return ::mkdir(full, 0755) == 0 || errno == EEXIST;
    }

    bool rename(const char* from, const char* to) {
        char f[512], t[512];
        snprintf(f, sizeof(f), "%s%s", _root, from);
        snprintf(t, sizeof(t), "%s%s", _root, to);
        return ::rename(f, t) == 0;
    }
};

}  // namespace fs

// Global instance — root is set in main() / setup() via LinuxFS.setRoot()
extern fs::FS LinuxFS;

// Pull File into global namespace, matching Arduino convention
using File = fs::File;
