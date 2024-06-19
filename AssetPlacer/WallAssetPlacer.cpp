// Created by:Ani  (2024-05-31)
// WallAssetPlacer.cpp

#include "StdAfx.h"
#include "WallAssetPlacer.h"
#include "SharedDefinations.h"
#include "GeometryUtils.h"
#include <vector>
#include <limits>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include "aced.h"
#include <cmath>
#include "acutads.h"
#include "acdocman.h"
#include "rxregsvc.h"
#include "geassign.h"
#include <string>
#include "SharedDefinations.h"

// Structure to hold panel information
struct Panel {
    int length;
    std::wstring id;
};

std::vector<AcGePoint3d> WallPlacer::detectPolylines() {
    acutPrintf(_T("\nDetecting polylines..."));
    std::vector<AcGePoint3d> corners;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return corners;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return corners;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return corners;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return corners;
    }

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            acutPrintf(_T("\nEntity type: %s"), pEnt->isA()->name());
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPolyline = AcDbPolyline::cast(pEnt);
                if (pPolyline) {
                    processPolyline(pPolyline, corners);
                }
            }
            pEnt->close();
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;
}

AcDbObjectId WallPlacer::loadAsset(const wchar_t* blockName) {
    acutPrintf(_T("\nLoading asset: %s"), blockName);
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) return AcDbObjectId::kNull;

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) return AcDbObjectId::kNull;

    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    acutPrintf(_T("\nLoaded block: %s"), blockName);
    return blockId;
}

void WallPlacer::placeWallSegment(const AcGePoint3d& start, const AcGePoint3d& end) {
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

    
    //TODO
    // Use the biggest block size to calculate the number of panels and empty space should be iterated  over using smaller block size
	double distance = start.distanceTo(end)-50;
    AcGeVector3d direction = (end - start).normal();
    AcGePoint3d currentPoint = start + direction * 25;

    // List of available panels
    std::vector<Panel> panelSizes = {
        {90, L"128280X"},
        {75, L"128281X"},
        {60, L"128282X"},
        {45, L"128283X"},
        {30, L"128284X"},
        {15, L"128285X"}
    };

    //Iterate through every panel type
    for (const auto& panel : panelSizes) {
        acutPrintf(_T("\npanelSizes test length: %d"), panel.length);
        acutPrintf(_T("\npanelSizes test id: %s"), panel.id);

        AcDbObjectId assetId = loadAsset(panel.id.c_str());

        if (assetId == AcDbObjectId::kNull) {
            acutPrintf(_T("\nFailed to load asset."));
        }
        else
        {
            //Place walls
            int numPanels = static_cast<int>(distance / panel.length);  // Calculate the number of panels

            for (int i = 0; i < numPanels; i++) {
                double rotation = atan2(direction.y, direction.x);

                // Place the wall segment without scaling
                AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                pBlockRef->setPosition(currentPoint);
                pBlockRef->setBlockTableRecord(assetId);
                pBlockRef->setRotation(rotation);  // Apply rotation
                pBlockRef->setScaleFactors(AcGeScale3d(0.1, 0.1, 0.1));  // Ensure no scaling

                if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
                    acutPrintf(_T("\nWall segment placed successfully."));
                }
                else {
                    acutPrintf(_T("\nFailed to place wall segment."));
                }
                pBlockRef->close();  // Decrement reference count

                currentPoint += direction * panel.length;  // Move to the next panel
                distance -= panel.length;
                /*
                if (currentPoint.distanceTo(end) < panel.length) {
                    break;  // Stop if the remaining distance is less than a panel length
                }
                */
            }
        }
    }

    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}

void WallPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
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

    AcDbText* pText = new AcDbText(position, text, AcDbObjectId::kNull, 0.2, 0);
    if (pModelSpace->appendAcDbEntity(pText) == Acad::eOk) {
        acutPrintf(_T("\nAdded text annotation: %s"), text);
    }
    else {
        acutPrintf(_T("\nFailed to add text annotation."));
    }
    pText->close();  // Decrement reference count

    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}

void WallPlacer::placeWalls() {
    acutPrintf(_T("\nPlacing walls..."));
    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo polylines detected."));
        return;
    }

    for (size_t i = 0; i < corners.size() - 1; ++i) {
        placeWallSegment(corners[i], corners[i + 1]);
    }

    /*
    AcDbObjectId assetId = loadAsset(L"128280X");

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (size_t i = 0; i < corners.size() - 1; ++i) {
        placeWallSegment(corners[i], corners[i + 1], assetId);
    }
    */

    acutPrintf(_T("\nCompleted placing walls."));
}
