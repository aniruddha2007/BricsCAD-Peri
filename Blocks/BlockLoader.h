#pragma once

#include <string>

class BlockLoader {
public:
    static void loadBlocksFromJson();
    static std::string extractFileNameFromPath(const std::string& path);
    static void loadBlockIntoBricsCAD(const char* blockName, const char* blockPath);
    static ACHAR* charToACHAR(const char* str);
};
