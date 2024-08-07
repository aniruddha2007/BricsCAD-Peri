﻿// Created by:Ani  (2024-05-31)
// Modified by:Ani (2024-07-04)
// TODO:
// CornerAssetPlacer.cpp
// This file contains the implementation of the CornerAssetPlacer class.
// The CornerAssetPlacer class is used to place assets at corners in BricsCAD.
// The detectPolylines function is used to detect polylines in the drawing.
// The addTextAnnotation function is used to add text annotations to the drawing.
// The placeAssetsAtCorners function is used to place assets at the detected corners.
// The loadAsset function is used to load an asset from the block table.
// The placeInsideCornerPostAndPanels function is used to place assets at inside corners.
// The placeOutsideCornerPostAndPanels function is used to place assets at outside corners.
// The recreateModelSpace function is used to recreate the model space.
// The CornerAssetPlacer class is part of the AssetPlacer namespace.
/////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CornerAssetPlacer.h"
#include "SharedDefinations.h"
#include "GeometryUtils.h"
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <limits>
#include <chrono>
#include <thread>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include <AcDb/AcDbBlockTable.h>
#include <AcDb/AcDbBlockTableRecord.h>
#include <AcDb/AcDbPolyline.h>
#include "gepnt3d.h"
#include "DefineHeight.h"
#include "DefineScale.h" 

// Static member definition
std::map<AcGePoint3d, std::vector<AcGePoint3d>, CornerAssetPlacer::Point3dComparator> CornerAssetPlacer::wallMap;

const int BATCH_SIZE = 10; // Process 10 entities at a time

const double TOLERANCE = 0.1; // Tolerance for angle comparison

bool isItInteger(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

// Function to recreate the model space
bool recreateModelSpace(AcDbDatabase* pDb) {
    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table for write access. Error status: %d\n"), es);
        return false;
    }

    AcDbBlockTableRecord* pModelSpace;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space for write access. Error status: %d\n"), es);
        pBlockTable->close();
        return false;
    }

    es = pModelSpace->erase();
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to erase model space. Error status: %d\n"), es);
        pModelSpace->close();
        pBlockTable->close();
        return false;
    }
    pModelSpace->close();

    AcDbBlockTableRecord* pNewModelSpace = new AcDbBlockTableRecord();
    pNewModelSpace->setName(ACDB_MODEL_SPACE);
    es = pBlockTable->add(pNewModelSpace);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to add new model space. Error status: %d\n"), es);
        pNewModelSpace->close();
        pBlockTable->close();
        return false;
    }
    pNewModelSpace->close();
    pBlockTable->close();

    return true;
}

// DETECT POLYLINES FROM DRAWING
std::vector<AcGePoint3d> CornerAssetPlacer::detectPolylines() {
    //acutPrintf(_T("\nDetecting polylines..."));
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

    //acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;
}

// ADD TEXT ANNOTATION TO DRAWING__ Only enable for debugging
//void CornerAssetPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
//    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
//    if (!pDb) {
//        acutPrintf(_T("\nNo working database found."));
//        return;
//    }
//
//    AcDbBlockTable* pBlockTable;
//    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
//    if (es != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get block table. Error status: %d\n"), es);
//        return;
//    }
//
//    AcDbBlockTableRecord* pModelSpace;
//    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
//    if (es != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get model space. Error status: %d\n"), es);
//        pBlockTable->close();
//        return;
//    }
//
//    AcDbText* pText = new AcDbText(position, text, AcDbObjectId::kNull, 0.2, 0);
//    es = pModelSpace->appendAcDbEntity(pText);
//    if (es == Acad::eOk) {
//        acutPrintf(_T("| Added text annotation: %s"), text);
//    }
//    else {
//        acutPrintf(_T("\nFailed to add text annotation. Error status: %d\n"), es);
//    }
//    pText->close();  // Decrement reference count
//
//    pModelSpace->close();  // Decrement reference count
//    pBlockTable->close();  // Decrement reference count
//}

double crossProduct2(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool directionOfDrawing2(std::vector<AcGePoint3d>& points) {
    // Ensure the shape is closed
    if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
        points.push_back(points.front());
    }

    double totalTurns = 0.0;

    for (size_t i = 1; i < points.size() - 1; ++i) {
        totalTurns += crossProduct2(points[i - 1], points[i], points[i + 1]);
    }

    if (totalTurns < 0) {
        return true;
    }
    else if (totalTurns > 0) {
        return false;
    }
}

