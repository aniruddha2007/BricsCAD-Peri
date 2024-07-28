#pragma once

#include "geassign.h"
#include <vector>
#include <tuple>
#include <string>
#include "dbmain.h"

class WalerConnector {
public:
    static std::vector<std::tuple<AcGePoint3d, std::wstring, double>> getWallPanelPositions();
    static std::vector<std::tuple<AcGePoint3d, double, std::wstring>> calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions);
    static AcDbObjectId loadConnectorAsset(const wchar_t* blockName);
    static void placeConnectors();
    static void placeConnectorAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId);
};
