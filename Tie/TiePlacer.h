#pragma once
#include <vector>
#include <tuple>
#include <string>
#include "gept3dar.h"  // For AcGePoint3d
#include "dbid.h"   // For AcDbObjectId

class TiePlacer {
public:
		static void placeTies();
		static void placeTie(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions);
private:
	static std::vector<AcGePoint3d> detectPolylines();
	static std::vector<std::tuple<AcGePoint3d, std::wstring, double>> getWallPanelPositions();
	static AcDbObjectId LoadTieAsset(const wchar_t* blockName);
	static void placeTieAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId);

	//Add other helper functions here

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

	// Static member to hold the wall mapping
	static std::map<AcGePoint3d, std::vector<AcGePoint3d>, Point3dComparator> wallMap;

};