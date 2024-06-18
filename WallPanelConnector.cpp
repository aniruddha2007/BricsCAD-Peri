#include "StdAfx.h"
#include "WallPanelConnector.h"
#include "SharedDefinations.h"  // For M_PI constants and asset definitions
#include <vector>
#include <tuple>
#include <array>
#include "dbapserv.h"        // For acdbHostApplicationServices() and related services
#include "dbents.h"          // For AcDbBlockReference
#include "dbsymtb.h"         // For block table record definitions
#include "AcDb.h"            // General database definitions

// Array of wall panel asset names
const std::array<const std::wstring, 14> wallPanelAssets = {
    ASSET_128280, ASSET_128285, ASSET_128286, ASSET_128281,
    ASSET_128283, ASSET_128284, ASSET_129837, ASSET_129838,
    ASSET_129839, ASSET_129840, ASSET_129841, ASSET_129842,
    ASSET_129864, ASSET_128282
};

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
                        for (const auto& asset : wallPanelAssets) {
                            if (wcscmp(blockName, asset.c_str()) == 0) {
                                positions.push_back(std::make_tuple(pBlockRef->position(), asset));
                                break;
                            }
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

    double offsets[] = { 22.5, 52.5, 97.5 };

    for (size_t i = 0; i < panelPositions.size() - 1; ++i) {
        AcGePoint3d pos = std::get<0>(panelPositions[i]);
        AcGePoint3d nextPos = std::get<0>(panelPositions[i + 1]);

        if (pos.distanceTo(nextPos) > 140.0) { // Panels are not adjacent
            continue;
        }

        AcGeVector3d direction = (nextPos - pos).normal();
        double rotation = atan2(direction.y, direction.x);

        for (double offset : offsets) {
            AcGePoint3d connectorPos = pos;
            AcGePoint3d connectorPosEnd = nextPos;
            connectorPos.z = pos.z + offset;
            connectorPosEnd.z = nextPos.z + offset;

            // Apply y-axis or x-axis offset based on rotation
            if (std::abs(rotation) < 1e-3 || std::abs(rotation - M_PI) < 1e-3) {
                connectorPos.y -= 5;
                connectorPosEnd.y -= 5;
            }
            else {
                connectorPos.x -= 5;
                connectorPosEnd.x -= 5;
            }

            connectorPositions.emplace_back(connectorPos, connectorPosEnd, rotation);
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
    AcDbObjectId assetId = loadConnectorAsset(ASSET_128247.c_str());  // Use the actual block name

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