// PLACE ASSETS AT DETECTED CORNERS
void CornerAssetPlacer::placeAssetsAtCorners() {
    //acutPrintf(_T("\nPlacing assets at corners..."));
    std::vector<AcGePoint3d> corners = detectPolylines();
    //check for corner connector cases
     
    //acutPrintf(_T("\nDetected %d corners from lines."), corners.size());

    AcDbObjectId cornerPostId = loadAsset(L"128286X");
    AcDbObjectId panelId = loadAsset(L"128285X");
    AcDbObjectId panel75Id = loadAsset(L"128281X");

    if (cornerPostId == AcDbObjectId::kNull || panelId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load assets."));
        return;
    }
    
    int closeLoopCounter = -1;
    int loopIndex = 0;
    double outerPointCounter = corners[0].x;
    int outerLoopIndexValue = 0;
    int firstLoopEnd;

    // Loop through corners, find outer and inner loop
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        AcGeVector3d direction = (end - start).normal();

        //acutPrintf(_T("\nCurrent position: %f, %f"), start.x, start.y); // Debug
        if (start.x > outerPointCounter) {
            outerPointCounter = start.x;
            outerLoopIndexValue = loopIndex;
        }

        //acutPrintf(_T("\ndirection.y is integer?: %f,"), direction.y); // Debug
        //acutPrintf(_T("\ndirection.x is integer?: %f,"), direction.x); // Debug
        if (isItInteger(direction.x) && isItInteger(direction.y)) {
            //acutPrintf(_T("\nYES."));
        }
        else {
            //acutPrintf(_T("\nNO. i < corners.size() - 1?"));
            if (cornerNum < corners.size() - 1) {
                //acutPrintf(_T("\nYES. loopIndex = 1"));
                closeLoopCounter = -1;
                loopIndex = 1;
                firstLoopEnd = cornerNum;
            }
            else {
                //acutPrintf(_T("\nNO."));
            }
        }
    }
    //acutPrintf(_T("\nOuter loop is loop number: %d,"), outerLoopIndexValue); // Debug

    acutPrintf(_T("\nOuter loop is loop[%d]"), outerLoopIndexValue);
    acutPrintf(_T("\nfirst loop end is %d"), firstLoopEnd);

    std::vector<AcGePoint3d> firstLoop(corners.begin(), corners.begin() + firstLoopEnd + 1);
    std::vector<AcGePoint3d> secondLoop(corners.begin() + firstLoopEnd + 1, corners.end());

    bool firstLoopIsClockwise = directionOfDrawing2(firstLoop);
    bool secondLoopIsClockwise = directionOfDrawing2(secondLoop);

    std::vector<bool> loopIsClockwise = {
        firstLoopIsClockwise,
        secondLoopIsClockwise
    };

    loopIndex = 0;
    int loopIndexLastPanel = 0;
    closeLoopCounter = -1;
    // Iterate through all detected corners and place assets accordingly
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {

        double rotation = 0.0;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        AcGeVector3d direction = (end - start).normal();

        closeLoopCounter++;
        //acutPrintf(_T("\ncloseLoopCounter: %d "), closeLoopCounter); // Debug

        bool isInside = false;

        //acutPrintf(_T("\nstart?: %f, %f"), start.x, start.y); // Debug
        //acutPrintf(_T("\nend?: %f, %f"), end.x, end.y); // Debug
        //acutPrintf(_T("| [%f, %f] "), start.x, start.y); // Debug

        //acutPrintf(_T("| direction.y is integer?: %f "), direction.y); // Debug
        //acutPrintf(_T("\ndirection.x is integer?: %f,"), direction.x); // Debug
        if (isItInteger(direction.x) && isItInteger(direction.y)) {
            //acutPrintf(_T("| YES."));
            start = corners[cornerNum];
            end = corners[cornerNum + 1];
        }
        else {
            //acutPrintf(_T("| NO. i < corners.size() - 1?"));
            if (cornerNum < corners.size() - 1) {
                //acutPrintf(_T("| YES."));
                start = corners[cornerNum];
                end = corners[cornerNum - closeLoopCounter];
                closeLoopCounter = -1;
                loopIndexLastPanel = 1;
            }
            else {
                //acutPrintf(_T("| NO."));
                start = corners[cornerNum];
                end = corners[cornerNum - closeLoopCounter];
            }
        }

        //acutPrintf(_T("\nstart after?: %f, %f"), start.x, start.y); // Debug
        //acutPrintf(_T("\nend after?: %f, %f"), end.x, end.y); // Debug

        direction = (end - start).normal();

        //acutPrintf(_T("\ndirection.y: %f,"), direction.y); // Debug
        //acutPrintf(_T("\ndirection.x: %f,"), direction.x); // Debug

        rotation = atan2(direction.y, direction.x);

        //acutPrintf(_T("\nloopIndex : %d "), loopIndex); // Debug
        //acutPrintf(_T("| loopIndexLastPanel : %d "), loopIndexLastPanel); // Debug
        //acutPrintf(_T("| outerLoopIndexValue : %d "), outerLoopIndexValue); // Debug
        if (!(loopIndex == outerLoopIndexValue)) {
            isInside = true;
        }
        if (!loopIsClockwise[loopIndex]) {
            isInside = !isInside;
        }

        if (cornerNum < corners.size() - 1) {
            AcGeVector3d prevDirection = corners[cornerNum] - corners[cornerNum > 0 ? cornerNum - 1 : corners.size() - 1];
            AcGeVector3d nextDirection = corners[cornerNum + 1] - corners[cornerNum];
            double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;
            if (crossProductZ > 0) {
                isInside = !isInside;
                rotation += M_PI_2;
            } // Change this logic based on your coordinate system
        }
        else {
            AcGeVector3d prevDirection = corners[cornerNum] - corners[cornerNum - 1];
            AcGeVector3d nextDirection = corners[0] - corners[cornerNum];
            double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;
            if (crossProductZ > 0) {
                isInside = !isInside;
                rotation += M_PI_2;
            } // Change this logic based on your coordinate system
        }

        if (isInside) {
            placeInsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, panelId);
            //addTextAnnotation(corners[cornerNum], L"Inside Corner"); // Debug
        }
        else {
            placeOutsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, panelId, panel75Id);
            //addTextAnnotation(corners[cornerNum], L"Outside Corner"); // Debug
        }
        loopIndex = loopIndexLastPanel;
    }

    //acutPrintf(_T("\nCompleted placing assets.")); // Debug
}

