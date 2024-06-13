// Created: by Ani (2024-05-31)
// WallAssetPlacer.h

#pragma once
#include <vector>
#include "gepnt3d.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

class WallPlacer {
public:
    static void placeWalls();

private:
    static std::vector<AcGePoint3d> detectPolylines();
    static AcDbObjectId loadAsset(const wchar_t* blockName);
    static void placeWallSegment(const AcGePoint3d& start, const AcGePoint3d& end, AcDbObjectId assetId);
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);
};
// Path: WallPlacer.cpp