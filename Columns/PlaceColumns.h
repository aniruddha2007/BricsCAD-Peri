// Created: by Daniel (2024-07-24)
// PlaceColumns.h

#pragma once
#include <vector>
#include "gepnt3d.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

class ColumnPlacer {
public:
    static void placeColumns();

private:
    static AcDbObjectId loadAsset(const wchar_t* blockName);
    static void placeColumn(const AcGePoint3d& position, const int width, const int length);
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);

    struct Point3dComparator {
        bool operator()(const AcGePoint3d& lhs, const AcGePoint3d& rhs) const {
            if (lhs.x != rhs.x)
                return lhs.x < rhs.x;
            if (lhs.y != rhs.y)
                return lhs.y < rhs.y;
            return lhs.z < rhs.z;
        }
    };

    static std::map<AcGePoint3d, std::vector<AcGePoint3d>, Point3dComparator> columnMap;
};