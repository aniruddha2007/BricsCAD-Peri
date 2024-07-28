#pragma once

#include "geassign.h"
#include <map>
#include <vector>
#include <string>
#include "dbmain.h"

class ColumnPlacer {
public:
    struct Point3dComparator {
        bool operator()(const AcGePoint3d& lhs, const AcGePoint3d& rhs) const {
            return (lhs.x < rhs.x || (lhs.x == rhs.x && (lhs.y < rhs.y || (lhs.y == rhs.y && lhs.z < rhs.z))));
        }
    };

    static std::map<AcGePoint3d, std::vector<AcGePoint3d>, Point3dComparator> columnMap;

    static AcDbObjectId loadAsset(const wchar_t* blockName);
    static void placeColumns();
    static AcDbObjectId createCompositeBlock(const wchar_t* newBlockName);
};
