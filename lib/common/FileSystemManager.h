#ifndef FILE_SYSTEM_MANAGER_H
#define FILE_SYSTEM_MANAGER_H

#include <LittleFS.h>

class FileSystemManager {
public:
    // Ensure LittleFS is mounted. The filesystem remains mounted for the lifetime
    // of the application so subsequent callers do not incur the mount cost again.
    static bool ensureMounted(bool formatOnFail = true);

private:
    static bool s_mounted;
};

#endif // FILE_SYSTEM_MANAGER_H
