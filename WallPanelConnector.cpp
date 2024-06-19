#include "StdAfx.h"
#include "WallPanelConnector.h"
#include "SharedDefinations.h"  // For M_PI constants
#include <vector>
#include <tuple>
#include <cmath>
#include "dbapserv.h"        // For acdbHostApplicationServices() and related services
#include "dbents.h"          // For AcDbBlockReference
#include "dbsymtb.h"         // For block table record definitions
#include "AcDb.h"            // General database definitions

const double TOLERANCE = 0.1;  // Define a small tolerance for angle comparisons

// DETECT WALL PANEL POSITIONS
std::vector<std::tuple<AcGePoint3d, std::wstring>> WallPanelConnector::detectWallPanelPositions() {
    std::vector<std::tuple<AcGePoint3d, std::wstring>> positions;

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
                            positions.emplace_back(pBlockRef->position(), blockNameStr);
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
std::vector<std::tuple<AcGePoint3d, AcGePoint3d, double>> WallPanelConnector::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, AcGePoint3d, double>> connectorPositions;

    double offsets[] = { 22.5, 52.5, 97.5 }; // Predefined Z-axis positions for connectors

    for (size_t i = 0; i < panelPositions.size() - 1; ++i) {
        AcGePoint3d pos = std::get<0>(panelPositions[i]);
        AcGePoint3d nextPos = std::get<0>(panelPositions[i + 1]);
        AcGeVector3d direction = (nextPos - pos).normal();
        double rotation = atan2(direction.y, direction.x);

        // Normalize the angle to be between 0 and 2π
        if (rotation < 0) {
            rotation += 2 * M_PI;
        }

        // Determine the rotation based on the angle within the tolerance
        if (fabs(rotation - 0) < TOLERANCE || fabs(rotation - 2 * M_PI) < TOLERANCE) {
            rotation = 0.0;  // Horizontal right
        }
        else if (fabs(rotation - M_PI_2) < TOLERANCE) {
            rotation = M_PI_2;  // Vertical up
        }
        else if (fabs(rotation - M_PI) < TOLERANCE) {
            rotation = M_PI;  // Horizontal left
        }
        else if (fabs(rotation - 3 * M_PI_2) < TOLERANCE) {
            rotation = 3 * M_PI_2;  // Vertical down
        }

        // Calculate connector positions with the updated rotation logic
        for (double offset : offsets) {
            AcGePoint3d connectorPos = pos;
            AcGePoint3d connectorPosEnd = nextPos;

            // Adjust positions based on the rotation and apply the Y-axis offset
            if (rotation == 0.0 || rotation == M_PI) {
                connectorPos.z = pos.z + offset;
                connectorPos.y = pos.y - 5.0;
                connectorPosEnd.z = nextPos.z + offset;
                connectorPosEnd.y = nextPos.y - 5.0;
            }
            else if (rotation == M_PI_2 || rotation == 3 * M_PI_2) {
                connectorPos.z = pos.z + offset;
                connectorPos.x = pos.x - 5.0;
                connectorPosEnd.z = nextPos.z + offset;
                connectorPosEnd.x = nextPos.x - 5.0;
            }

            // Print debug information
            acutPrintf(_T("\nConnector calculated:\n"));
            acutPrintf(_T("Start Position: (%f, %f, %f)\n"), connectorPos.x, connectorPos.y, connectorPos.z);
            acutPrintf(_T("End Position: (%f, %f, %f)\n"), connectorPosEnd.x, connectorPosEnd.y, connectorPosEnd.z);
            acutPrintf(_T("Rotation: %f radians\n"), rotation);

            connectorPositions.emplace_back(std::make_tuple(connectorPos, connectorPosEnd, rotation));
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
    std::vector<std::tuple<AcGePoint3d, std::wstring>> panelPositions = detectWallPanelPositions();
    std::vector<std::tuple<AcGePoint3d, AcGePoint3d, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId assetId = loadConnectorAsset(ASSET_128247.c_str());  // Replace with the actual block name

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (const auto& connector : connectorPositions) {
        placeConnectorAtPosition(std::get<0>(connector), std::get<2>(connector), assetId);
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
    pBlockRef->setScaleFactors(AcGeScale3d(0.1, 0.1, 0.1));  // Ensure no scaling

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
