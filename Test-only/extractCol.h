#pragma once

#include <vector>
#include <string>
#include "gept3dar.h"
#include <dbapserv.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <AcDb.h>

// Inline the function to avoid multiple definition errors
inline std::string getBlockName(AcDbObjectId blockId);

struct ColumnData {
    std::string blockName;
    AcGePoint3d position;
    double rotation;
    AcDbObjectId blockId;
};

class ColumnExtractor {
public:
    std::vector<ColumnData> extractColumnData();
    void extractAndCreateBlock(const std::string& blockName, const std::string& jsonFilePath);
};
