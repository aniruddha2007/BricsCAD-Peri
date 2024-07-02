#include "StdAfx.h"
#include "WallPanelConnector.h"
#include "SharedDefinations.h"  // For M_PI constants
#include "DefineScale.h"       // For globalVarScale
#include <vector>
#include <tuple>
#include <cmath>
#include "dbapserv.h"        // For acdbHostApplicationServices() and related services
#include "dbents.h"          // For AcDbBlockReference
#include "dbsymtb.h"         // For block table record definitions
#include "AcDb.h"            // General database definitions

const double TOLERANCE = 0.1;  // Define a small tolerance for angle comparisons

const std::vector<std::wstring> panelsWithTwoConnectors = {
    L"129864", L"129840", L"129838", L"129842", L"129841",
    L"129839", L"129837", L"129879", L"129884"
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

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbBlockReference::desc())) {
                AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
                if (pBlockRef) {
                    AcDbObjectId blockId = pBlockRef->blockTableRecord();
                    AcDbBlockTableRecord* pBlockDef;
                    if (acdbOpenObject(pBlockDef, blockId, AcDb::kForRead) == Acad::eOk) {
                        const wchar_t* blockName;
                        pBlockDef->getName(blockName);

                        // Compare with assets list
                        std::wstring blockNameStr(blockName);
                        if (blockNameStr == ASSET_128280 || blockNameStr == ASSET_128285 ||
                            blockNameStr == ASSET_128286 || blockNameStr == ASSET_128281 ||
                            blockNameStr == ASSET_128283 || blockNameStr == ASSET_128284 ||
                            blockNameStr == ASSET_129837 || blockNameStr == ASSET_129838 ||
                            blockNameStr == ASSET_129839 || blockNameStr == ASSET_129840 ||
                            blockNameStr == ASSET_129841 || blockNameStr == ASSET_129842 ||
                            blockNameStr == ASSET_129864 || blockNameStr == ASSET_128282) {
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

        int connectorCount = (std::find(panelsWithTwoConnectors.begin(), panelsWithTwoConnectors.end(), panelName) != panelsWithTwoConnectors.end()) ? 2 : 3;  // Only 2 connectors for 60* panels

        for (int i = 0; i < connectorCount; ++i) {
            AcGePoint3d connectorPos = pos;
            connectorPos.z += zOffsets[i];

            // Adjust positions based on the rotation and apply the Y-axis offset
            switch (static_cast<int>(round(panelRotation / M_PI_2))) {
            case 0: // 0 degrees
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
                acutPrintf(_T("\nInvalid rotation angle detected."));
                continue;
            }

            // Print debug information
            acutPrintf(_T("\nConnector calculated:\n"));
            acutPrintf(_T("Position: (%f, %f, %f)\n"), connectorPos.x, connectorPos.y, connectorPos.z);
            acutPrintf(_T("Panel: %s\n"), panelName.c_str());
            acutPrintf(_T("Rotation: %f radians\n"), panelRotation);

            connectorPositions.emplace_back(std::make_tuple(connectorPos, panelRotation));
        }
    }

    return connectorPositions;
}

// LOAD CONNECTOR ASSET
AcDbObjectId WallPanelConnector::loadConnectorAsset(const wchar_t* blockName) {
    acutPrintf(_T("\nLoading asset: %s"), blockName);
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
    acutPrintf(_T("\nLoaded block: %s"), blockName);
    return blockId;
}

// PLACE CONNECTORS
void WallPanelConnector::placeConnectors() {
    acutPrintf(_T("\nPlacing connectors..."));
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    std::vector<std::tuple<AcGePoint3d, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId assetId = loadConnectorAsset(ASSET_128247.c_str());  // Replace with the actual block name

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (const auto& connector : connectorPositions) {
        placeConnectorAtPosition(std::get<0>(connector), std::get<1>(connector), assetId);
    }

    acutPrintf(_T("\nCompleted placing connectors."));
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
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

    if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
        acutPrintf(_T("\nConnector placed successfully."));
    }
    else {
        acutPrintf(_T("\nFailed to place connector."));
    }

    pBlockRef->close();  // Decrement reference count
    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}
