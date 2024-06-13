#pragma once
#include <vector>
#include "gept3dar.h"  // For AcGePoint3d
#include "dbidmap.h"   // For AcDbObjectId

class WallPanelConnector {
public:
    static void placeConnectors();
private:
    static std::vector<AcGePoint3d> detectWallPanelPositions();
    static std::vector<std::tuple<AcGePoint3d, AcGePoint3d, double>> calculateConnectorPositions(const std::vector<AcGePoint3d>& panelPositions);
    static AcDbObjectId loadConnectorAsset(const wchar_t* blockName);
    static void placeConnectorAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId);

    // Comparator for AcGePoint3d
    struct Point3dComparator {
        bool operator()(const AcGePoint3d& lhs, const AcGePoint3d& rhs) const {
            if (lhs.x != rhs.x)
                return lhs.x < rhs.x;
            if (lhs.y != rhs.y)
                return lhs.y < rhs.y;
            return lhs.z < rhs.z;
        }
    };
};
