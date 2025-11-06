#include "FileSystemManager.h"
#include "Logger.h"

bool FileSystemManager::s_mounted = false;

bool FileSystemManager::ensureMounted(bool formatOnFail)
{
    if (s_mounted) {
        return true;
    }

    if (LittleFS.begin(false)) {
        s_mounted = true;
        return true;
    }

    if (!formatOnFail) {
        Logger::getInstance().log("LittleFS: mount failed");
        return false;
    }

    Logger::getInstance().log("LittleFS: mount failed, attempting format");
    if (!LittleFS.begin(true)) {
        Logger::getInstance().log("LittleFS: format/mount failed");
        return false;
    }

    s_mounted = true;
    return true;
}