// LOAD ASSET FROM BLOCK TABLE
AcDbObjectId CornerAssetPlacer::loadAsset(const wchar_t* blockName) {
    //acutPrintf(_T("\nLoading asset: %s"), blockName);
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
    //acutPrintf(_T("\nLoaded block: %s"), blockName);
    return blockId;
}

// PLACE ASSETS AT INSIDE CORNERS
void CornerAssetPlacer::placeInsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelId) {
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

    int wallHeight = globalVarHeight;
    int currentHeight = 0;
    int panelHeights[] = { 135, 60 };

    for (int panelNum = 0; panelNum < 2; panelNum++) {
        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

        if (panelNum == 1) {
            cornerPostId = loadAsset(L"129864X");
            panelId = loadAsset(L"129842X");
            //panel75Id = loadAsset(L"");
        }

        for (int x = 0; x < numPanelsHeight; x++) {
            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d cornerWithHeight = corner;
            cornerWithHeight.z += currentHeight;
            pCornerPostRef->setPosition(cornerWithHeight);
            pCornerPostRef->setBlockTableRecord(cornerPostId);
            rotation = normalizeAngle(rotation);
            rotation = snapToExactAngle(rotation, TOLERANCE);
            pCornerPostRef->setRotation(rotation);
            pCornerPostRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
                //acutPrintf(_T("\nCorner post placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place corner post."));
            }
            pCornerPostRef->close();

            AcGeVector3d panelAOffset, panelBOffset, panel75OffsetA, panel75OffsetB;
            rotation = normalizeAngle(rotation);
            rotation = snapToExactAngle(rotation, TOLERANCE);

            if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                panelAOffset = AcGeVector3d(10.0, 0.0, 0.0);
                panelBOffset = AcGeVector3d(0.0, -25.0, 0.0);
                panel75OffsetA = panelAOffset + AcGeVector3d(15.0, 0.0, 0.0);
                panel75OffsetB = panelBOffset + AcGeVector3d(0.0, -75.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(0.0, 10.0, 0.0);
                panelBOffset = AcGeVector3d(25.0, 0.0, 0.0);
                panel75OffsetA = panelAOffset + AcGeVector3d(0.0, 15.0, 0.0);
                panel75OffsetB = panelBOffset + AcGeVector3d(15.0, 0.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
                panelAOffset = AcGeVector3d(-10.0, 0.0, 0.0);
                panelBOffset = AcGeVector3d(0.0, 25.0, 0.0);
                panel75OffsetA = panelAOffset + AcGeVector3d(-15.0, 0.0, 0.0);
                panel75OffsetB = panelBOffset + AcGeVector3d(0.0, 15.0, 0.0);
            }
            else if (areAnglesEqual(rotation, 3 * M_PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(0.0, -10.0, 0.0);
                panelBOffset = AcGeVector3d(-25.0, 0.0, 0.0);
                panel75OffsetA = panelAOffset + AcGeVector3d(0.0, -15.0, 0.0);
                panel75OffsetB = panelBOffset + AcGeVector3d(-15.0, 0.0, 0.0);
            }
            else {
                acutPrintf(_T("\nInvalid rotation angle detected: %f at %f"), rotation, corner);
                continue;
            }

            AcGePoint3d panelPositionA = cornerWithHeight + panelAOffset;
            AcGePoint3d panelPositionB = cornerWithHeight + panelBOffset;
            AcGePoint3d panelPosition75A = cornerWithHeight + panel75OffsetA;
            AcGePoint3d panelPosition75B = cornerWithHeight + panel75OffsetB;

            AcDbBlockReference* pPanelARef = new AcDbBlockReference();
            pPanelARef->setPosition(panelPositionA);
            pPanelARef->setBlockTableRecord(panelId);
            pPanelARef->setRotation(rotation);
            pPanelARef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanelARef) == Acad::eOk) {
                //acutPrintf(_T("\nPanel A placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel A."));
            }
            pPanelARef->close();

            AcDbBlockReference* pPanelBRef = new AcDbBlockReference();
            pPanelBRef->setPosition(panelPositionB);
            pPanelBRef->setBlockTableRecord(panelId);
            pPanelBRef->setRotation(rotation + M_PI_2);
            pPanelBRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanelBRef) == Acad::eOk) {
                //acutPrintf(_T("\nPanel B placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel B."));
            }
            pPanelBRef->close();

            // Place Panel 75 near Panel A and Panel B
            AcDbBlockReference* pPanel75RefA = new AcDbBlockReference();
            pPanel75RefA->setPosition(panelPosition75A);
            //pPanel75RefA->setBlockTableRecord(panel75Id);
            pPanel75RefA->setRotation(rotation);
            pPanel75RefA->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanel75RefA) == Acad::eOk) {
                //acutPrintf(_T("\nPanel 75 placed near Panel A successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel 75 near Panel A."));
            }
            pPanel75RefA->close();

            AcDbBlockReference* pPanel75RefB = new AcDbBlockReference();
            pPanel75RefB->setPosition(panelPosition75B);
            //pPanel75RefB->setBlockTableRecord(panel75Id);
            pPanel75RefB->setRotation(rotation + M_PI_2);
            pPanel75RefB->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanel75RefB) == Acad::eOk) {
                //acutPrintf(_T("\nPanel 75 placed near Panel B successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel 75 near Panel B."));
            }
            pPanel75RefB->close();

            currentHeight += panelHeights[panelNum];
        }
    }

    pModelSpace->close();
    pBlockTable->close();
}

