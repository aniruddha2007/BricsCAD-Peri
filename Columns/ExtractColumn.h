#pragma once

#include <acdb.h>
#include <dbents.h>
#include <dbapserv.h>
#include <dbsymtb.h>
#include <nlohmann/json.hpp> // Include nlohmann JSON header
#include <fstream>
#include <string>
#include <codecvt>
#include <locale>

// Declare the function SaveBlocksToJson
void ExtractColumn();
//void SaveBlocksToJson(const std::string& filePath);
