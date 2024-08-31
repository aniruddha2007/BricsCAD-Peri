// Created by:Ani  (2024-05-31)
// Modified by:Ani (2024-06-01)
// TODO:
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
#include "CornerAssetPlacer.h"
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
#include "DefineHeight.h"
#include "DefineScale.h"
#include <thread>
#include <chrono>
#include <map>
#include "Timber/TimberAssetCreator.h"

std::map<AcGePoint3d, std::vector<AcGePoint3d>, WallPlacer::Point3dComparator> WallPlacer::wallMap;

const int BATCH_SIZE = 30; // Batch size for processing entities

const double TOLERANCE = 0.1; // Tolerance for comparing angles

// Structure to hold panel information
struct Panel {
    int length;
    std::wstring id[3];
};

bool isInteger(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

//AcGePoint3d findTopLeftPoint(const std::vector<AcGePoint3d>& points) {
//    return *std::min_element(points.begin(), points.end(), [](const AcGePoint3d& a, const AcGePoint3d& b) {
//        if (a.y == b.y) {
//            return a.x < b.x;
//        }
//        return a.y > b.y;
//        });
//}
//
////function to normalize polyline orientation
//std::vector<AcGePoint3d> normalizePolylineOrientation(std::vector<AcGePoint3d>& points) {
//    if (points.empty()) {
//        return points;
//    }
//
//    AcGePoint3d topLeft = findTopLeftPoint(points);
//
//    auto it = std::find(points.begin(), points.end(), topLeft);
//    if (it != points.begin()) {
//		std::rotate(points.begin(), it, points.end());
//	}
//    return points;
//}

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

    forcePolylineClockwise(corners);

    acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;

}

