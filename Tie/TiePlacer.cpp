// Created by: Ani (2024-07-13)
// Modified by:
// TODO: Write the sub-function to select which tie to place(line 360)
// TiePlacer.cpp
/////////////////////////////////////////////////////////////////////////

#include "stdAfx.h"
#include "TiePlacer.h"
#include "SharedDefinations.h"  // For the shared definitions
#include "DefineScale.h"        // For globalVarScale
#include <vector>               // For the vector
#include <algorithm>            // For the algorithm
#include <tuple>                // For the tuple
#include "dbapserv.h"           // For acdbHostApplicationServices() and related services
#include "dbents.h"             // For AcDbBlockReference
#include "dbsymtb.h"            // For block table record definitions
#include "AssetPlacer/GeometryUtils.h" // For the geometry utilities
#include <array>
#include <cmath>
#include <map>
#include <thread>
#include <chrono>
#include "DefineHeight.h"
#include <string>

// Static member definition
std::map<AcGePoint3d, std::vector<AcGePoint3d>, TiePlacer::Point3dComparator> TiePlacer::wallMap;

bool isThisInteger(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

const double TOLERANCE = 0.1; // Define a small tolerance for angle comparisons

const int BATCH_SIZE = 1000; // Define the batch size for processing entities

double distanceBetweenPoly;

// Structure to hold panel information
struct Panel {
    int length;
    std::wstring id[2];
};

// Structure to hold tie information
struct Tie {
    int length;
    std::wstring id;
};

bool isCornerConcaveTie(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
    // Calculate cross product to determine corner type
    AcGeVector3d v1 = current - prev;
    AcGeVector3d v2 = next - current;
    double cross = v1.x * v2.y - v1.y * v2.x;

    double tolerance = 1e-6;

    bool isConcave = cross < -tolerance;

    //// Debugging information
    //acutPrintf(_T("\nChecking corner at (%f, %f): "), current.x, current.y);
    //acutPrintf(_T("Previous Point: (%f, %f), Next Point: (%f, %f)"), prev.x, prev.y, next.x, next.y);
    //acutPrintf(_T("Cross Product: %f, Identified as Concave: %d"), cross, isConcave);

    return isConcave;
}

bool isCornerConvexTie(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
    AcGeVector3d v1 = current - prev;
    AcGeVector3d v2 = next - current;
    double cross = v1.x * v2.y - v1.y * v2.x;

    // Tolerance to handle floating-point errors
    double tolerance = 1e-6;

    bool isConvex = cross > tolerance;

    //acutPrintf(_T("\nChecking corner at (%f, %f): Previous Point: (%f, %f), Next Point: (%f, %f)"),
    //    current.x, current.y, prev.x, prev.y, next.x, next.y);
    //acutPrintf(_T("Cross Product: %f, Identified as Convex: %d"), cross, isConvex);

    return isConvex;
}

//Detect polylines
std::vector<AcGePoint3d> TiePlacer::detectPolylines() {
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

//// Function to get the vertices of an AcDbPolyline
//std::vector<AcGePoint3d> getPolylineVertices(AcDbPolyline* pPolyline) {
//    std::vector<AcGePoint3d> vertices;
//    int numVerts = pPolyline->numVerts();
//    for (int i = 0; i < numVerts; ++i) {
//        AcGePoint3d point;
//        pPolyline->getPointAt(i, point);
//        vertices.push_back(point);
//    }
//    return vertices;
//}
//
//// Function to calculate the distance between corresponding vertices of two polylines
//double getPolylineDistance(AcDbPolyline* pPolyline1, AcDbPolyline* pPolyline2) {
//    std::vector<AcGePoint3d> vertices1 = getPolylineVertices(pPolyline1);
//    std::vector<AcGePoint3d> vertices2 = getPolylineVertices(pPolyline2);
//
//    if (vertices1.size() != vertices2.size()) {
//        acutPrintf(_T("\nThe polylines have different numbers of vertices."));
//        return -1.0;
//    }
//
//    for (size_t i = 0; i < vertices1.size(); ++i) {
//        double deltaX = vertices2[i].x - vertices1[i].x;
//        double deltaY = vertices2[i].y - vertices1[i].y;
//
//        if (std::abs(deltaX) == std::abs(deltaY)) {
//            return std::abs(deltaX); // Return the positive distance value
//        }
//    }
//
//    return -1.0; // Return -1 if no matching deltas are found
//}

// Function to get the wall panel positions and find the distance between polylines
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> TiePlacer::getWallPanelPositions() {
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> positions;

    // Get the working database
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return positions;
    }

    // Get the block table
    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return positions;
    }

    // Get the model space
    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return positions;
    }

    // Create an iterator for the model space
    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return positions;
    }

    AcDbPolyline* pFirstPolyline = nullptr;
    AcDbPolyline* pSecondPolyline = nullptr;

    // Iterate through the entities to find polylines
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                if (!pFirstPolyline) {
                    pFirstPolyline = AcDbPolyline::cast(pEnt);
                }
                else {
                    pSecondPolyline = AcDbPolyline::cast(pEnt);
                    pEnt->close();
                    break; // Found both polylines, no need to continue
                }
            }
            pEnt->close();
        }
    }

    // Calculate the distance between the first two polylines
    if (pFirstPolyline && pSecondPolyline) {
        double distance = getPolylineDistance(pFirstPolyline, pSecondPolyline);
        if (distance > 0) {
            distanceBetweenPoly = distance;
        }
        else {
            acutPrintf(_T("\nNo matching deltas found between polylines."));
        }
    }
    else {
        acutPrintf(_T("\nDid not find two polylines."));
    }

    // Close the iterator and model space
    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    return positions;
}

