// Created by: Ani (2024-07-13)
// Modified by:
// TODO: Write the sub-function to select which tie to place, can refer to WallAssetPlacer.cpp for reference
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

// Static member definition
std::map<AcGePoint3d, std::vector<AcGePoint3d>, TiePlacer::Point3dComparator> TiePlacer::wallMap;

bool isThisInteger(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

const double TOLERANCE = 0.1; // Define a small tolerance for angle comparisons

const int BATCH_SIZE = 10; // Define the batch size for processing entities

// Structure to hold panel information
struct Panel {
    int length;
    std::wstring id[2];
};

//Detect polylines
std::vector<AcGePoint3d> TiePlacer::detectPolylines() {
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



// Function to get the vertices of an AcDbPolyline
std::vector<AcGePoint3d> getPolylineVertices(AcDbPolyline* pPolyline) {
    std::vector<AcGePoint3d> vertices;
    int numVerts = pPolyline->numVerts();
    for (int i = 0; i < numVerts; ++i) {
        AcGePoint3d point;
        pPolyline->getPointAt(i, point);
        vertices.push_back(point);
    }
    return vertices;
}

// Function to calculate the distance between corresponding vertices of two polylines
double getPolylineDistance(AcDbPolyline* pPolyline1, AcDbPolyline* pPolyline2) {
    std::vector<AcGePoint3d> vertices1 = getPolylineVertices(pPolyline1);
    std::vector<AcGePoint3d> vertices2 = getPolylineVertices(pPolyline2);

    if (vertices1.size() != vertices2.size()) {
        acutPrintf(_T("\nThe polylines have different numbers of vertices."));
        return -1.0;
    }

    for (size_t i = 0; i < vertices1.size(); ++i) {
        double deltaX = vertices2[i].x - vertices1[i].x;
        double deltaY = vertices2[i].y - vertices1[i].y;

        if (std::abs(deltaX) == std::abs(deltaY)) {
            return std::abs(deltaX); // Return the positive distance value
        }
    }

    return -1.0; // Return -1 if no matching deltas are found
}

// Function to get the wall panel positions and find the distance between polylines
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> TiePlacer::getWallPanelPositions() {
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

    AcDbPolyline* pFirstPolyline = nullptr;
    AcDbPolyline* pSecondPolyline = nullptr;

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                if (pFirstPolyline == nullptr) {
                    pFirstPolyline = AcDbPolyline::cast(pEnt);
                }
                else {
                    pSecondPolyline = AcDbPolyline::cast(pEnt);
                    break; // Found both polylines, no need to continue
                }
            }
            pEnt->close();
        }
    }

    if (pFirstPolyline && pSecondPolyline) {
        double distance = getPolylineDistance(pFirstPolyline, pSecondPolyline);
        if (distance > 0) {
            acutPrintf(_T("\nDistance between polylines: %f"), distance);
        }
        else {
            acutPrintf(_T("\nNo matching deltas found between polylines."));
        }
    }
    else {
        acutPrintf(_T("\nDid not find two polylines."));
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    return positions;
}

// Calculate position of the tie
std::vector<std::tuple<AcGePoint3d, double>> calculateTiePositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double>> tiePositions;
    acutPrintf(L"\n Debug: calculateTiePositions");
    // Define offsets here
    double xOffset = 20.0; // Define the x offset
    double yOffset = 2.5; // Define the y offset
    std::array<double, 2> zOffset = { 30.0, 105.0 }; // Define the z offset array

    for (const auto& panelPositions : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPositions);
        std::wstring panelName = std::get<1>(panelPositions);
        double rotation = std::get<2>(panelPositions);

        // Adjust rotation by adding 90 degrees (pi/2 radians)
        rotation += M_PI_2;

        int tieCount = 2;

        for (int i = 0; i < tieCount; ++i) {
            AcGePoint3d tiePos = pos;

            // Adjust positions based on the rotation and apply any offset if required
            switch (static_cast<int>(round(rotation / M_PI_2))) {
            case 1: // 90 degrees (top)
                tiePos.x += yOffset;
                tiePos.y += xOffset;
                tiePos.z += zOffset[i];
                break;
            case 2: // 180 degrees(left)
                tiePos.x -= xOffset;
                tiePos.y -= yOffset;
                tiePos.z += zOffset[i];
                break;
            case 3: // 270 degrees (bottom)
                tiePos.x -= yOffset;
                tiePos.y -= xOffset;
                tiePos.z += zOffset[i];
                break;
            case 4: // 360 degrees(right)
                tiePos.x += xOffset;
                tiePos.y -= yOffset;
                tiePos.z += zOffset[i];
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle: %f"), rotation);
                break;
            }
            // Print Debug Information
            // acutPrintf(_T("\nTie position: (%f, %f, %f)"), tiePos.x, tiePos.y, tiePos.z);
            // acutPrintf(_T("\nTie rotation: %f"), rotation);

            tiePositions.emplace_back(std::make_tuple(tiePos, rotation));
        }
    }

    return tiePositions;
}

