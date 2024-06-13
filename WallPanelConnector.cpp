#include "StdAfx.h"
#include "WallPanelConnector.h"
#include "SharedDefinations.h"  // For M_PI constants
#include <vector>
#include <tuple>
#include "dbapserv.h"        // For acdbHostApplicationServices() and related services
#include "dbents.h"          // For AcDbBlockReference
#include "dbsymtb.h"         // For block table record definitions
#include "AcDb.h"            // General database definitions

// DETECT WALL PANEL POSITIONS
std::vector<AcGePoint3d> WallPanelConnector::detectWallPanelPositions() {
    std::vector<AcGePoint3d> positions;

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
                        if (wcscmp(blockName, L"128280X") == 0 || wcscmp(blockName, L"128286X") == 0 || wcscmp(blockName, L"128285X") == 0) {
                            positions.push_back(pBlockRef->position());
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
std::vector<std::tuple<AcGePoint3d, AcGePoint3d, double>> WallPanelConnector::calculateConnectorPositions(const std::vector<AcGePoint3d>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, AcGePoint3d, double>> connectorPositions;

    double panelHeight = 135.0;
    double offsets[] = { 37.5, 82.5, 112.5 };

    for (size_t i = 0; i < panelPositions.size() - 1; ++i) {
        AcGePoint3d pos = panelPositions[i];
        AcGePoint3d nextPos = panelPositions[i + 1];
        AcGeVector3d direction = (nextPos - pos).normal();
        double rotation = atan2(direction.y, direction.x);
        double angle = rotation;

        // Ensure connectors are only placed between adjacent panels
        if (pos.distanceTo(nextPos) < 140.0) { // Adjust this value if needed
            for (double offset : offsets) {
                AcGePoint3d connectorPos = pos;
                AcGePoint3d connectorPosEnd = nextPos;
                if (std::abs(angle) < 1e-3 || std::abs(angle - M_PI) < 1e-3) {
                    connectorPos.z = pos.z + offset;
                    connectorPos.x = pos.x;
                    connectorPos.y = pos.y - 4.53;
                    connectorPosEnd.z = nextPos.z + offset;
                    connectorPosEnd.x = nextPos.x;
                    connectorPosEnd.y = nextPos.y - 4.53;
                }
                else {
                    connectorPos.z = pos.z + offset;
                    connectorPos.x = pos.x - 4.53;
                    connectorPos.y = pos.y;
                    connectorPosEnd.z = nextPos.z + offset;
                    connectorPosEnd.x = nextPos.x - 4.53;
                    connectorPosEnd.y = nextPos.y;
                }
                connectorPositions.emplace_back(std::make_tuple(connectorPos, connectorPosEnd, rotation));
            }
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
    std::vector<AcGePoint3d> panelPositions = detectWallPanelPositions();
    std::vector<std::tuple<AcGePoint3d, AcGePoint3d, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId assetId = loadConnectorAsset(L"128247X");  // Replace with the actual block name

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
