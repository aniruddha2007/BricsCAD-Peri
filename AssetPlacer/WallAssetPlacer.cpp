// Created by:Ani  (2024-05-31)
// Modified by:Ani (2024-06-01)
// TODO:
// Move the *10 Compensator to the middle of the panel
// WallAssetPlacer.cpp
// This file contains the implementation of the WallPlacer class.
// The WallPlacer class is used to place wall segments in BricsCAD.
// The detectPolylines function is used to detect polylines in the drawing.
// The loadAsset function is used to load a block asset from the database.
// The placeWallSegment function is used to place a wall segment between two points.
// The addTextAnnotation function is used to add a text annotation at a given position.
// The placeWalls function is the main function that places the walls in the drawing.
// The placeWalls function is registered as a command in the BrxApp.cpp file.
// The placeWalls function is called when the PlaceWalls command is executed in BricsCAD.
// The placeWalls function is also added to the custom menu in the acrxEntryPoint.cpp file.
// The placeWalls function is part of the WallPlacer class.
/////////////////////////////////////////////////////////////////////////

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
#include <thread>
#include <chrono>

std::map<AcGePoint3d, std::vector<AcGePoint3d>, WallPlacer::Point3dComparator> WallPlacer::wallMap;

const int BATCH_SIZE = 10; // Batch size for processing entities

const double TOLERANCE = 0.1; // Tolerance for comparing angles

// Structure to hold panel information
struct Panel {
    int length;
    std::wstring id[2];
};

bool isInteger(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

//Detect polylines
std::vector<AcGePoint3d> WallPlacer::detectPolylines() {
    acutPrintf(_T("\nDetecting polylines..."));
    std::vector<AcGePoint3d> corners;
    wallMap.clear();  // Clear previous data

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return corners;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table. Error status: %d\n"), es);
        return corners;
    }

    AcDbBlockTableRecord* pModelSpace;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space. Error status: %d\n"), es);
        pBlockTable->close();
        return corners;
    }

    AcDbBlockTableRecordIterator* pIter;
    es = pModelSpace->newIterator(pIter);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator. Error status: %d\n"), es);
        pModelSpace->close();
        pBlockTable->close();
        return corners;
    }

    int entityCount = 0;
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        es = pIter->getEntity(pEnt, AcDb::kForRead);
        if (es == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPolyline = AcDbPolyline::cast(pEnt);
                if (pPolyline) {
                    processPolyline(pPolyline, corners, 90.0, TOLERANCE);  // Assuming 90.0 degrees as the threshold for corners
                }
            }
            pEnt->close();
            entityCount++;

            if (entityCount % BATCH_SIZE == 0) {
                acutPrintf(_T("\nProcessed %d entities. Pausing to avoid resource exhaustion.\n"), entityCount);
                std::this_thread::sleep_for(std::chrono::seconds(1));  // Pause for a moment
            }
        }
        else {
            acutPrintf(_T("\nFailed to get entity. Error status: %d\n"), es);
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;

}

// Load asset
AcDbObjectId WallPlacer::loadAsset(const wchar_t* blockName) {
    // acutPrintf(_T("\nLoading asset: %s"), blockName);
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
    // acutPrintf(_T(" Loaded block: %s"), blockName);
    return blockId;
}

// Add text annotation
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

// Place wall segment
void WallPlacer::placeWallSegment(const AcGePoint3d& start, const AcGePoint3d& end) {
    
}

