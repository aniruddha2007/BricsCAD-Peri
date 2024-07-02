// Created by:Ani  (2024-05-31)
// Modified by:Ani (2024-06-01)
// TODO: Missing side for rectangle
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
#include "DefineHeight.h"
#include "DefineScale.h"

// Structure to hold panel information
struct Panel {
    int length;
    std::wstring id[2];
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

    
    // Use the biggest block size to calculate the number of panels and empty space should be iterated  over using smaller block size
	double distance = start.distanceTo(end)-50;
    AcGeVector3d direction = (end - start).normal();
    AcGePoint3d currentPoint = start + direction * 25;

    //Fetch this variable from DefineHeight
    int wallHeight = globalVarHeight;

    int currentHeight = 0;
    int panelHeights[] = { 135, 60 };

    // List of available panels
    std::vector<Panel> panelSizes = {
        {90, {L"128280X", L"129837X"}},
        {75, {L"128281X", L"129838X"}},
        {60, {L"128282X", L"129839X"}},
        {45, {L"128283X", L"129840X"}},
        {30, {L"128284X", L"129841X"}},
        {15, {L"128285X", L"129842X"}}
    };

    //Iterate through every panel type
    for (const auto& panel : panelSizes) {
        currentHeight = 0;
        AcGePoint3d backupCurrentPoint = currentPoint;
        double backupDistance = distance;

        //Iterate through 135 and 60 height
        for (int panelNum = 0; panelNum < 2; panelNum++) {
            currentPoint = backupCurrentPoint;
            distance = backupDistance;
            AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());

            if (assetId == AcDbObjectId::kNull) {
                acutPrintf(_T("\nFailed to load asset."));
            }
            else
            {
                acutPrintf(_T("\nwallHeight: %d,"), wallHeight);
                acutPrintf(_T(" currentHeight: %d,"), currentHeight);
                acutPrintf(_T(" panelHeight num: %d,"), panelNum);
                acutPrintf(_T(" panelHeight: %d"), panelHeights[panelNum]);

                int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);  // Calculate the number of panels that fit vertically

                for (int x = 0; x < numPanelsHeight; x++) {
                    currentPoint = backupCurrentPoint;
                    distance = backupDistance;

                    //Place walls
                    int numPanels = static_cast<int>(distance / panel.length);  // Calculate the number of panels that fit horizontaly
                    int numOfWallSegmentsPlaced = 0;
                    for (int i = 0; i < numPanels; i++) {
                        double rotation = atan2(direction.y, direction.x);

                        // Place the wall segment without scaling
                        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                        AcGePoint3d currentPointWithHeight = currentPoint;
                        currentPointWithHeight.z += currentHeight;
                        pBlockRef->setPosition(currentPointWithHeight);
                        pBlockRef->setBlockTableRecord(assetId);
                        pBlockRef->setRotation(rotation);  // Apply rotation
                        pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

                        if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
                            //acutPrintf(_T("\nWall segment placed successfully."));
                            numOfWallSegmentsPlaced += 1;
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
                    acutPrintf(_T("\n%d wall segments placed successfully."), numOfWallSegmentsPlaced);
                    //Check if panel height fits in required wall height
                    /*if (wallHeight - currentHeight >= panelHeights[panelNum]) {

                    }*/
                    currentHeight += panelHeights[panelNum];
                }
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