// PLACE ASSETS AT OUTSIDE CORNERS
void CornerAssetPlacer::placeOutsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelId, AcDbObjectId panel75Id) {
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

    int wallHeight = globalVarHeight;
    int currentHeight = 0;
    int panelHeights[] = { 135, 60 };

    AcGePoint3d cornerWithHeight = corner;
    cornerWithHeight.z += currentHeight;

    double offset = 10.0;
    int rotationDegrees = static_cast<int>(rotation * 180 / M_PI);
    //acutPrintf(_T("\nrotationDegrees: %d "), rotationDegrees); // Debug
    switch (rotationDegrees) {
    case 90:
        cornerWithHeight.x -= offset;
        cornerWithHeight.y -= offset;
        break;
    case 180:
        cornerWithHeight.x += offset;
        cornerWithHeight.y -= offset;
        break;
    case -90:
        cornerWithHeight.x += offset;
        cornerWithHeight.y += offset;
        break;
    case 270:
        cornerWithHeight.x += 2 * offset;
    case 0:
        cornerWithHeight.x -= offset;
        cornerWithHeight.y += offset;
        break;
    default:
        acutPrintf(_T("\nInvalid rotation angle detected."));
        return;
    }

    rotation -= M_PI / 2;
    rotation = normalizeAngle(rotation + M_PI_2);
    rotation = snapToExactAngle(rotation, TOLERANCE);

    for (int panelNum = 0; panelNum < 2; panelNum++) {
        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

        if (panelNum == 1) {
            cornerPostId = loadAsset(L"129864X");
            panel75Id = loadAsset(L"129838X");
        }

        for (int x = 0; x < numPanelsHeight; x++) {
            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d currentCornerWithHeight = cornerWithHeight;
            currentCornerWithHeight.z += currentHeight;
            pCornerPostRef->setPosition(currentCornerWithHeight);
            pCornerPostRef->setBlockTableRecord(cornerPostId);
            rotation = normalizeAngle(rotation);
            rotation = snapToExactAngle(rotation, TOLERANCE);
            pCornerPostRef->setRotation(rotation);
            pCornerPostRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
                //acutPrintf(_T("\nCorner post placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place corner post."));
            }
            pCornerPostRef->close();

            AcGeVector3d panelAOffset, panelBOffset, panel75OffsetA, panel75OffsetB;
            rotation = normalizeAngle(rotation);
            rotation = snapToExactAngle(rotation, TOLERANCE);

            if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                panelAOffset = AcGeVector3d(85.0, -10.0, 0.0);
                panelBOffset = AcGeVector3d(10.0, -10.0, 0.0);
                panel75OffsetA = panelAOffset;
                panel75OffsetB = panelBOffset;
            }
            else if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(10.0, 85.0, 0.0);
                panelBOffset = AcGeVector3d(10.0, 10.0, 0.0);
                panel75OffsetA = panelAOffset;
                panel75OffsetB = panelBOffset;
            }
            else if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
                panelAOffset = AcGeVector3d(-85.0, 10.0, 0.0);
                panelBOffset = AcGeVector3d(-10.0, 10.0, 0.0);
                panel75OffsetA = panelAOffset;
                panel75OffsetB = panelBOffset;
            }
            else if (areAnglesEqual(rotation, 3 * M_PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(-10.0, -85.0, 0.0);
                panelBOffset = AcGeVector3d(-10.0, -10.0, 0.0);
                panel75OffsetA = panelAOffset;
                panel75OffsetB = panelBOffset;
            }
            else {
                acutPrintf(_T("\nInvalid rotation angle detected."));
                continue;
            }

            AcGePoint3d panelPosition75A = currentCornerWithHeight + panel75OffsetA;
            AcGePoint3d panelPosition75B = currentCornerWithHeight + panel75OffsetB;


            // Place Panel 75 near Panel A and Panel B
            AcDbBlockReference* pPanel75RefA = new AcDbBlockReference();
            pPanel75RefA->setPosition(panelPosition75A);
            pPanel75RefA->setBlockTableRecord(panel75Id);
            pPanel75RefA->setRotation(rotation + M_PI);
            pPanel75RefA->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanel75RefA) == Acad::eOk) {
                //acutPrintf(_T("\nPanel 75 placed near Panel A successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel 75 near Panel A."));
            }
            pPanel75RefA->close();

            AcDbBlockReference* pPanel75RefB = new AcDbBlockReference();
            pPanel75RefB->setPosition(panelPosition75B);
            pPanel75RefB->setBlockTableRecord(panel75Id);
            pPanel75RefB->setRotation(rotation + M_PI_2 + M_PI);
            pPanel75RefB->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanel75RefB) == Acad::eOk) {
                //acutPrintf(_T("\nPanel 75 placed near Panel B successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel 75 near Panel B."));
            }
            pPanel75RefB->close();

            currentHeight += panelHeights[panelNum];
        }
    }

    pModelSpace->close();
    pBlockTable->close();
}
