// Created by: Ani (2024-07-16)
// Modified by: 
// TODO: 
// StackedWallPanelConnector.cpp
/////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "WallPanelConnector.h"
#include "SharedDefinations.h"  // For M_PI constants
#include "DefineScale.h"       // For globalVarScale
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>            // For std::transform
#include "dbapserv.h"        // For acdbHostApplicationServices() and related services
#include "dbents.h"          // For AcDbBlockReference
#include "dbsymtb.h"         // For block table record definitions
#include "AcDb.h"            // General database definitions

//const double TOLERANCE = 0.1; // Tolerance for comparing double values
//
//const std::vector<std::wstring> panelNames = {
//	ASSET_128285,
//	ASSET_128280,
//	ASSET_129840,
//	ASSET_129838,
//	ASSET_128283,
//	ASSET_128281,
//	ASSET_129842,
//	ASSET_129841,
//	ASSET_129839,
//	ASSET_129837,
//	ASSET_128284,
//	ASSET_128282,
//	ASSET_129879,
//	ASSET_129884,
//	ASSET_128287,
//	ASSET_128292, 
//	ASSET_136096 };
//
////Calculate the position of the connectors for the stacked wall panels
//std::vector<std::tuple<AcGePoint3d, double>> WallPanelConnector::calculateVerticalConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
//	std::vector<std:tuple<AcGePoint3d, double>> connectorPositions;
//
//	double zOffsets[] = { 60, 135 }; // Z offsets for the connectors
//	double yOffset = 5.0; // Y offset for the connectors
//	double connectorRotation = M_PI_2; // Rotation for the connectors
//}