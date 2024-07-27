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
    static void placeColumn(const AcGePoint3d& position, const int width, const int length);
    static void placeColumns();
    static void placeColumnsCmd();
    static void insertCompositeBlockCmd();
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);

    // Add declarations for the composite block functions
    static void createCompositeBlock(AcDbDatabase* pDb, const wchar_t* blockName, const std::vector<std::wstring>& panelNames, const std::vector<AcGePoint3d>& positions, const std::vector<double>& rotations);
    static void insertCompositeBlock(AcDbDatabase* pDb, const wchar_t* blockName, const AcGePoint3d& insertPoint);
};