// Load Tie Asset
AcDbObjectId TiePlacer::LoadTieAsset(const wchar_t* blockName) {
    acutPrintf(L"\nLoading Tie Asset: %s", blockName);
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
        acutPrintf(_T("\nBlock not found %s"), blockName);
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    acutPrintf(L"\nLoaded block: %s", blockName);
    return blockId;
}

// Place Tie at Position
void TiePlacer::placeTieAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(L"\nFailed to get the working database");
        return;
    }

    AcDbBlockTable* pBlockTable;
    if (Acad::eOk != pDb->getBlockTable(pBlockTable, AcDb::kForRead)) {
        acutPrintf(L"\nFailed to get the block table");
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(L"\nFailed to get the model space");
        pBlockTable->close();
        return;
    }

    // Print the values of position, assetId, and rotation
    // acutPrintf(L"\nPosition: (%f, %f, %f)", position.x, position.y, position.z);
    // acutPrintf(L"\nRotation: %f", rotation);
    // acutPrintf(L"\nAsset ID: %llu", static_cast<unsigned long long>(assetId.asOldId()));

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(assetId);
    pBlockRef->setRotation(rotation);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Set the scale factor

    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
        acutPrintf(_T("\nFailed to append block reference."));
    }
    else {
        // acutPrintf(_T("\nFailed to place tie."));
    }

    pBlockRef->close();
    pModelSpace->close();
    pBlockTable->close();
}

// Place Ties
void TiePlacer::placeTies() {
    acutPrintf(L"\nPlacing Ties");
    /*std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(L"\nNo wall panels found");
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double>> tiePositions = calculateTiePositions(panelPositions);*/
    AcDbObjectId assetId = LoadTieAsset(ASSET_030005.c_str());  // Replace ASSET_TIE with the actual asset name

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(L"\nFailed to load the tie asset");
        return;
    }

    /*for (const auto& tiePos : tiePositions) {
        placeTieAtPosition(std::get<0>(tiePos), std::get<1>(tiePos), assetId);
    }*/


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
        if (isThisInteger(direction.x) && isThisInteger(direction.y)) {
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
        if (isThisInteger(direction.x) && isThisInteger(direction.y)) {
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
        int panelSizes[] = {75, 60, 45, 30, 15, 10, 5};
        
        // List of available ties
        /*std::vector<Panel> panelSizes = {
            // {90, {L"128280X", L"129837X"}}, // ONLY ENABLE FOR 90 PANELS
            {75, {L"128281X", L"129838X"}},
            {60, {L"128282X", L"129839X"}},
            {45, {L"128283X", L"129840X"}},
            {30, {L"128284X", L"129841X"}},
            {15, {L"128285X", L"129842X"}},
            {10, {L"128292X", L"129884X"}}, // *10 Compensator move to middle TODO:
            {5, {L"128287X", L"129879X"}} // *5 Compensator add a break
        };*/

        // Iterate through every panel type
        for (int panelSize = 0; panelSize < sizeof(panelSizes); panelSize++) {
            currentHeight = 0;
            AcGePoint3d backupCurrentPoint = currentPoint;
            double backupDistance = distance;

            // Iterate through 135 and 60 height
            for (int panelNum = 0; panelNum < 2; panelNum++) {

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
                    int numPanels = static_cast<int>(distance / panelSizes[panelSize]);  // Calculate the number of panels that fit horizontally
                    int numOfWallSegmentsPlaced = 0;
                    for (int i = 0; i < numPanels; i++) {

                        // Place the wall segment without scaling
                        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                        AcGePoint3d currentPointWithHeight = currentPoint;
                        currentPointWithHeight.z += currentHeight;
                        if (loopIndex == outerLoopIndexValue) {
                            currentPointWithHeight += direction * panelSizes[panelSize];
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

                        currentPoint += direction * panelSizes[panelSize];  // Move to the next panel
                        distance -= panelSizes[panelSize];
                    }
                    //acutPrintf(_T("\n%d wall segments placed successfully."), numOfWallSegmentsPlaced);
                    currentHeight += panelHeights[panelNum];
                }

                /*AcDbObjectId assetId = LoadTieAsset(panel.id[panelNum].c_str());
                //acutPrintf(_T("\nPanel length: %d,"), panel.length); // Debug

                if (assetId == AcDbObjectId::kNull) {
                    acutPrintf(_T("\nFailed to load asset."));
                }
                else {
                    
                }*/
            }
        }
        /*if (!(loopIndex == outerLoopIndexValue) && loopIndexLastPanel == outerLoopIndexValue) {
            loopIndexLastPanel = 1;
        }*/
        loopIndex = loopIndexLastPanel;
    }
    acutPrintf(L"\nTies placed successfully");
}
