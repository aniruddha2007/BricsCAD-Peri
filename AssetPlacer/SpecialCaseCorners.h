#pragma once

#include <vector>
#include <map>
#include "gept3dar.h"  // For AcGePoint3d
#include "dbsymtb.h"   // For AcDbObjectId

class SpecialCaseCorners {
public:
	// Public method to initiate asset placement at positions
	static void handleSpecialCases();

private:
	// Method to detect polylines in the drawing
	static std::vector<AcGePoint3d> detectPolylines();
	// Method to load an asset block from the block table
	static AcDbObjectId loadAsset(const wchar_t* blockName);
	// method to Determine setup based on distance and place panels accordingly
	static void determineAndPlacePanels(const std::vector<AcGePoint3d>& positions);
	// Method to place an asset at a specific corner with a given rotation
	static void placeAssetAtCorner(const AcGePoint3d& corner, double rotation, AcDbObjectId assetId);
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