//calculate distance between polylines
double WallPlacer::calculateDistanceBetweenPolylines() {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        return -1.0;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        return -1.0;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        pBlockTable->close();
        return -1.0;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        pModelSpace->close();
        pBlockTable->close();
        return -1.0;
    }

    AcDbPolyline* pFirstPolyline = nullptr;
    AcDbPolyline* pSecondPolyline = nullptr;

    // Find the first two polylines
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                if (!pFirstPolyline) {
                    pFirstPolyline = AcDbPolyline::cast(pEnt);
                }
                else if (!pSecondPolyline) {
                    pSecondPolyline = AcDbPolyline::cast(pEnt);
                    pEnt->close();
                    break; // Found both polylines, no need to continue
                }
            }
            pEnt->close();
        }
    }

    double distance = -1.0;
    if (pFirstPolyline && pSecondPolyline) {
        distance = getPolylineDistance(pFirstPolyline, pSecondPolyline);
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();
    return distance;
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

// Add text annotation function only enable for debugging
//void WallPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
//    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
//    if (!pDb) {
//        acutPrintf(_T("\nNo working database found."));
//        return;
//    }
//
//    AcDbBlockTable* pBlockTable;
//    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get block table."));
//        return;
//    }
//
//    AcDbBlockTableRecord* pModelSpace;
//    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get model space."));
//        pBlockTable->close();
//        return;
//    }
//
//    AcDbText* pText = new AcDbText(position, text, AcDbObjectId::kNull, 0.2, 0);
//    if (pModelSpace->appendAcDbEntity(pText) == Acad::eOk) {
//        acutPrintf(_T("\nAdded text annotation: %s"), text);
//    }
//    else {
//        acutPrintf(_T("\nFailed to add text annotation."));
//    }
//    pText->close();  // Decrement reference count
//
//    pModelSpace->close();  // Decrement reference count
//    pBlockTable->close();  // Decrement reference count
//}

// Place wall segment
void WallPlacer::placeWallSegment(const AcGePoint3d& start, const AcGePoint3d& end) {

}

// Function to compute if the corner is turning clockwise or counterclockwise
bool isClockwise(const AcGePoint3d& p0, const AcGePoint3d& p1, const AcGePoint3d& p2) {
    // Compute the vectors for the edges
    AcGeVector3d v1 = p1 - p0;  // Vector from p0 to p1
    AcGeVector3d v2 = p2 - p1;  // Vector from p1 to p2

    // Compute the cross product
    AcGeVector3d crossProduct = v1.crossProduct(v2);

    // Determine the direction of the turn
    // If cross product z-component is positive, the turn is clockwise
    // If cross product z-component is negative, the turn is counterclockwise
    return crossProduct.z < 0;
}

double crossProduct(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool directionOfDrawing(std::vector<AcGePoint3d>& points) {
    // Ensure the shape is closed
    if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
        points.push_back(points.front());
    }

    double totalTurns = 0.0;

    for (size_t i = 1; i < points.size() - 1; ++i) {
        totalTurns += crossProduct(points[i - 1], points[i], points[i + 1]);
    }

    if (totalTurns < 0) {
        return true;
    }
    else if (totalTurns > 0) {
        return false;
    }
}

// Function to rotate a vector by a specified angle (in radians) around the Z-axis
AcGeVector3d rotateVector(const AcGeVector3d& direction, double angle) {
    // Rotation matrix components for the specified angle around Z-axis
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    double x = direction.x * cosAngle - direction.y * sinAngle;
    double y = direction.x * sinAngle + direction.y * cosAngle;
    double z = direction.z; // Z remains unchanged when rotating around Z-axis

    return AcGeVector3d(x, y, z);
}

void adjustStartAndEndPoints(AcGePoint3d& point, const AcGeVector3d& direction, bool isInner, double distanceBetweenPolylines, const CornerConfig& cornerConfig) {
    bool isInnerCorner = cornerConfig.isInside;

    if (isInner) {
        if (distanceBetweenPolylines == 150) {
            point -= direction * 300;
        }
        else {
            point -= direction * 250;
        }
    }
    else {
        // Using the provided table for outer loop adjustments
        int adjustment = 0;

        if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
            adjustment = 550;
        }
        else if (distanceBetweenPolylines == 250) {
            adjustment = 600;
        }
        else if (distanceBetweenPolylines == 300) {
            adjustment = 650;
        }
        else if (distanceBetweenPolylines == 350) {
            adjustment = 700;
        }
        else if (distanceBetweenPolylines == 400) {
            adjustment = 750;
        }
        else if (distanceBetweenPolylines == 450) {
            adjustment = 800;
        }
        else if (distanceBetweenPolylines == 500) {
            adjustment = 850;
        }
        else if (distanceBetweenPolylines == 550) {
            adjustment = 900;
        }
        else if (distanceBetweenPolylines == 600) {
            adjustment = 950;
        }
        else if (distanceBetweenPolylines == 650) {
            adjustment = 1000;
        }
        else if (distanceBetweenPolylines == 700) {
            adjustment = 1050;
        }
        else if (distanceBetweenPolylines == 750) {
            adjustment = 1100;
        }
        else if (distanceBetweenPolylines == 800) {
            adjustment = 1150;
        }
        else if (distanceBetweenPolylines == 850) {
            adjustment = 1200;
        }
        else if (distanceBetweenPolylines == 900) {
            adjustment = 1250;
        }
        else if (distanceBetweenPolylines == 950) {
            adjustment = 1300;
        }
        else if (distanceBetweenPolylines == 1000) {
            adjustment = 1350;
        }
        else if (distanceBetweenPolylines == 1050) {
            adjustment = 1400;
        }
        else if (distanceBetweenPolylines == 1100) {
            adjustment = 1450;
        }
        else if (distanceBetweenPolylines == 1150) {
            adjustment = 1500;
        }
        else if (distanceBetweenPolylines == 1200) {
            adjustment = 1550;
        }
        else if (distanceBetweenPolylines == 1250) {
            adjustment = 1600;
        }
        else if (distanceBetweenPolylines == 1300) {
            adjustment = 1650;
        }
        else if (distanceBetweenPolylines == 1350) {
            adjustment = 1700;
        }
        else if (distanceBetweenPolylines == 1400) {
            adjustment = 1750;
        }
        else if (distanceBetweenPolylines == 1450) {
            adjustment = 1800;
        }
        else if (distanceBetweenPolylines == 1500) {
            adjustment = 1850;
        }
        else if (distanceBetweenPolylines == 1550) {
            adjustment = 1900;
        }
        else if (distanceBetweenPolylines == 1600) {
            adjustment = 1950;
        }
        else if (distanceBetweenPolylines == 1650) {
            adjustment = 2000;
        }
        else if (distanceBetweenPolylines == 1700) {
            adjustment = 2050;
        }
        else if (distanceBetweenPolylines == 1750) {
            adjustment = 2100;
        }
        else if (distanceBetweenPolylines == 1800) {
            adjustment = 2150;
        }
        else if (distanceBetweenPolylines == 1850) {
            adjustment = 2200;
        }
        else if (distanceBetweenPolylines == 1900) {
            adjustment = 2250;
        }
        else if (distanceBetweenPolylines == 1950) {
            adjustment = 2300;
        }
        else if (distanceBetweenPolylines == 2000) {
            adjustment = 2350;
        }
        else if (distanceBetweenPolylines == 2050) {
            adjustment = 2400;
        }
        else if (distanceBetweenPolylines == 2100) {
            adjustment = 2450;
        }
        else {
            adjustment = 150; // Default case for any unexpected distance value
        }

        adjustment -= 100;

        if (!isInnerCorner) {
            // Flip the adjustment for outside corners
            adjustment = -adjustment;
        }

        point -= direction * adjustment;
    }
}

