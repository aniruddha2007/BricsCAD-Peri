#pragma once

#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include "aced.h"
#include "acutads.h"
#include "acdocman.h"
#include "rxregsvc.h"
#include "geassign.h"
#include <string>
#include <vector>
#include "gepnt3d.h"

struct BlockInfo {
    std::wstring blockName;
    AcGePoint3d position;
    double rotation;
};

class PlaceBracket {
public:
    static std::vector<std::tuple<AcGePoint3d, std::wstring, double>> getWallPanelPositions();
    static AcDbObjectId loadAsset(const wchar_t* blockName);
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);
    static void placeAsset(const AcGePoint3d& position, const wchar_t* blockName, double rotation = 0.0, double scale = 1.0);
    static void placeBrackets();

private:
    static std::vector<AcGePoint3d> detectPolylines();

    // Comparator for AcGePoint3d to be used in the map
    struct Point3dComparator {
        bool operator()(const AcGePoint3d& lhs, const AcGePoint3d& rhs) const {
            if (lhs.x != rhs.x)
                return lhs.x < rhs.x;
            if (lhs.y != rhs.y)
                return lhs.y < rhs.y;
            return lhs.z < rhs.z;
        }
    };

    // Static member to hold the wall mapping
    static std::map<AcGePoint3d, std::vector<AcGePoint3d>, Point3dComparator> wallMap;
};