// Place walls
void WallPlacer::placeWalls() {
    acutPrintf(_T("\nPlacing walls..."));
    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo polylines detected."));
        return;
    }

    int closeLoopCounter = -1;
    int loopIndex = 0;
    double outerPointCounter = corners[0].x;
    int outerLoopIndexValue = 0;

    // Loop through corners, find outer and inner loop
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        AcGeVector3d direction = (end - start).normal();

        acutPrintf(_T("\nCurrent position: %f, %f"), start.x, start.y); // Debug
        if (start.x > outerPointCounter) {
            outerPointCounter = start.x;
            outerLoopIndexValue = loopIndex;
        }

        acutPrintf(_T("\ndirection.y is integer?: %f,"), direction.y); // Debug
        acutPrintf(_T("\ndirection.x is integer?: %f,"), direction.x); // Debug
        if (isInteger(direction.x) && isInteger(direction.y)) {
            acutPrintf(_T("\nYES."));
        }
        else {
            acutPrintf(_T("\nNO. i < corners.size() - 1?"));
            if (cornerNum < corners.size() - 1) {
                acutPrintf(_T("\nYES."));
                closeLoopCounter = -1;
                loopIndex = 1;
            }
            else {
                acutPrintf(_T("\nNO."));
            }
        }
    }
    acutPrintf(_T("\nOuter loop is loop number: %d,"), outerLoopIndexValue); // Debug

    loopIndex = 0;
    int loopIndexLastPanel = 0;
    closeLoopCounter = -1;
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        //acutPrintf(_T("\ncornerNum: %d,"), cornerNum); // Debug
        //placeWallSegment(corners[i], corners[i + 1]);
        closeLoopCounter++;
        //acutPrintf(_T("\ncloseLoopCounter: %d,"), closeLoopCounter); // Debug

        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        AcGeVector3d direction = (end - start).normal();

        //acutPrintf(_T("\nstart?: %f, %f"), start.x, start.y); // Debug
        //acutPrintf(_T("\nend?: %f, %f"), end.x, end.y); // Debug

        //acutPrintf(_T("\ndirection.y is integer?: %f,"), direction.y); // Debug
        //acutPrintf(_T("\ndirection.x is integer?: %f,"), direction.x); // Debug
        if (isInteger(direction.x) && isInteger(direction.y)) {
            //acutPrintf(_T("\nYES.")); // Debug
            start = corners[cornerNum];
            end = corners[cornerNum + 1];
        }
        else {
            //acutPrintf(_T("\nNO. i < corners.size() - 1?")); // Debug
            if (cornerNum < corners.size() - 1) {
                //acutPrintf(_T("\nYES.")); // Debug
                start = corners[cornerNum];
                end = corners[cornerNum - closeLoopCounter];
                closeLoopCounter = -1;
                loopIndexLastPanel = 1;
            }
            else {
                //acutPrintf(_T("\nNO.")); // Debug
                start = corners[cornerNum];
                end = corners[cornerNum - closeLoopCounter];
            }
        }

        //acutPrintf(_T("\nstart after?: %f, %f"), start.x, start.y); // Debug
        //acutPrintf(_T("\nend after?: %f, %f"), end.x, end.y); // Debug


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

        // Use the biggest block size to calculate the number of panels and empty space should be iterated over using smaller block size
        double distance = start.distanceTo(end) - 50;
        direction = (end - start).normal();

        //acutPrintf(_T("\ndirection.y: %f,"), direction.y); // Debug
        //acutPrintf(_T("\ndirection.x: %f,"), direction.x); // Debug

        AcGePoint3d currentPoint = start + direction * 25;
        double rotation = atan2(direction.y, direction.x);

        if (loopIndex == outerLoopIndexValue) { // FIX consider if next corner is inner or outter
            distance += 20;
            currentPoint -= direction * 10;
            rotation += M_PI;
        }
        
        //acutPrintf(_T("\nrotation: %f,"), rotation); // Debug

        //acutPrintf(_T("\nrotation after snap: %f,"), snapToExactAngle(rotation, TOLERANCE)); // Debug

        // Fetch this variable from DefineHeight
        int wallHeight = globalVarHeight;

        int currentHeight = 0;
        int panelHeights[] = { 135, 60 };

        // List of available panels
        std::vector<Panel> panelSizes = {
            /* {90, {L"128280X", L"129837X"}},*/ // ONLY ENABLE FOR 90 PANELS
            /*{75, {L"128281X", L"129838X"}}, */ // ONLY ENABLE FOR 75 PANELS
            {60, {L"128282X", L"129839X"}},
            {45, {L"128283X", L"129840X"}},
            {30, {L"128284X", L"129841X"}},
            {15, {L"128285X", L"129842X"}},
            {10, {L"128292X", L"129884X"}}, // *10 Compensator move to middle TODO:
            {5, {L"128287X", L"129879X"}} // *5 Compensator add a break
        };

        // Iterate through every panel type
        for (const auto& panel : panelSizes) {
            currentHeight = 0;
            AcGePoint3d backupCurrentPoint = currentPoint;
            double backupDistance = distance;

            // Iterate through 135 and 60 height
            for (int panelNum = 0; panelNum < 2; panelNum++) {
                AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());
                //acutPrintf(_T("\nPanel length: %d,"), panel.length); // Debug

                if (assetId == AcDbObjectId::kNull) {
                    acutPrintf(_T("\nFailed to load asset."));
                }
                else {
                    //acutPrintf(_T("\nwallHeight: %d,"), wallHeight); // Debug
                    //acutPrintf(_T(" currentHeight: %d,"), currentHeight); // Debug
                    //acutPrintf(_T(" panelHeight num: %d,"), panelNum); // Debug
                    //acutPrintf(_T(" panelHeight: %d"), panelHeights[panelNum]); // Debug

                    int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);  // Calculate the number of panels that fit vertically

                    //acutPrintf(_T("\nnumPanelsHeight: %d,"), numPanelsHeight); // Debug

                    for (int x = 0; x < numPanelsHeight; x++) {
                        currentPoint = backupCurrentPoint;
                        distance = backupDistance;

                        // Place walls
                        int numPanels = static_cast<int>(distance / panel.length);  // Calculate the number of panels that fit horizontally
                        int numOfWallSegmentsPlaced = 0;
                        for (int i = 0; i < numPanels; i++) {

                            // Place the wall segment without scaling
                            AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                            AcGePoint3d currentPointWithHeight = currentPoint;
                            currentPointWithHeight.z += currentHeight;
                            if (loopIndex == outerLoopIndexValue) {
                                currentPointWithHeight += direction * panel.length;
                            }
                            pBlockRef->setPosition(currentPointWithHeight);
                            pBlockRef->setBlockTableRecord(assetId);
                            rotation = normalizeAngle(rotation);
                            rotation = snapToExactAngle(rotation, TOLERANCE);
                            pBlockRef->setRotation(rotation);  // Apply rotation
                            pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

                            if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
                                numOfWallSegmentsPlaced += 1;
                            }
                            else {
                                acutPrintf(_T("\nFailed to place wall segment."));
                            }
                            pBlockRef->close();  // Decrement reference count

                            currentPoint += direction * panel.length;  // Move to the next panel
                            distance -= panel.length;
                        }
                        //acutPrintf(_T("\n%d wall segments placed successfully."), numOfWallSegmentsPlaced);
                        currentHeight += panelHeights[panelNum];
                    }
                }
            }
        }
        /*if (!(loopIndex == outerLoopIndexValue) && loopIndexLastPanel == outerLoopIndexValue) {
            loopIndexLastPanel = 1;
        }*/
        loopIndex = loopIndexLastPanel;
        pModelSpace->close();  // Decrement reference count
        pBlockTable->close();  // Decrement reference count
    }

    //acutPrintf(_T("\nCompleted placing walls."));
}
