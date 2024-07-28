#include "StdAfx.h"
#include "WalerConnector.h"
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

// List of panels to be handled
const std::vector<std::wstring> panelsToHandle = {
    ASSET_128292, ASSET_129884
};

// GET WALL PANEL POSITIONS
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> WalerConnector::getWallPanelPositions() {
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
                        const wchar_t* blockName;
                        pBlockDef->getName(blockName);
                        std::wstring blockNameStr(blockName);
                        blockNameStr = toUpperCase(blockNameStr);

                        // Compare with assets list
                        if (blockNameStr == ASSET_128292 || blockNameStr == ASSET_129884) {
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
std::vector<std::tuple<AcGePoint3d, double, std::wstring>> WalerConnector::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double, std::wstring>> connectorPositions;

    // Offsets for connectors
    double zOffsets[] = { 30.0, 105.0 }; // Predefined Z-axis positions for connectors
    double xOffset_128255 = 2.5;
    double yOffset_128255 = -10.0;
    double xOffset_128293A = -15.0;
    double yOffset_128293A = -10.0;
    double xOffset_128293B = 25.0;
    double yOffset_128293B = -10.0;

    for (const auto& panelPosition : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPosition);
        std::wstring panelName = std::get<1>(panelPosition);
        double panelRotation = std::get<2>(panelPosition);

        int connectorSetCount = (panelName == ASSET_128292) ? 2 : 1; // Two sets of connectors for 128292, one set for 129884

        for (int set = 0; set < connectorSetCount; ++set) {
            for (int i = 0; i < 3; ++i) {  // Each set has 3 connectors: 1 of 128255 and 2 of 128293 (A and B)
                AcGePoint3d connectorPos = pos;
                connectorPos.z += zOffsets[set];  // Different zOffset for each set

                double xOffset, yOffset;
                std::wstring connectorName;
                if (i == 0) {
                    xOffset = xOffset_128255;
                    yOffset = yOffset_128255;
                    connectorName = ASSET_128255;
                }
                else if (i == 1) {
                    xOffset = xOffset_128293A;
                    yOffset = yOffset_128293A;
                    connectorName = ASSET_128293;
                }
                else {
                    xOffset = xOffset_128293B;
                    yOffset = yOffset_128293B;
                    connectorName = ASSET_128293;
                }

                // Adjust positions based on the rotation and apply the X and Y axis offset
                switch (static_cast<int>(round(panelRotation / M_PI_2))) {
                case 0: // 0 degrees
                case 4: // Normalize 360 degrees to 0 degrees
                    connectorPos.x += xOffset;
                    connectorPos.y += yOffset;
                    break;
                case 1: // 90 degrees
                    connectorPos.x -= yOffset;
                    connectorPos.y += xOffset;
                    break;
                case 2: // 180 degrees
                    connectorPos.x -= xOffset;
                    connectorPos.y -= yOffset;
                    break;
                case 3: // 270 degrees
                case -1: // Normalize -90 degrees to 270 degrees
                    connectorPos.x += yOffset;
                    connectorPos.y -= xOffset;
                    break;
                default:
                    acutPrintf(_T("\nInvalid rotation angle detected: %f "), panelRotation);
                    continue;
                }

                connectorPositions.emplace_back(std::make_tuple(connectorPos, panelRotation, connectorName));
            }
        }
    }

    return connectorPositions;
}

// LOAD CONNECTOR ASSET
AcDbObjectId WalerConnector::loadConnectorAsset(const wchar_t* blockName) {
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
void WalerConnector::placeConnectors() {
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(_T("\nNo wall panels detected."));
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double, std::wstring>> connectorPositions = calculateConnectorPositions(panelPositions);

    for (const auto& connector : connectorPositions) {
        AcDbObjectId assetId = loadConnectorAsset(std::get<2>(connector).c_str());
        if (assetId == AcDbObjectId::kNull) {
            acutPrintf(_T("\nFailed to load asset: %s"), std::get<2>(connector).c_str());
            continue;
        }
        placeConnectorAtPosition(std::get<0>(connector), std::get<1>(connector), assetId);
    }
}

// PLACE CONNECTOR AT POSITION
void WalerConnector::placeConnectorAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId) {
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
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure scaling

    if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
        //acutPrintf(_T("\nConnector placed successfully.")); // Debug information
    }
    else {
        acutPrintf(_T("\nFailed to place connector."));
    }

    pBlockRef->close();
    pModelSpace->close();
    pBlockTable->close();
}