void WallPlacer::placeWalls() {
    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo polylines detected.")); // Debug
        return;
    }

    double distanceBetweenPolylines = calculateDistanceBetweenPolylines();

    // Initialize PanelDimensions instance
    PanelDimensions panelDims;

    // Generate the CornerConfig vector using the public getPanelConfig method
    std::vector<CornerConfig> cornerConfigs = CornerAssetPlacer::generateCornerConfigs(corners, CornerAssetPlacer::getPanelConfig(distanceBetweenPolylines, panelDims));

    int closeLoopCounter = -1;
    int loopIndex = 0;
    double outerPointCounter = corners[0].x;
    int outerLoopIndexValue = 0;
    int firstLoopEnd;

    // First Pass: Determine inner and outer loops
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[(cornerNum + 1) % corners.size()];  // Wrap around to the first point
        AcGeVector3d direction = (end - start).normal();

        if (start.x > outerPointCounter) {
            outerPointCounter = start.x;
            outerLoopIndexValue = loopIndex;
        }

        if (!isInteger(direction.x) || !isInteger(direction.y)) {
            if (cornerNum < corners.size() - 1) {
                closeLoopCounter = -1;
                loopIndex = 1;
                firstLoopEnd = cornerNum;
            }
        }
    }

    //Debug
    //acutPrintf(_T("\nOuter loop is loop[%d]"), outerLoopIndexValue);
    //acutPrintf(_T("\nfirst loop end is %d"), firstLoopEnd);

    std::vector<AcGePoint3d> firstLoop(corners.begin(), corners.begin() + firstLoopEnd +1);
    std::vector<AcGePoint3d> secondLoop(corners.begin() + firstLoopEnd + 1, corners.end());

    bool firstLoopIsClockwise = directionOfDrawing(firstLoop);
    bool secondLoopIsClockwise = directionOfDrawing(secondLoop);

    std::vector<bool> loopIsClockwise = {
        firstLoopIsClockwise,
        secondLoopIsClockwise
    };

    //Debug
    /*acutPrintf(_T("\nFirst loop first corner at (%f."), corners[0].x);
    acutPrintf(_T(", %f)"), corners[0].y);
    acutPrintf(_T(", next corner (%f, "), corners[1].x);
    acutPrintf(_T(", %f), is"), corners[1].y);
    if (!firstLoopIsClockwise) {
        acutPrintf(_T(" not"));
    }
    acutPrintf(_T(" Clockwise"));
    acutPrintf(_T("\nSecond loop first corner at (%f."), corners[firstLoopEnd + 1].x);
    acutPrintf(_T(", %f),"), corners[firstLoopEnd + 1].y);
    acutPrintf(_T(", next corner (%f, "), corners[firstLoopEnd + 2].x);
    acutPrintf(_T(", %f), is"), corners[firstLoopEnd + 2].y);
    if (!secondLoopIsClockwise) {
        acutPrintf(_T(" not"));
    }
    acutPrintf(_T(" Clockwise"));*/



    struct WallPanel {
        AcGePoint3d position;
        AcDbObjectId assetId;
        double rotation;
        double length;
        int height;
        int loopIndex;
        bool isOuterLoop;
    };

    struct Timber {
        AcGePoint3d position;
        AcDbObjectId assetId;
        double rotation;
        double length;
        int height;
        int loopIndex;
        bool isOuterLoop;
    };

    std::vector<WallPanel> wallPanels;
    std::vector<Timber> timber;

    std::vector<std::pair<AcGePoint3d, AcGePoint3d>> segments;

    loopIndex = 0;
    int loopIndexLastPanel = 0;
    closeLoopCounter = -1;


    int wallHeight = globalVarHeight;
    int currentHeight = 0;
    int panelHeights[] = { 1350, 1200, 600 };

    std::vector<Panel> panelSizes = {
        {600, {L"128282X", L"136096X", L"129839X"}},
        {450, {L"128283X", L"Null", L"129840X"}},
        {300, {L"128284X", L"Null", L"129841X"}},
        {150, {L"128285X", L"Null", L"129842X"}},
        {100, {L"128292X", L"Null", L"129884X"}},
        {50, {L"128287X", L"Null", L"129879X"}}
    };


    // Second pass: Saw tooth detect and modify walls
    int sawToothCounter = 0;
    std::vector<int> tempSawToothIndex;
    std::vector<int> sawToothIndex;
    int startIndex;
    int endIndex;

    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {

        closeLoopCounter++;
        AcGePoint3d start = corners[cornerNum];
        startIndex = cornerNum;
        AcGePoint3d end = corners[cornerNum + 1];
        endIndex = cornerNum + 1;
        AcGeVector3d direction = (end - start).normal();

        if (!isInteger(direction.x) || !isInteger(direction.y)) {
            if (cornerNum < corners.size() - 1) {
                start = corners[cornerNum];
                startIndex = cornerNum;
                end = corners[cornerNum - closeLoopCounter];
                endIndex = cornerNum - closeLoopCounter;
                closeLoopCounter = -1;
                loopIndexLastPanel = 1;
            }
            else {
                start = corners[cornerNum];
                startIndex = cornerNum;
                end = corners[cornerNum - closeLoopCounter];
                endIndex = cornerNum - closeLoopCounter;
            }
        }
        if (start.distanceTo(end) < 100) {
            tempSawToothIndex.push_back(startIndex);
            tempSawToothIndex.push_back(endIndex);
        }
        else if (tempSawToothIndex.size() == 2)
        {
            if (loopIndex != outerLoopIndexValue) {
                acutPrintf(_T("\nFound a tooth?"));
                //AcGeVector3d rotatedDirection = rotateVector(direction, -M_PI);
                corners[tempSawToothIndex[0]] -= direction * 35;
                //rotatedDirection = rotateVector(direction, M_PI_2);
                corners[tempSawToothIndex[1]] -= direction * 35;
                sawToothIndex.push_back(tempSawToothIndex[0]);
            }
            else if (!loopIsClockwise[loopIndex] && loopIndex == outerLoopIndexValue) {

            }
            tempSawToothIndex.clear();
        }
        else if (tempSawToothIndex.size() == 6)
        {
            if (loopIndex != outerLoopIndexValue) {
                //acutPrintf(_T("\nFound a tooth?")); // Debug
                AcGeVector3d rotatedDirection = rotateVector(direction, -M_PI_2);
                corners[tempSawToothIndex[2]] -= rotatedDirection * 35;
                corners[tempSawToothIndex[3]] -= rotatedDirection * 35;
                sawToothIndex.push_back(tempSawToothIndex[2]);
            }
            else if (!loopIsClockwise[loopIndex] && loopIndex == outerLoopIndexValue) {

            }
            tempSawToothIndex.clear();
        }

        loopIndex = loopIndexLastPanel;
    }

    loopIndex = 0;
    loopIndexLastPanel = 0;
    closeLoopCounter = -1;
    double totalPanelsPlaced = 0;
    std::vector<int> cornerLocations;

    // Third Pass: Save all positions, asset IDs, and rotations
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {

        closeLoopCounter++;
        cornerLocations.push_back(static_cast<int>(totalPanelsPlaced));
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        AcGeVector3d direction = (end - start).normal();
        AcGeVector3d reverseDirection = (start - end).normal();

        if (!isInteger(direction.x) || !isInteger(direction.y)) {
            if (cornerNum < corners.size() - 1) {
                start = corners[cornerNum];
                end = corners[cornerNum - closeLoopCounter];
                closeLoopCounter = -1;
                loopIndexLastPanel = 1;
            }
            else {
                start = corners[cornerNum];
                end = corners[cornerNum - closeLoopCounter];
            }
        }

        CornerConfig cornerConfig = cornerConfigs[cornerNum];

        if (std::find(sawToothIndex.begin(), sawToothIndex.end(), cornerNum) == sawToothIndex.end()) {
            // Get previous and next corners
            AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
            AcGePoint3d next = corners[(cornerNum + 2) % corners.size()];

            bool prevClockwise = isClockwise(prev, start, end);
            bool nextClockwise = isClockwise(start, end, next);

            bool isInner = loopIndex != outerLoopIndexValue;
            bool isOuter = !isInner;
            if (!loopIsClockwise[loopIndex]) {
                isInner = !isInner;
                isOuter = !isOuter;
            }

            direction = (end - start).normal();
            reverseDirection = (start - end).normal();

            // Adjust start and end points symmetrically
            adjustStartAndEndPoints(start, direction, isInner, distanceBetweenPolylines, cornerConfig);
            adjustStartAndEndPoints(end, reverseDirection, isInner, distanceBetweenPolylines, cornerConfig);

            double distance = start.distanceTo(end);
            if (distance < 50) {
                acutPrintf(_T("\nDistance between corners is too small to place any panel."));
                continue; // Skip this segment if distance is insufficient
            }
            AcGePoint3d currentPoint = start;

            // Ensure the distance is sufficient to place panels
            if (distance < panelSizes[0].length) {
                acutPrintf(_T("\nDistance between corners is too small to place any panel."));
                return; // Exit if distance is insufficient
            }

            double rotation = atan2(direction.y, direction.x);

            // Adjust rotation if the loop is outer
            if (isOuter) {
                rotation += M_PI;
            }

            // Place panels along the segment
            for (const auto& panel : panelSizes) {
                currentHeight = 0;

                for (int panelNum = 0; panelNum < 3; panelNum++) {
                    AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());

                    if (assetId == AcDbObjectId::kNull) {
                        acutPrintf(_T("\nFailed to load asset for panel %s."), panel.id[panelNum].c_str());
                        continue; // Skip this panel if asset loading fails
                    }

                    int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);
                    if (numPanelsHeight <= 0) continue;

                    int numPanels = static_cast<int>(distance / panel.length);
                    if (numPanels == 0) {
                        acutPrintf(_T("\nInsufficient distance to place any panels: %f"), distance);
                        continue; // Skip this panel if distance is insufficient
                    }
                    for (int i = 0; i < numPanels; i++) {
                        AcGePoint3d currentPointWithHeight = currentPoint;
                        currentPointWithHeight.z += currentHeight;

                        if (isOuter) {
                            currentPointWithHeight += direction * panel.length;
                        }

                        rotation = normalizeAngle(rotation);
                        rotation = snapToExactAngle(rotation, TOLERANCE);

                        wallPanels.push_back(WallPanel{
    currentPointWithHeight,  // position
    assetId,                 // assetId
    rotation,                // rotation
    static_cast<double>(panel.length),  // length, ensure this is a double
    panelHeights[panelNum],  // height
    loopIndex,               // loopIndex
    isOuter                  // isOuterLoop
                            });

                        totalPanelsPlaced++;
                        currentPoint += direction * panel.length;
                        distance -= panel.length;

                        if (distance < 0) {
                            acutPrintf(_T("\nInsufficient distance left for further panels."));
                            break;
                        }
                    }

                    // Place timber for remaining distance
                    if (distance > 0 && distance < 50) {
                        AcDbObjectId timberAssetId = TimberAssetCreator::createTimberAsset(distance, panelHeights[panelNum]);
                        if (timberAssetId != AcDbObjectId::kNull) {
                            AcGePoint3d timberPosition = currentPoint;
                            timberPosition.z += static_cast<double>(panelHeights[panelNum]) / 2.0;

                            timberPosition += AcGeVector3d(50 * cos(rotation), 50 * sin(rotation), 0);

                            timber.push_back({ timberPosition, timberAssetId, rotation, distance, panelHeights[panelNum], loopIndex, isOuter });
                            distance = 0;
                        }
                        else {
                            acutPrintf(_T("\nFailed to create timber asset."));
                        }
                    }
                }
            }

            segments.push_back(std::make_pair(start, end)); // Save segment for later compensator placement
            loopIndex = loopIndexLastPanel;
        }

        
        
        segments.push_back(std::make_pair(start, end)); // Save segment for later compensator placement
        loopIndex = loopIndexLastPanel;
    }

    // Forth Pass: Adjust positions for specific asset IDs(compensators)
    std::vector<AcDbObjectId> centerAssets = {
        loadAsset(L"128285X"),
        loadAsset(L"129842X"),
        loadAsset(L"129879X"),
        loadAsset(L"129884X"),
        loadAsset(L"128287X"),
        loadAsset(L"128292X")
    };

    int prevStartCornerIndex = -1;
    int movedCompensators = 0;

    for (int panelNum = 0; panelNum < totalPanelsPlaced; ++panelNum) {
        WallPanel& panel = wallPanels[panelNum];
        if (std::find(centerAssets.begin(), centerAssets.end(), panel.assetId) != centerAssets.end()) {

            // Find the two corner points between which the panel is placed
            int panelPosition = panelNum;  // This should be the index of the panel
            //acutPrintf(_T("\nFound compensator at %d."), panelNum);
            WallPanel detectedPanel = wallPanels[panelPosition];
            AcGePoint3d detectedPanelPosition = detectedPanel.position;
            AcDbObjectId detectedPanelId = detectedPanel.assetId;

            double panelLength = wallPanels[panelPosition].length;


            //acutPrintf(_T(" panelLength = %f."), panelLength);

            int startCornerIndex = -1;
            int endCornerIndex = -1;

            for (int j = 0; j < cornerLocations.size(); ++j) {
                if (cornerLocations[j] < panelNum) {
                    startCornerIndex = cornerLocations[j];  // Last corner before the panel
                }
                if (cornerLocations[j] > panelNum) {
                    endCornerIndex = cornerLocations[j];  // First corner after the panel
                    break;
                }
            }
            //acutPrintf(_T(" Between %d."), startCornerIndex);
            if (endCornerIndex == -1) {
                endCornerIndex = panelNum + 1;
            }
            //acutPrintf(_T(" and %d."), endCornerIndex);


            if (prevStartCornerIndex != startCornerIndex) {
                movedCompensators = 0;
                prevStartCornerIndex = startCornerIndex;
            }

            // Validate the corner indices
            if (startCornerIndex == -1 || endCornerIndex == -1) {
                // No valid corners found; handle error
            }

            // Calculate the center index in wallPanels
            int centerIndex = (startCornerIndex + endCornerIndex) / 2;

            // Get positions of centerIndex and detectedPanel(compensator)
            AcGePoint3d centerPanelPosition = wallPanels[centerIndex + movedCompensators].position;

            AcGeVector3d direction = (wallPanels[panelNum].position - wallPanels[centerIndex].position).normal();

            // Adjust the position of the detected compensator panel
            wallPanels[panelNum].position = centerPanelPosition;
            if (wallPanels[panelNum].isOuterLoop && loopIsClockwise[wallPanels[panelNum].loopIndex]) {
                wallPanels[panelNum].position -= direction * wallPanels[centerIndex + movedCompensators].length;
                wallPanels[panelNum].position += direction * panelLength;
            }
            if (wallPanels[panelNum].isOuterLoop && !loopIsClockwise[wallPanels[panelNum].loopIndex]) {
                wallPanels[panelNum].position -= direction * wallPanels[centerIndex + movedCompensators].length;
                wallPanels[panelNum].position += direction * panelLength;
            }


            //acutPrintf(_T("\t | Moved to centerIndex = %d."), centerIndex + movedCompensators);
            for (int centerToCornerPanelNum = centerIndex + movedCompensators; centerToCornerPanelNum < panelNum - movedCompensators; centerToCornerPanelNum++) {
                wallPanels[centerToCornerPanelNum].position = wallPanels[centerToCornerPanelNum].position + direction * panelLength;
            }
            if (prevStartCornerIndex == startCornerIndex) {
                movedCompensators++;
            }
        }
    }

    //acutPrintf(_T("\ncornerLocations (size: %d): "), cornerLocations.size());
    //acutPrintf(_T("\ncornerLocations: "));
    for (size_t i = 0; i < cornerLocations.size(); ++i) {
        //acutPrintf(_T("%d "), cornerLocations[i]);
    }
    //acutPrintf(_T("\n"));

    wallHeight = globalVarHeight;
    currentHeight = globalVarHeight;

    // Fifth Pass: Place all wall panels
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
    int timberHeight;
    for (const auto& panel : wallPanels) {

        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
        pBlockRef->setPosition(panel.position);
        pBlockRef->setBlockTableRecord(panel.assetId);
        pBlockRef->setRotation(panel.rotation);
        pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

        if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
            acutPrintf(_T("\nFailed to place wall segment."));
        }
        pBlockRef->close();
        currentHeight = panel.height;
        timberHeight = panel.height;
        for (const auto& panel2 : panelSizes) {
            if (panel2.length == panel.length) {
                for (int panelNum = 0; panelNum < 3; panelNum++) {
                    AcDbObjectId assetId = loadAsset(panel2.id[panelNum].c_str());
                    if(assetId == AcDbObjectId::kNull) {
						acutPrintf(_T("\nFailed to load asset for panel %s."), panel2.id[panelNum].c_str());
					}

                    if (assetId != AcDbObjectId::kNull) {
                        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

                        for (int x = 0; x < numPanelsHeight; x++) {

                            AcGePoint3d currentPointWithHeight = panel.position;
                            currentPointWithHeight.z += currentHeight;

                            AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                            pBlockRef->setPosition(currentPointWithHeight);
                            pBlockRef->setBlockTableRecord(assetId);
                            pBlockRef->setRotation(panel.rotation);
                            pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

                            if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
                                acutPrintf(_T("\nFailed to place wall segment."));
                            }
                            pBlockRef->close();
                            currentHeight += panelHeights[panelNum];
                        }
                    }
                }
            }
        }
    }

    pModelSpace->close();
    pBlockTable->close();
    //acutPrintf(_T("\nCompleted placing walls."));

    // Sixth Pass: Place all timber
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return;
    }

    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return;
    }

    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return;
    }

    for (const auto& timb : timber) {
        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
        pBlockRef->setPosition(timb.position);
        pBlockRef->setBlockTableRecord(timb.assetId);
        pBlockRef->setRotation(timb.rotation);
        pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

        if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
            acutPrintf(_T("\nFailed to place wall segment."));
        }
        pBlockRef->close();
        currentHeight = timberHeight;
        for (const auto& panel2 : panelSizes) {
            for (int panelNum = 0; panelNum < 3; panelNum++) {
                AcDbObjectId assetId = loadAsset(panel2.id[panelNum].c_str());
                if (assetId == AcDbObjectId::kNull) {
                    acutPrintf(_T("\nFailed to load asset for panel %s."), panel2.id[panelNum].c_str());
                }

                if (assetId != AcDbObjectId::kNull) {
                    int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);
                    //acutPrintf(_T("\nnumPanelsHeight = %d"), numPanelsHeight);

                    for (int x = 0; x < numPanelsHeight; x++) {
                        //acutPrintf(_T("\ncurrentHeight = %d"), currentHeight);

                        AcGePoint3d currentPointWithHeight = timb.position;
                        currentPointWithHeight.z += currentHeight;
                        AcDbObjectId timberAssetId = TimberAssetCreator::createTimberAsset(timb.length, panelHeights[panelNum]);
                        if (timberAssetId == AcDbObjectId::kNull) {
                            acutPrintf(_T("\nFailed to create timber asset."));
                        }
                        else {
                            AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                            pBlockRef->setPosition(currentPointWithHeight);
                            pBlockRef->setBlockTableRecord(timberAssetId);
                            pBlockRef->setRotation(timb.rotation);
                            pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

                            if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
                                acutPrintf(_T("\nFailed to place timber."));
                            }
                            pBlockRef->close();
                            currentHeight += panelHeights[panelNum];
                        }
                    }
                }
            }
        }

        /*for (const auto& panel2 : panelSizes) {
            if (panel2.length == timb.length) {
                for (int panelNum = 0; panelNum < 3; panelNum++) {
                        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

                        for (int x = 0; x < numPanelsHeight; x++) {
                            AcDbObjectId timberAssetId = TimberAssetCreator::createTimberAsset(timb.length, panelHeights[panelNum]);
                            if (timberAssetId == AcDbObjectId::kNull) {
                                acutPrintf(_T("\nFailed to create timber asset."));
                            }
                            else {
                                AcGePoint3d currentPointWithHeight = timb.position;
                                currentPointWithHeight.z += currentHeight;

                                AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                                pBlockRef->setPosition(currentPointWithHeight);
                                pBlockRef->setBlockTableRecord(timberAssetId);
                                pBlockRef->setRotation(timb.rotation);
                                pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

                                if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
                                    acutPrintf(_T("\nFailed to place wall segment."));
                                }
                                pBlockRef->close();
                            }
                        }
                }
            }
        }*/
    }

    pModelSpace->close();
    pBlockTable->close();
    acutPrintf(_T("\nCompleted placing walls."));
}
