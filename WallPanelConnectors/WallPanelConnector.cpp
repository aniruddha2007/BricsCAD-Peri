// Created by: Ani (2024-05-31)
// Modified by: Ani (2024-07-01)
// TODO:
// WallPanelConnector.cpp
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

const double TOLERANCE = 0.1;  // Define a small tolerance for angle comparisons

// List of panels with three connectors
const std::vector<std::wstring> panelsWithThreeConnectors = {
    ASSET_128285, ASSET_128280, ASSET_128283,
    ASSET_128281, ASSET_128284, ASSET_128282
};

// List of panels with two connectors
const std::vector<std::wstring> panelsWithTwoConnectors = {
    ASSET_129840, ASSET_129838, ASSET_129842,
    ASSET_129841, ASSET_129839, ASSET_129837
};

// GET WALL PANEL POSITIONS
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> WallPanelConnector::getWallPanelPositions() {
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> positions;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return positions;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return positions;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return positions;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return positions;
    }

    int entityCount = 0;
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        entityCount++;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbBlockReference::desc())) {
                AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
                if (pBlockRef) {
                    AcDbObjectId blockId = pBlockRef->blockTableRecord();
                    AcDbBlockTableRecord* pBlockDef;
                    if (acdbOpenObject(pBlockDef, blockId, AcDb::kForRead) == Acad::eOk) {
                        AcString blockName;
                        pBlockDef->getName(blockName);
                        std::wstring blockNameStr(blockName.kACharPtr());
                        blockNameStr = toUpperCase(blockNameStr);

                        // Compare with assets list
                        if (std::find(panelsWithThreeConnectors.begin(), panelsWithThreeConnectors.end(), blockNameStr) != panelsWithThreeConnectors.end() ||
                            std::find(panelsWithTwoConnectors.begin(), panelsWithTwoConnectors.end(), blockNameStr) != panelsWithTwoConnectors.end()) {
                            positions.emplace_back(pBlockRef->position(), blockNameStr, pBlockRef->rotation());
                        }
                        pBlockDef->close();
                    }
                }
            }
            pEnt->close();
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    return positions;
}

// CALCULATE CONNECTOR POSITIONS
std::vector<std::tuple<AcGePoint3d, double>> WallPanelConnector::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double>> connectorPositions;

    double zOffsets[] = { 22.5, 52.5, 97.5 }; // Predefined Z-axis positions for connectors
    double yOffset = 5.0;

    for (const auto& panelPosition : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPosition);
        std::wstring panelName = std::get<1>(panelPosition);
        double panelRotation = std::get<2>(panelPosition);

        int connectorCount = (std::find(panelsWithTwoConnectors.begin(), panelsWithTwoConnectors.end(), panelName) != panelsWithTwoConnectors.end()) ? 2 : 3;

        for (int i = 0; i < connectorCount; ++i) {
            AcGePoint3d connectorPos = pos;
            connectorPos.z += zOffsets[i];

            // Adjust positions based on the rotation and apply the Y-axis offset
            switch (static_cast<int>(round(panelRotation / M_PI_2))) {
            case 0: // 0 degrees
            case 4: // Normalize 360 degrees to 0 degrees
                connectorPos.y -= yOffset;
                break;
            case 1: // 90 degrees
                connectorPos.x += yOffset;
                break;
            case 2: // 180 degrees
                connectorPos.y += yOffset;
                break;
            case 3: // 270 degrees
            case -1: // Normalize -90 degrees to 270 degrees
                connectorPos.x -= yOffset;
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle detected: %f "), panelRotation);
                continue;
            }

            connectorPositions.emplace_back(std::make_tuple(connectorPos, panelRotation));
        }
    }

    return connectorPositions;
}

// LOAD CONNECTOR ASSET
AcDbObjectId WallPanelConnector::loadConnectorAsset(const wchar_t* blockName) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        acutPrintf(_T("\nBlock not found: %s"), blockName);
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    return blockId;
}

// PLACE CONNECTORS
void WallPanelConnector::placeConnectors() {
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(_T("\nNo wall panels detected."));
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId assetId = loadConnectorAsset(ASSET_128247.c_str());

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (const auto& connector : connectorPositions) {
        placeConnectorAtPosition(std::get<0>(connector), std::get<1>(connector), assetId);
    }
}

// PLACE CONNECTOR AT POSITION
void WallPanelConnector::placeConnectorAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return;
    }

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(assetId);
    pBlockRef->setRotation(rotation);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

    if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
        // Connector placed successfully
    }
    else {
        acutPrintf(_T("\nFailed to place connector."));
    }

    pBlockRef->close();
    pModelSpace->close();
    pBlockTable->close();
}
