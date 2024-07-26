#pragma once

#include <vector>
#include <map>
#include "gept3dar.h"  // For AcGePoint3d
#include "dbsymtb.h"   // For AcDbObjectId

class CornerAssetPlacer {
public:
    // Public method to initiate asset placement at corners
    static void placeAssetsAtCorners();
    // Public method to identify walls, ensuring declaration matches definition
    static void identifyWalls();

private:
    // Method to detect polylines in the drawing
    static std::vector<AcGePoint3d> detectPolylines();
    // Method to load an asset block from the block table
    static AcDbObjectId loadAsset(const wchar_t* blockName);
    // Method to place an asset at a specific corner with a given rotation
    static void placeAssetAtCorner(const AcGePoint3d& corner, double rotation, AcDbObjectId assetId);
    // Method to place corner post and panels (Inside corner)
    static void placeInsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelId);
    // Method to place corner post and panels (Outside corner)
    static void placeOutsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelId, AcDbObjectId panel75Id);
    // Method to add text annotation at a specific position
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);

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
