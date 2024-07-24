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
    // If cross product z-component is positive, the turn is counterclockwise
    // If cross product z-component is negative, the turn is clockwise
    return crossProduct.z > 0;
}

void WallPlacer::placeWalls() {
    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo polylines detected."));
        return;
    }

    int closeLoopCounter = -1;
    int loopIndex = 0;
    double outerPointCounter = corners[0].x;
    int outerLoopIndexValue = 0;

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
            }
        }
    }

    struct WallPanel {
        AcGePoint3d position;
        AcDbObjectId assetId;
        double rotation;
        double length;
        bool isOuterLoop;
    };

    std::vector<WallPanel> wallPanels;
    std::vector<std::pair<AcGePoint3d, AcGePoint3d>> segments;

    loopIndex = 0;
    int loopIndexLastPanel = 0;
    closeLoopCounter = -1;
    double totalPanelsPlaced = 0;
    std::vector<int> cornerLocations;

    // Second Pass: Save all positions, asset IDs, and rotations
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {

        acutPrintf(_T("\ntotalPanelsPlaced= %d."), static_cast<int>(totalPanelsPlaced));
        closeLoopCounter++;
        cornerLocations.push_back(static_cast<int>(totalPanelsPlaced));
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[(cornerNum + 1) % corners.size()];
        AcGeVector3d direction = (end - start).normal();

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

        // Get previous and next corners
        AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
        AcGePoint3d next = corners[(cornerNum + 2) % corners.size()];

        bool prevClockwise = isClockwise(prev, start, end);
        bool nextClockwise = isClockwise(start, end, next);

        bool isInner = loopIndex != outerLoopIndexValue;
        bool isOuter = !isInner;  // Outer loop is the opposite of inner

        // Adjust start point
        if (prevClockwise && isInner) {
            start -= direction * 10;
        }
        else if (prevClockwise && isOuter) {
            start += direction * 10;
        }

        // Adjust end point
        if (nextClockwise && isInner) {
            end += direction * 10;
        }
        else if (nextClockwise && isOuter) {
            end -= direction * 10;
        }

        double distance = start.distanceTo(end) - 50;
        direction = (end - start).normal();
        AcGePoint3d currentPoint = start + direction * 25;
        double rotation = atan2(direction.y, direction.x);
        double panelLength;

        if (isOuter) {
            distance += 20;
            currentPoint -= direction * 10;
            rotation += M_PI;
        }

        int wallHeight = globalVarHeight;
        int currentHeight = 0;
        int panelHeights[] = { 135, 120, 60 };

        std::vector<Panel> panelSizes = {
            {60, {L"128282X", L"136096X", L"129839X"}},
            {45, {L"128283X", L"Null", L"129840X"}},
            {30, {L"128284X", L"Null", L"129841X"}},
            {15, {L"128285X", L"Null", L"129842X"}},
            {10, {L"128292X", L"Null", L"129884X"}},
            {5, {L"128287X", L"Null", L"129879X"}}
        };

        for (const auto& panel : panelSizes) {
            currentHeight = 0;
            AcGePoint3d backupCurrentPoint = currentPoint;
            double backupDistance = distance;

            for (int panelNum = 0; panelNum < 2; panelNum++) {
                AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());

                if (assetId != AcDbObjectId::kNull) {
                    int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

                    for (int x = 0; x < numPanelsHeight; x++) {
                        currentPoint = backupCurrentPoint;
                        distance = backupDistance;

                        int numPanels = static_cast<int>(distance / panel.length);
                        for (int i = 0; i < numPanels; i++) {
                            AcGePoint3d currentPointWithHeight = currentPoint;
                            currentPointWithHeight.z += currentHeight;
                            if (isOuter) {
                                currentPointWithHeight += direction * panel.length;
                            }
                            rotation = normalizeAngle(rotation);
                            rotation = snapToExactAngle(rotation, TOLERANCE);

                            panelLength = panel.length;
                            wallPanels.push_back({ currentPointWithHeight, assetId, rotation, panelLength, isOuter });

                            totalPanelsPlaced++;
                            currentPoint += direction * panelLength;
                            distance -= panelLength;
                        }
                        // Place timber for remaining distance
                        if (distance > 0 && distance < 5) {
                            acutPrintf(_T("\nPlacing timber at distance: %f, height: %d"), distance, panelHeights[panelNum]);
                            AcDbObjectId timberAssetId = TimberAssetCreator::createTimberAsset(distance, panelHeights[panelNum]);
                            if (timberAssetId == AcDbObjectId::kNull) {
                                acutPrintf(_T("\nFailed to create timber asset."));
                            }
                            else {

                                wallPanels.push_back({ currentPoint, timberAssetId, rotation, 0, isOuter });

                                /*AcDbBlockReference* pTimberRef = new AcDbBlockReference();
                                AcGePoint3d timberPosition = currentPoint;
                                timberPosition.z += currentHeight;
                                pTimberRef->setPosition(timberPosition);
                                pTimberRef->setBlockTableRecord(timberAssetId);
                                pTimberRef->setRotation(rotation);
                                pTimberRef->setScaleFactors(AcGeScale3d(globalVarScale));

                                if (pModelSpace->appendAcDbEntity(pTimberRef) == Acad::eOk) {
                                    acutPrintf(_T("\nTimber placed successfully."));
                                }
                                else {
                                    acutPrintf(_T("\nFailed to place timber."));
                                }
                                pTimberRef->close();*/
                            }
                        }

                        //acutPrintf(_T("\n%d wall segments placed successfully."), numOfWallSegmentsPlaced);
                        currentHeight += panelHeights[panelNum];
                    }
                }
            }
        }
        segments.push_back(std::make_pair(start, end)); // Save segment for later compensator placement
        loopIndex = loopIndexLastPanel;
    }
    acutPrintf(_T("\ntotalPanelsPlaced= %d."), static_cast<int>(totalPanelsPlaced));

    // Third Pass: Adjust positions for specific asset IDs
    std::vector<AcDbObjectId> centerAssets = {
        loadAsset(L"128285X"),
        loadAsset(L"129842X"),
        loadAsset(L"129879X"),
        loadAsset(L"129884X"),
        loadAsset(L"128287X"),
        loadAsset(L"128292X")
    };


    acutPrintf(_T("\ntotalPanelsPlaced= %d."), static_cast<int>(totalPanelsPlaced));
    for (int panelNum = 0; panelNum < totalPanelsPlaced; ++panelNum) {
        WallPanel& panel = wallPanels[panelNum];
        if (std::find(centerAssets.begin(), centerAssets.end(), panel.assetId) != centerAssets.end()) {
            // Find the two corner points between which the panel is placed
            int panelPosition = panelNum;  // This should be the index of the panel
            acutPrintf(_T("\nFound 5, 10 or 15 at %d."), panelNum);
            WallPanel detectedPanel = wallPanels[panelPosition];
            AcGePoint3d detectedPanelPosition = detectedPanel.position;
            AcDbObjectId detectedPanelId = detectedPanel.assetId;

            double panelLength = wallPanels[panelPosition].length;


            acutPrintf(_T("\npanelLength = %f."), panelLength);

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
            acutPrintf(_T(", between %d."), startCornerIndex);
            if (endCornerIndex == -1) {
                endCornerIndex = panelNum + 1;
            }
            acutPrintf(_T(" and %d."), endCornerIndex);

            // Validate the corner indices
            if (startCornerIndex == -1 || endCornerIndex == -1) {
                // No valid corners found; handle error
            }

            // Calculate the center index in wallPanels
            int centerIndex = (startCornerIndex + endCornerIndex) / 2;

            // Get positions of centerIndex and detectedPanel
            AcGePoint3d centerPanelPosition = wallPanels[centerIndex].position;

            AcGeVector3d direction = (wallPanels[panelNum].position - wallPanels[centerIndex].position).normal();

            // Adjust the position of the detected panel
            wallPanels[panelNum].position = centerPanelPosition;
            if (wallPanels[panelNum].isOuterLoop) {
                wallPanels[panelNum].position -= direction * wallPanels[centerIndex].length;
                wallPanels[panelNum].position += direction * panelLength;
            }


            acutPrintf(_T("\ncenterIndex = %d."), centerIndex);
            acutPrintf(_T(" < panelNum = %d."), panelNum);
            for (int centerToCornerPanelNum = centerIndex; centerToCornerPanelNum < panelNum; centerToCornerPanelNum++) {
                wallPanels[centerToCornerPanelNum].position = wallPanels[centerToCornerPanelNum].position + direction * panelLength;
            }

        }
    }

    acutPrintf(_T("\ncornerLocations (size: %d): "), cornerLocations.size());
    acutPrintf(_T("\ncornerLocations: "));
    for (size_t i = 0; i < cornerLocations.size(); ++i) {
        acutPrintf(_T("%d "), cornerLocations[i]);
    }
    acutPrintf(_T("\n"));

    // Fourth Pass: Place all wall panels
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
    }

    pModelSpace->close();
    pBlockTable->close();
    acutPrintf(_T("\nCompleted placing walls."));
}