// Calculate position of the tie
std::vector<std::tuple<AcGePoint3d, double>> calculateTiePositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    return {};
}

// Load Tie Asset
AcDbObjectId TiePlacer::LoadTieAsset(const wchar_t* blockName) {
    //acutPrintf(L"\nLoading Tie Asset: %s", blockName);
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(L"\nFailed to get the working database");
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    if (Acad::eOk != pDb->getBlockTable(pBlockTable, AcDb::kForRead)) {
        acutPrintf(L"\nFailed to get the block table");
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        //acutPrintf(_T("\nBlock not found %s"), blockName);
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    //acutPrintf(L"\nLoaded block: %s", blockName);
    return blockId;
}

// Place Tie at Position
void TiePlacer::placeTieAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId) {
    
}

double crossProduct3(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool directionOfDrawing3(std::vector<AcGePoint3d>& points) {
    // Ensure the shape is closed
    if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
        points.push_back(points.front());
    }

    double totalTurns = 0.0;

    for (size_t i = 1; i < points.size() - 1; ++i) {
        totalTurns += crossProduct3(points[i - 1], points[i], points[i + 1]);
    }

    // If totalTurns is negative, the shape is drawn clockwise
    if (totalTurns < 0) {
        return true;  // Clockwise
    }
    // If totalTurns is positive, the shape is drawn counterclockwise
    else if (totalTurns > 0) {
        return false; // Counterclockwise
    }
    // Handle the case where totalTurns is zero (indicating an undefined direction)
    else {
        acutPrintf(_T("Warning: The shape does not have a defined direction. Defaulting to clockwise.\n"));
        return true;  // Default to clockwise if direction cannot be determined
    }
}

