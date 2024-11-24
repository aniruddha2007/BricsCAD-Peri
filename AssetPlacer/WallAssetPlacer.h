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

#ifndef POLYLINE_INFO_H
#define POLYLINE_INFO_H

#include <vector>
#include <AcDb.h>

struct PolylineInfo {
	AcDbPolyline* polyline;
	std::vector<AcGePoint3d> corners;
	bool isOuterLoop;
};

#endif // POLYLINE_INFO_H


class WallPlacer {
public:
    static void placeWalls();
	void detectPolylines(std::vector<PolylineInfo>& polylineInfos, double angleThreshold = 90.0, double tolerance = 1e-6);

private:
    
    static AcDbObjectId loadAsset(const wchar_t* blockName);
    static void placeWallSegment(const AcGePoint3d& start, const AcGePoint3d& end, const AcGeVector3d& direction, bool isOuter);
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);
	//static void WallPlacer::adjustStartAndEndPoints(AcGePoint3d& point, const AcGeVector3d& direction, double distanceBetweenPolylines, bool isInner);
	static double calculateDistanceBetweenPolylines();
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
// Path: WallPlacer.cpp