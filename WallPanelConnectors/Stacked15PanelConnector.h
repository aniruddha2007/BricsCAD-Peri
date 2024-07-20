#pragma once
#include <vector>
#include <tuple>
#include <string>
#include "gept3dar.h"  // For AcGePoint3d
#include "dbid.h"   // For AcDbObjectId

// Declare the function to get the width of the panel based on its name
double get15Panel(const std::wstring& panelName);

class Stacked15PanelConnector {
public:
	static void place15panelConnectors();
private:
	static std::vector<std::tuple<AcGePoint3d, std::wstring, double>> getWallPanelPositions();
	static std::vector<std::tuple<AcGePoint3d, double, double, double, double>> calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions);
	static AcDbObjectId loadConnectorAsset(const wchar_t* blockName);
	static void placeConnectorAtPosition(const AcGePoint3d& position, double rotationX, double rotationY, double rotationZ, double panelRotation, AcDbObjectId assetId);
};