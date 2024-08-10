#pragma once

#include <vector>
#include <string>
#include "gept3dar.h"

struct ColumnDataload {
    std::string blockName;
    AcGePoint3d position;
    double rotation;
};

class ColumnPlacermain {
public:
    std::vector<ColumnDataload> extractColumnData();
    void saveColumnDataToJson(const std::vector<ColumnDataload>& columns, const std::string& filePath);
    std::vector<ColumnDataload> loadColumnDataFromJson(const std::string& filePath);
    void placeColumnsFromJson(const std::string& filePath);
};
