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
};