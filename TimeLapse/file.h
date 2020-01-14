#pragma once
bool writeFile(const char *path, const unsigned char *data, unsigned long len);
bool appendFile(const char *path, const unsigned char *data, unsigned long len);
bool initFileSystem();
bool createDir(const char *path);
bool fileExists(const char *path);