bool isThisClockwise(const AcGePoint3d& p0, const AcGePoint3d& p1, const AcGePoint3d& p2) {
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

//calculate distance between polylines
double TiePlacer::calculateDistanceBetweenPolylines() {
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

void TiePlacer::adjustStartAndEndPoints(AcGePoint3d& point, const AcGeVector3d& direction, double distanceBetweenPolylines, bool isInner) {
    if (isInner) {
        if (distanceBetweenPolylines == 150) {
            point += direction * 300;
        }
        else {
            point += direction * 250;
        }
    }
    else {
        // Using the provided table for outer loop adjustments
        int adjustment = 0;

        if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
            adjustment = 550;
            //acutPrintf(_T("\nDistance between polylines is 150 or 200."));
        }
        else if (distanceBetweenPolylines == 250) {
            adjustment = 600;
            //acutPrintf(_T("\nDistance between polylines is 250."));
        }
        else if (distanceBetweenPolylines == 300) {
            adjustment = 650;
            //acutPrintf(_T("\nDistance between polylines is 300."));
        }
        else if (distanceBetweenPolylines == 350) {
            adjustment = 700;
            //acutPrintf(_T("\nDistance between polylines is 350."));
        }
        else if (distanceBetweenPolylines == 400) {
            adjustment = 750;
            //acutPrintf(_T("\nDistance between polylines is 400."));
        }
        else if (distanceBetweenPolylines == 450) {
            adjustment = 800;
            //acutPrintf(_T("\nDistance between polylines is 450."));
        }
        else if (distanceBetweenPolylines == 500) {
            adjustment = 850;
            //acutPrintf(_T("\nDistance between polylines is 500."));
        }
        else if (distanceBetweenPolylines == 550) {
            adjustment = 900;
            //acutPrintf(_T("\nDistance between polylines is 550."));
        }
        else if (distanceBetweenPolylines == 600) {
            adjustment = 950;
            //acutPrintf(_T("\nDistance between polylines is 600."));
        }
        else if (distanceBetweenPolylines == 650) {
            adjustment = 1000;
            //acutPrintf(_T("\nDistance between polylines is 650."));
        }
        else if (distanceBetweenPolylines == 700) {
            adjustment = 1050;
            //acutPrintf(_T("\nDistance between polylines is 700."));
        }
        else if (distanceBetweenPolylines == 750) {
            adjustment = 1100;
            //acutPrintf(_T("\nDistance between polylines is 750."));
        }
        else if (distanceBetweenPolylines == 800) {
            adjustment = 1150;
            //acutPrintf(_T("\nDistance between polylines is 800."));
        }
        else if (distanceBetweenPolylines == 850) {
            adjustment = 1200;
            //acutPrintf(_T("\nDistance between polylines is 850."));
        }
        else if (distanceBetweenPolylines == 900) {
            adjustment = 1250;
            //acutPrintf(_T("\nDistance between polylines is 900."));
        }
        else if (distanceBetweenPolylines == 950) {
            adjustment = 1300;
            //acutPrintf(_T("\nDistance between polylines is 950."));
        }
        else if (distanceBetweenPolylines == 1000) {
            adjustment = 1350;
            //acutPrintf(_T("\nDistance between polylines is 1000."));
        }
        else if (distanceBetweenPolylines == 1050) {
            adjustment = 1400;
            //acutPrintf(_T("\nDistance between polylines is 1050."));
        }
        else if (distanceBetweenPolylines == 1100) {
            adjustment = 1450;
            //acutPrintf(_T("\nDistance between polylines is 1100."));
        }
        else if (distanceBetweenPolylines == 1150) {
            adjustment = 1500;
            //acutPrintf(_T("\nDistance between polylines is 1150."));
        }
        else if (distanceBetweenPolylines == 1200) {
            adjustment = 1550;
            //acutPrintf(_T("\nDistance between polylines is 1200."));
        }
        else if (distanceBetweenPolylines == 1250) {
            adjustment = 1600;
            //acutPrintf(_T("\nDistance between polylines is 1250."));
        }
        else if (distanceBetweenPolylines == 1300) {
            adjustment = 1650;
            //acutPrintf(_T("\nDistance between polylines is 1300."));
        }
        else if (distanceBetweenPolylines == 1350) {
            adjustment = 1700;
            //acutPrintf(_T("\nDistance between polylines is 1350."));
        }
        else if (distanceBetweenPolylines == 1400) {
            adjustment = 1750;
            //acutPrintf(_T("\nDistance between polylines is 1400."));
        }
        else if (distanceBetweenPolylines == 1450) {
            adjustment = 1800;
            //acutPrintf(_T("\nDistance between polylines is 1450."));
        }
        else if (distanceBetweenPolylines == 1500) {
            adjustment = 1850;
            //acutPrintf(_T("\nDistance between polylines is 1500."));
        }
        else if (distanceBetweenPolylines == 1550) {
            adjustment = 1900;
            //acutPrintf(_T("\nDistance between polylines is 1550."));
        }
        else if (distanceBetweenPolylines == 1600) {
            adjustment = 1950;
            //acutPrintf(_T("\nDistance between polylines is 1600."));
        }
        else if (distanceBetweenPolylines == 1650) {
            adjustment = 2000;
            //acutPrintf(_T("\nDistance between polylines is 1650."));
        }
        else if (distanceBetweenPolylines == 1700) {
            adjustment = 2050;
            //acutPrintf(_T("\nDistance between polylines is 1700."));
        }
        else if (distanceBetweenPolylines == 1750) {
            adjustment = 2100;
            //acutPrintf(_T("\nDistance between polylines is 1750."));
        }
        else if (distanceBetweenPolylines == 1800) {
            adjustment = 2150;
            //acutPrintf(_T("\nDistance between polylines is 1800."));
        }
        else if (distanceBetweenPolylines == 1850) {
            adjustment = 2200;
            //acutPrintf(_T("\nDistance between polylines is 1850."));
        }
        else if (distanceBetweenPolylines == 1900) {
            adjustment = 2250;
            //acutPrintf(_T("\nDistance between polylines is 1900."));
        }
        else if (distanceBetweenPolylines == 1950) {
            adjustment = 2300;
            //acutPrintf(_T("\nDistance between polylines is 1950."));
        }
        else if (distanceBetweenPolylines == 2000) {
            adjustment = 2350;
            //acutPrintf(_T("\nDistance between polylines is 2000."));
        }
        else if (distanceBetweenPolylines == 2050) {
            adjustment = 2400;
            //acutPrintf(_T("\nDistance between polylines is 2050."));
        }
        else if (distanceBetweenPolylines == 2100) {
            adjustment = 2450;
            //acutPrintf(_T("\nDistance between polylines is 2100."));
        }
        else adjustment = 150; // Default case for any unexpected distance value
        adjustment -= 100;
        point += direction * adjustment;
    }
}

// Place Ties
void TiePlacer::placeTies() {
    //acutPrintf(L"\nPlacing Ties");
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    //acutPrintf(_T("\nDistance between polylines: %f"), distanceBetweenPoly);
    if (panelPositions.empty()) {
        //acutPrintf(L"\nNo wall panels found");
    }

    double distanceBetweenPolylines = calculateDistanceBetweenPolylines();

    // List of available Tieswith their sizes
    std::vector<Tie> tieSizes = {
        {500, L"030005X"},
        {850, L"030010X"},
        {1000, L"030480X"},
        {1200, L"030490X"},
        {1500, L"030170X"},
        {1700, L"030020X"},
        {2500, L"030710X"},
        {3000, L"030720X"},
        {3500, L"030730X"},
        {6000, L"030160X"}
    };

    AcDbObjectId tieAssetId;
    AcDbObjectId tieAssetWalerId;
    const std::wstring wingnut = L"030110X";
    AcDbObjectId assetIdWingnut = LoadTieAsset(wingnut.c_str());

    //acutPrintf(_T("\nDistance Between Polylines : %d"), ((int)distanceBetweenPoly + 300));

    for (const auto& tie : tieSizes) {
        if (tie.length >= ((int)distanceBetweenPoly + 300)) {
            //acutPrintf(_T("\nSelected Tie Length: %d"), tie.length);
            tieAssetId = LoadTieAsset(tie.id.c_str());  // Replace ASSET_TIE with the actual asset name
            break;
        }
    }

    for (const auto& tie : tieSizes) {
        if (tie.length >= ((int)distanceBetweenPoly + 300 + 90)) {
           // acutPrintf(_T("\nSelected Tie Length: %d"), tie.length);
            tieAssetWalerId = LoadTieAsset(tie.id.c_str());  // Replace ASSET_TIE with the actual asset name
            break;
        }
    }

    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo polylines detected."));
        return;
    }

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

        if (!isThisInteger(direction.x) || !isThisInteger(direction.y)) {
            if (cornerNum < corners.size() - 1) {
                closeLoopCounter = -1;
                loopIndex = 1;
                firstLoopEnd = static_cast<int>(cornerNum);
            }
        }
    }

    // Debug
    //acutPrintf(_T("\nOuter loop is loop[%d]"), outerLoopIndexValue);
    //acutPrintf(_T("\nfirst loop end is %d"), firstLoopEnd);

    std::vector<AcGePoint3d> firstLoop(corners.begin(), corners.begin() + firstLoopEnd + 1);
    std::vector<AcGePoint3d> secondLoop(corners.begin() + firstLoopEnd + 1, corners.end());

    bool firstLoopIsClockwise = directionOfDrawing3(firstLoop);
    bool secondLoopIsClockwise = directionOfDrawing3(secondLoop);

    // Debug
    //acutPrintf(_T("\nfirst loop is clockwise: %d, second loop is clockwise: %d (1:true, 0:false)"), firstLoopIsClockwise, secondLoopIsClockwise);

    std::vector<bool> loopIsClockwise = {
        firstLoopIsClockwise,
        secondLoopIsClockwise
    };


    struct WallPanel {
        AcGePoint3d position;
        AcDbObjectId assetId;
        double rotation;
        double length;
        int height;
        int loopIndex;
        bool isOuterLoop;
        bool firstOrLast;
        bool waler;
    };
    struct CornerTie {
        AcGePoint3d position;
        AcDbObjectId assetId;
        double rotation;
        double length;
        int height;
        int loopIndex;
        bool isOuterLoop;
        bool firstOrLast;
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
    std::vector<CornerTie> cornerTie;
    std::vector<Timber> timber;


    loopIndex = 0;
    int loopIndexLastPanel = 0;
    closeLoopCounter = -1;
    double totalPanelsPlaced = 0;
    std::vector<int> cornerLocations;


    int wallHeight = globalVarHeight;
    int currentHeight = 0;
    int panelHeights[] = { 1350, 1200, 600 };

    // Structure to hold panel information
    struct Panel {
        int length;
        std::wstring id[3];
    };

    std::vector<Panel> panelSizes = {
        {600, {L"128282X", L"136096X", L"129839X"}},
        {450, {L"128283X", L"Null", L"129840X"}},
        {300, {L"128284X", L"Null", L"129841X"}},
        {150, {L"128285X", L"Null", L"129842X"}},
        {100, {L"128292X", L"Null", L"129884X"}},
        {50, {L"128287X", L"Null", L"129879X"}}
    };

    AcGePoint3d first_start;

    // Second Pass: Save all positions, asset IDs, and rotations
    for (int cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        cornerLocations.push_back(static_cast<int>(totalPanelsPlaced));
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        if (cornerNum == 0) {
            first_start = start;
            //acutPrintf(_T("\nFirst Start: %f, %f"), first_start.x, first_start.y);
        }
        AcGeVector3d direction = (end - start).normal();
        AcGeVector3d reverseDirection = (start - end).normal();

        if (!isThisInteger(direction.x) || !isThisInteger(direction.y)) {
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

        // Get previous and next corners
        AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
        AcGePoint3d current = corners[cornerNum];
        AcGePoint3d next;
        AcGePoint3d nextNext;
        if (cornerNum + 1 < corners.size()) {
            //acutPrintf(_T("\nIfTest"));
            next = corners[(cornerNum + 1) % corners.size()];
        }
        else {
            //acutPrintf(_T("\nElseTest"));
            next = corners[cornerNum + 1 - closeLoopCounter];
        }
        if (cornerNum + 2 < corners.size()) {
            nextNext = corners[(cornerNum + 2) % corners.size()]; //
        }
        else {
            nextNext = corners[cornerNum + 2 - (corners.size() / 2)];
        }

        bool isConcave = isCornerConcave(prev, current, next);
        bool isConvex = !isConcave && isCornerConvexTie(prev, current, next);

        // Flagging adjacent corners
        //bool isAdjacentConvex = false;
        //bool isAdjacentConcave = false;
        bool isAdjacentConcave = isCornerConcave(current, next, nextNext);
        bool isAdjacentConvex = !isAdjacentConcave && isCornerConvexTie(current, next, nextNext);

        if (isConvex) {
            // Flag previous and next corners as adjacent to a convex corner
            //acutPrintf(_T("\nConvex"));
            size_t prevIndex = (cornerNum + corners.size() - 1) % corners.size();
            size_t nextIndex = (cornerNum + 1) % corners.size();

            //isAdjacentConvex = true;
            // Set flag for adjacent corners
            //isAdjacentConcave = false;  // Reset any concave flag if the previous corner was marked incorrectly
        }
        else if (isConcave) {
            //acutPrintf(_T("\nConcave"));
            //isAdjacentConvex = false;  // Reset any convex flag if the previous corner was marked incorrectly
            // Flagging adjacent corners as adjacent to concave is not needed in this approach
        }

        bool prevClockwise = isThisClockwise(prev, start, end);
        bool nextClockwise = isThisClockwise(start, end, next);

        bool isInner = loopIndex != outerLoopIndexValue;
        bool isOuter = !isInner;  // Outer loop is the opposite of inner

        if (!loopIsClockwise[loopIndex]) {
            isInner = !isInner;
            isOuter = !isOuter;
        }





        AcGePoint3d currentPointWithHeight;
        double rotation;
        bool firstOrLast;
        int prevHeight;

        if ((isInner && loopIsClockwise[loopIndex]) || (isOuter && !loopIsClockwise[loopIndex])) {

            direction = (end - start).normal();
            reverseDirection = (start - end).normal();

            bool skipFirstTie = false;
            bool skipLastTie = false;
            if (!prevClockwise) {
                skipFirstTie = true;
            }
            if (!nextClockwise) {
                skipLastTie = true;
            }
            //// Adjust start point
            //if (loopIsClockwise[loopIndex]) {
            //    if (!isConvex && isInner) {
            //        adjustStartAndEndPoints(start, direction, distanceBetweenPolylines, isInner);
            //    }
            //    // Adjust end point
            //    if (!isAdjacentConvex && isInner) {
            //        adjustStartAndEndPoints(end, reverseDirection, distanceBetweenPolylines, isInner);
            //    }
            //}
            //else {
            //    if (isConvex && isInner) {
            //        adjustStartAndEndPoints(start, direction, distanceBetweenPolylines, isInner);
            //    }
            //    // Adjust end point
            //    if (isAdjacentConvex && isInner) {
 
            int adjustment = 0;

            if (isOuter) {
                isConvex = !isConvex;
                isAdjacentConvex = !isAdjacentConvex;
            }

            if (!isConvex) {
                if (distanceBetweenPolylines == 150) {
                    start += direction * 0;
                }
                else {
                    start += direction * 0;
                }
            }
            else {
                // Using the provided table for outer loop adjustments

                if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
                    adjustment = 500;
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

                adjustment -= 350;

                //if (!isInnerCorner) {
                //    // Flip the adjustment for outside corners
                //    adjustment = -adjustment;
                //}

                start += direction * adjustment;

            }


            if (!isAdjacentConvex) {
                //acutPrintf(_T("\nnot AdjacentConvex"));
                if (distanceBetweenPolylines == 150) {
                    end -= direction * 50;
                }
                else {
                    //acutPrintf(_T("\nnot AdjacentConvex ELSE"));
                    end -= direction * 0;
                }
            }
            else {
                // Using the provided table for outer loop adjustments

                //acutPrintf(_T("\n!!!!AdjacentConvex"));
                //acutPrintf(_T("\ndistanceBetweenPolylines %f"), distanceBetweenPolylines);
                if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
                    adjustment = 500;
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

                adjustment -= 300;

                //if (!isInnerCorner) {
                //    // Flip the adjustment for outside corners
                //    adjustment = -adjustment;
                //}

                //acutPrintf(_T("\nEnd: %f and %f"), end.x, end.y);
                end -= direction * adjustment;
            }




            //        adjustStartAndEndPoints(end, reverseDirection, distanceBetweenPolylines, isInner);
            //    }
            //}

            double distance = start.distanceTo(end) - 500;
            AcGePoint3d currentPoint = start + direction * 250;
            rotation = atan2(direction.y, direction.x);
            double panelLength;

            if (isOuter) {
                rotation += M_PI;
            }
            double skipedFirstTie = false;
            for (const auto& panel : panelSizes) {
                currentHeight = 0;

                for (int panelNum = 0; panelNum < 3; panelNum++) {
                    AcDbObjectId assetId = LoadTieAsset(panel.id[panelNum].c_str());

                    if (assetId != AcDbObjectId::kNull) {
                        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

                        if (numPanelsHeight > 0) {

                            int numPanels = static_cast<int>(distance / panel.length);
                            if (numPanels != 0) {
                                for (int i = 0; i < numPanels; i++) {
                                    currentPointWithHeight = currentPoint;
                                    currentPointWithHeight.z += currentHeight;
                                    if (isOuter) {
                                        currentPointWithHeight += direction * panel.length;
                                    }
                                    rotation = normalizeAngle(rotation);
                                    rotation = snapToExactAngle(rotation, TOLERANCE);
                                    firstOrLast = false;

                                    panelLength = panel.length;
                                    prevHeight = panelHeights[panelNum];
                                    wallPanels.push_back({ currentPointWithHeight, assetId, rotation, panelLength, panelHeights[panelNum], loopIndex, isOuter, firstOrLast, false });
                                    
                                    totalPanelsPlaced++;
                                    currentPoint += direction * panelLength;
                                    distance -= panelLength;
                                }
                            }
                            currentHeight = wallHeight;
                        }
                    }
                }
            }
            WallPanel lastPanel = wallPanels.back();
            CornerTie newTie = {
                lastPanel.position,
                lastPanel.assetId,
                lastPanel.rotation,
                lastPanel.length,
                lastPanel.height,
                lastPanel.loopIndex,
                lastPanel.isOuterLoop,
                lastPanel.firstOrLast
            };
            cornerTie.push_back(newTie);
            cornerTie.back().assetId = LoadTieAsset(panelSizes[3].id[0].c_str());
            cornerTie.back().length = panelSizes[3].length;
            if (outerLoopIndexValue == 0 && !loopIsClockwise[0]) {
                cornerTie.back().position -= direction * (start.distanceTo(end) - 500);
            }
            else if(!loopIsClockwise[1]){
                cornerTie.back().position -= direction * (start.distanceTo(end) - 500);
            }
            else {
                cornerTie.back().position += direction * wallPanels.back().length;
            }
        }
        loopIndex = loopIndexLastPanel;
    }

    // Third Pass: Adjust positions for specific asset IDs
    std::vector<AcDbObjectId> centerAssets = {
        LoadTieAsset(L"128285X"),
        LoadTieAsset(L"129842X"),
        LoadTieAsset(L"129879X"),
        LoadTieAsset(L"129884X"),
        LoadTieAsset(L"128287X"),
        LoadTieAsset(L"128292X")
    };

    int prevStartCornerIndex = -1;
    int movedCompensators = 0;

    for (int panelNum = 0; panelNum < totalPanelsPlaced; ++panelNum) {
        WallPanel& panel = wallPanels[panelNum];
        if (std::find(centerAssets.begin(), centerAssets.end(), panel.assetId) != centerAssets.end()) {

            // Find the two corner points between which the panel is placed
            int panelPosition = panelNum;
            WallPanel detectedPanel = wallPanels[panelPosition];
            AcGePoint3d detectedPanelPosition = detectedPanel.position;
            AcDbObjectId detectedPanelId = detectedPanel.assetId;

            double panelLength = wallPanels[panelPosition].length;

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
            if (endCornerIndex == -1) {
                endCornerIndex = panelNum + 1;
            }


            if (prevStartCornerIndex != startCornerIndex) {
                movedCompensators = 0;
                prevStartCornerIndex = startCornerIndex;
            }

            // Calculate the center index in wallPanels
            int centerIndex = (startCornerIndex + endCornerIndex) / 2;

            // Get positions of centerIndex and detectedPanel
            AcGePoint3d centerPanelPosition = wallPanels[centerIndex + movedCompensators].position;

            AcGeVector3d direction = (wallPanels[panelNum].position - wallPanels[centerIndex].position).normal();

            // Adjust the position of the detected panel
            wallPanels[panelNum].position = centerPanelPosition;
            if (wallPanels[panelNum].isOuterLoop && loopIsClockwise[wallPanels[panelNum].loopIndex]) {
                wallPanels[panelNum].position -= direction * wallPanels[centerIndex + movedCompensators].length;
                wallPanels[panelNum].position += direction * panelLength;
            }

            for (int centerToCornerPanelNum = centerIndex + movedCompensators; centerToCornerPanelNum < panelNum - movedCompensators; centerToCornerPanelNum++) {
                wallPanels[centerToCornerPanelNum].position = wallPanels[centerToCornerPanelNum].position + direction * panelLength;
            }
            if (prevStartCornerIndex == startCornerIndex) {
                movedCompensators++;
            }
            if ((loopIsClockwise[0] && outerLoopIndexValue == 1) || (loopIsClockwise[1] && outerLoopIndexValue == 0))
            {
                if (wallPanels[panelNum].length == 100) {
                    wallPanels[centerIndex].position -= direction * 50;
                    wallPanels[centerIndex].waler = true;
                }
            }
            else {
                if (wallPanels[panelNum].length == 100) {
                    wallPanels[centerIndex - 1].position += direction * 50;
                    wallPanels[centerIndex - 1].waler = true;
                }
            }
            
        }
    }

    wallHeight = globalVarHeight;
    currentHeight = globalVarHeight;

    // Fourth Pass: Place all ties
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

    int tieOffsetHeight[] = { 300, 1050 };
    double xOffset;
    if (loopIsClockwise[outerLoopIndexValue]) {
        xOffset = distanceBetweenPoly / 2; // X offset for the tie
    }
    else {
        xOffset = distanceBetweenPoly / 2; // X offset for the tie
    }
    double yOffset = 25; // Y offset for the tie
    double wingtieOffset = (distanceBetweenPoly + 200) / 2;
    double walerOffset = 45;
    AcGePoint3d wingnutPosition;
    double wingnutRotation;
    AcGePoint3d currentPointWithHeight;

    for (const auto& panel : wallPanels) {
        if (panel.length > 100 && !panel.firstOrLast) {
            int tiesToPlace = 2;
            if (panel.height == 600) {
                tiesToPlace = 1;
            }
            for (int tiePlaced = 0; tiePlaced < tiesToPlace; tiePlaced++) {
                // Place the tie without scaling
                currentPointWithHeight = panel.position;
                currentPointWithHeight.z += tieOffsetHeight[tiePlaced];
                
                switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                case 0: // 0 degrees TOP
                    currentPointWithHeight.x += yOffset;
                    currentPointWithHeight.y += xOffset;
                    break;
                case 1: // 90 degrees LEFT
                    currentPointWithHeight.x -= xOffset;
                    currentPointWithHeight.y += yOffset;
                    break;
                case 2: // 180 degrees BOTTOM
                    currentPointWithHeight.x -= yOffset;
                    currentPointWithHeight.y -= xOffset;
                    break;
                case 3: // 270 degrees RIGHT
                    currentPointWithHeight.x += xOffset;
                    currentPointWithHeight.y -= yOffset;
                    break;
                }

                AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                pBlockRef->setPosition(currentPointWithHeight);
                if (panel.waler) {
                    pBlockRef->setBlockTableRecord(tieAssetWalerId);
                }
                else {
                    pBlockRef->setBlockTableRecord(tieAssetId);
                }
                pBlockRef->setRotation(panel.rotation + M_PI_2);
                pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
                    acutPrintf(_T("\nFailed to place tie."));
                }
                pBlockRef->close();
                for (int wingnutNum = 0; wingnutNum < 2; wingnutNum++) {
                    AcDbBlockReference* pWingnutRef = new AcDbBlockReference();
                    wingnutPosition = currentPointWithHeight;
                    wingtieOffset = -wingtieOffset;
                    walerOffset = -walerOffset;
                    wingnutRotation = panel.rotation;
                    if (panel.waler) {
                        switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                        case 0: // 0 degrees TOP
                            wingnutPosition.y += (wingtieOffset + walerOffset);
                            wingnutRotation += M_PI;
                            break;
                        case 1: // 90 degrees LEFT
                            wingnutPosition.x += (wingtieOffset + walerOffset);
                            break;
                        case 2: // 180 degrees BOTTOM
                            wingnutPosition.y -= (wingtieOffset + walerOffset);
                            wingnutRotation += M_PI;
                            break;
                        case 3: // 270 degrees RIGHT
                            wingnutPosition.x -= (wingtieOffset + walerOffset);
                            break;
                        case -1:
                            break;
                        }
                    }
                    else {
                        switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                        case 0: // 0 degrees TOP
                            wingnutPosition.y += wingtieOffset;
                            wingnutRotation += M_PI;
                            break;
                        case 1: // 90 degrees LEFT
                            wingnutPosition.x += wingtieOffset;
                            break;
                        case 2: // 180 degrees BOTTOM
                            wingnutPosition.y -= wingtieOffset;
                            wingnutRotation += M_PI;
                            break;
                        case 3: // 270 degrees RIGHT
                            wingnutPosition.x -= wingtieOffset;
                            break;
                        case -1:
                            break;
                        }
                    }
                    

                    pWingnutRef->setPosition(wingnutPosition);
                    pWingnutRef->setBlockTableRecord(assetIdWingnut);
                    if (wingnutNum == 1) {
                        pWingnutRef->setRotation(wingnutRotation);  // Apply rotation
                    }
                    else {
                        pWingnutRef->setRotation(wingnutRotation + M_PI);  // Apply rotation
                    }
                    pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

                    if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                        //acutPrintf(_T("\nPlaced wingnut."));
                    }
                    else {
                        acutPrintf(_T("\nFailed to place wingnut."));
                    }
                    pWingnutRef->close();  // Decrement reference count
                }
            }
            currentHeight = panel.height;
            currentPointWithHeight.z += tieOffsetHeight[0];
            int tieOffsetHeight2[] = { 300, 750 };
            for (const auto& panel2 : panelSizes) {
                if (panel2.length == panel.length) {
                    for (int panelNum = 0; panelNum < 3; panelNum++) {
                        AcDbObjectId assetId = LoadTieAsset(panel2.id[panelNum].c_str());

                        if (assetId != AcDbObjectId::kNull) {
                            int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

                            for (int x = 0; x < numPanelsHeight; x++) {

                                for (int tiePlaced = 0; tiePlaced + (panelNum/2) < 2; tiePlaced++) {
                                    currentPointWithHeight.z += tieOffsetHeight2[tiePlaced];

                                    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                                    pBlockRef->setPosition(currentPointWithHeight);
                                    if (panel.waler) {
                                        pBlockRef->setBlockTableRecord(tieAssetWalerId);
                                    }
                                    else {
                                        pBlockRef->setBlockTableRecord(tieAssetId);
                                    }
                                    pBlockRef->setRotation(panel.rotation + M_PI_2);
                                    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

                                    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
                                        acutPrintf(_T("\nFailed to place tie."));
                                    }
                                    pBlockRef->close();

                                    for (int wingnutNum = 0; wingnutNum < 2; wingnutNum++) {
                                        AcDbBlockReference* pWingnutRef = new AcDbBlockReference();
                                        wingnutPosition = currentPointWithHeight;
                                        wingtieOffset = -wingtieOffset;
                                        walerOffset = -walerOffset;
                                        wingnutRotation = panel.rotation;
                                        if (panel.waler) {
                                            switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                                            case 0: // 0 degrees TOP
                                                wingnutPosition.y += (wingtieOffset + walerOffset);
                                                wingnutRotation += M_PI;
                                                break;
                                            case 1: // 90 degrees LEFT
                                                wingnutPosition.x += (wingtieOffset + walerOffset);
                                                break;
                                            case 2: // 180 degrees BOTTOM
                                                wingnutPosition.y -= (wingtieOffset + walerOffset);
                                                wingnutRotation += M_PI;
                                                break;
                                            case 3: // 270 degrees RIGHT
                                                wingnutPosition.x -= (wingtieOffset + walerOffset);
                                                break;
                                            case -1:
                                                break;
                                            }
                                        }
                                        else {
                                            switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                                            case 0: // 0 degrees TOP
                                                wingnutPosition.y += wingtieOffset;
                                                wingnutRotation += M_PI;
                                                break;
                                            case 1: // 90 degrees LEFT
                                                wingnutPosition.x += wingtieOffset;
                                                break;
                                            case 2: // 180 degrees BOTTOM
                                                wingnutPosition.y -= wingtieOffset;
                                                wingnutRotation += M_PI;
                                                break;
                                            case 3: // 270 degrees RIGHT
                                                wingnutPosition.x -= wingtieOffset;
                                                break;
                                            case -1:
                                                break;
                                            }
                                        }
                                        pWingnutRef->setPosition(wingnutPosition);
                                        pWingnutRef->setBlockTableRecord(assetIdWingnut);
                                        if (wingnutNum == 1) {
                                            pWingnutRef->setRotation(wingnutRotation);  // Apply rotation
                                        }
                                        else {
                                            pWingnutRef->setRotation(wingnutRotation + M_PI);  // Apply rotation
                                        }
                                        pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

                                        if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                                            //acutPrintf(_T("\nPlaced wingnut."));
                                        }
                                        else {
                                            acutPrintf(_T("\nFailed to place wingnut."));
                                        }
                                        pWingnutRef->close();  // Decrement reference count

                                    }
                                }
                                currentPointWithHeight.z += tieOffsetHeight[0];
                                
                            }
                            if (numPanelsHeight != 0) {
                                currentHeight += panelHeights[panelNum];
                            }
                            
                        }
                    }
                }
            }
        }
    }

    prevStartCornerIndex = -1;
    movedCompensators = 0;



    // Place corner ties
    for (const auto& panel : cornerTie) {
        if (panel.length > 100 && !panel.firstOrLast) {
            int tiesToPlace = 2;
            if (panel.height == 600) {
                tiesToPlace = 1;
            }
            for (int tiePlaced = 0; tiePlaced < tiesToPlace; tiePlaced++) {
                // Place the tie without scaling
                currentPointWithHeight = panel.position;
                currentPointWithHeight.z += tieOffsetHeight[tiePlaced];

                switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                case 0: // 0 degrees TOP
                    currentPointWithHeight.x += yOffset;
                    currentPointWithHeight.y += xOffset;
                    break;
                case 1: // 90 degrees LEFT
                    currentPointWithHeight.x -= xOffset;
                    currentPointWithHeight.y += yOffset;
                    break;
                case 2: // 180 degrees BOTTOM
                    currentPointWithHeight.x -= yOffset;
                    currentPointWithHeight.y -= xOffset;
                    break;
                case 3: // 270 degrees RIGHT
                    currentPointWithHeight.x += xOffset;
                    currentPointWithHeight.y -= yOffset;
                    break;
                }
                AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                pBlockRef->setPosition(currentPointWithHeight);
                pBlockRef->setBlockTableRecord(tieAssetId);
                pBlockRef->setRotation(panel.rotation + M_PI_2);
                pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
                    acutPrintf(_T("\nFailed to place tie."));
                }
                pBlockRef->close();
                for (int wingnutNum = 0; wingnutNum < 2; wingnutNum++) {
                    AcDbBlockReference* pWingnutRef = new AcDbBlockReference();
                    wingnutPosition = currentPointWithHeight;
                    wingtieOffset = -wingtieOffset;
                    wingnutRotation = panel.rotation;
                    switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                    case 0: // 0 degrees TOP
                        wingnutPosition.y += wingtieOffset;
                        wingnutRotation += M_PI;
                        break;
                    case 1: // 90 degrees LEFT
                        wingnutPosition.x += wingtieOffset;
                        break;
                    case 2: // 180 degrees BOTTOM
                        wingnutPosition.y -= wingtieOffset;
                        wingnutRotation += M_PI;
                        break;
                    case 3: // 270 degrees RIGHT
                        wingnutPosition.x -= wingtieOffset;
                        break;
                    case -1:
                        break;
                    }

                    pWingnutRef->setPosition(wingnutPosition);
                    pWingnutRef->setBlockTableRecord(assetIdWingnut);
                    if (wingnutNum == 1) {
                        pWingnutRef->setRotation(wingnutRotation);  // Apply rotation
                    }
                    else {
                        pWingnutRef->setRotation(wingnutRotation + M_PI);  // Apply rotation
                    }
                    pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

                    if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                        //acutPrintf(_T("\nPlaced wingnut."));
                    }
                    else {
                        acutPrintf(_T("\nFailed to place wingnut."));
                    }
                    pWingnutRef->close();  // Decrement reference count
                }
            }
            currentHeight = panel.height;
            currentPointWithHeight.z += tieOffsetHeight[0];
            int tieOffsetHeight2[] = { 300, 750 };
            for (const auto& panel2 : panelSizes) {
                if (panel2.length == panel.length) {
                    for (int panelNum = 0; panelNum < 3; panelNum++) {
                        AcDbObjectId assetId = LoadTieAsset(panel2.id[panelNum].c_str());

                        if (assetId != AcDbObjectId::kNull) {
                            int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

                            for (int x = 0; x < numPanelsHeight; x++) {

                                for (int tiePlaced = 0; tiePlaced + (panelNum / 2) < 2; tiePlaced++) {
                                    currentPointWithHeight.z += tieOffsetHeight2[tiePlaced];

                                    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                                    pBlockRef->setPosition(currentPointWithHeight);
                                    pBlockRef->setBlockTableRecord(tieAssetId);
                                    pBlockRef->setRotation(panel.rotation + M_PI_2);
                                    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

                                    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
                                        acutPrintf(_T("\nFailed to place tie."));
                                    }

                                    pBlockRef->close();

                                    for (int wingnutNum = 0; wingnutNum < 2; wingnutNum++) {
                                        AcDbBlockReference* pWingnutRef = new AcDbBlockReference();
                                        wingnutPosition = currentPointWithHeight;
                                        wingtieOffset = -wingtieOffset;
                                        wingnutRotation = panel.rotation;
                                        switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                                        case 0: // 0 degrees TOP
                                            wingnutPosition.y += wingtieOffset;
                                            wingnutRotation += M_PI;
                                            break;
                                        case 1: // 90 degrees LEFT
                                            wingnutPosition.x += wingtieOffset;
                                            break;
                                        case 2: // 180 degrees BOTTOM
                                            wingnutPosition.y -= wingtieOffset;
                                            wingnutRotation += M_PI;
                                            break;
                                        case 3: // 270 degrees RIGHT
                                            wingnutPosition.x -= wingtieOffset;
                                            break;
                                        case -1:
                                            break;
                                        }
                                        pWingnutRef->setPosition(wingnutPosition);
                                        pWingnutRef->setBlockTableRecord(assetIdWingnut);
                                        if (wingnutNum == 1) {
                                            pWingnutRef->setRotation(wingnutRotation);  // Apply rotation
                                        }
                                        else {
                                            pWingnutRef->setRotation(wingnutRotation + M_PI);  // Apply rotation
                                        }
                                        pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

                                        if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                                            //acutPrintf(_T("\nPlaced wingnut."));
                                        }
                                        else {
                                            acutPrintf(_T("\nFailed to place wingnut."));
                                        }
                                        pWingnutRef->close();  // Decrement reference count

                                    }
                                }
                                currentPointWithHeight.z += tieOffsetHeight[0];

                            }
                            if (numPanelsHeight != 0) {
                                currentHeight += panelHeights[panelNum];
                            }

                        }
                    }
                }
            }
        }
    }

    pModelSpace->close();
    pBlockTable->close();

    acutPrintf(L"\nTies placed successfully");
}
