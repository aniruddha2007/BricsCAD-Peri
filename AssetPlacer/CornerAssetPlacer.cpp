// Created by:Ani  (2024-05-31)
// Modified by:Ani (2024-07-04)
// TODO: fix extra corner post at last vertice
// Added 5 panel width
// CornerAssetPlacer.cpp
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

double normalizeAngle(double angle) {
    while (angle < 0) {
        angle += 2 * M_PI;
    }
    while (angle >= 2 * M_PI) {
        angle -= 2 * M_PI;
    }
    return angle;
}

//bool areAnglesEqual(double angle1, double angle2, double tolerance) {
//    return std::abs(angle1 - angle2) < tolerance;
//}

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

        // Additional checks for better diagnostics
        acutPrintf(_T("\nChecking if model space exists in block table.\n"));
        if (pBlockTable->has(ACDB_MODEL_SPACE)) {
            acutPrintf(_T("\nModel space exists.\n"));
        }
        else {
            acutPrintf(_T("\nModel space does not exist.\n"));
        }

        acutPrintf(_T("\nAttempting to recreate model space.\n"));
        if (!recreateModelSpace(pDb)) {
            acutPrintf(_T("\nFailed to recreate model space.\n"));
            pBlockTable->close();
            return corners;
        }

        // Retry accessing the newly created model space
        es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead);
        if (es != Acad::eOk) {
            acutPrintf(_T("\nFailed to get new model space. Error status: %d\n"), es);
            pBlockTable->close();
            return corners;
        }
    }

    int entityCount = 0;
    while (true) {
        AcDbBlockTableRecordIterator* pIter;
        es = pModelSpace->newIterator(pIter);
        if (es != Acad::eOk) {
            acutPrintf(_T("\nFailed to create iterator. Error status: %d\n"), es);
            pModelSpace->close();
            pBlockTable->close();
            return corners;
        }

        for (pIter->start(); !pIter->done(); pIter->step()) {
            AcDbEntity* pEnt;
            es = pIter->getEntity(pEnt, AcDb::kForRead);
            if (es == Acad::eOk) {
                acutPrintf(_T("\nEntity type: %s"), pEnt->isA()->name());
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
                    pIter->start();  // Reset the iterator
                    delete pIter;
                    pModelSpace->close();
                    std::this_thread::sleep_for(std::chrono::seconds(1));  // Pause for a moment
                    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead);
                    if (es != Acad::eOk) {
                        acutPrintf(_T("\nFailed to reaccess model space after pause. Error status: %d\n"), es);
                        pBlockTable->close();
                        return corners;
                    }
                    break;
                }
            }
            else {
                acutPrintf(_T("\nFailed to get entity. Error status: %d\n"), es);
            }
        }

        if (entityCount % BATCH_SIZE != 0) {
            delete pIter;
            break;
        }
    }

    pModelSpace->close();
    pBlockTable->close();

    acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;
}

// ADD TEXT ANNOTATION TO DRAWING
void CornerAssetPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table. Error status: %d\n"), es);
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space. Error status: %d\n"), es);
        pBlockTable->close();
        return;
    }

    AcDbText* pText = new AcDbText(position, text, AcDbObjectId::kNull, 0.2, 0);
    es = pModelSpace->appendAcDbEntity(pText);
    if (es == Acad::eOk) {
        acutPrintf(_T("\nAdded text annotation: %s"), text);
    }
    else {
        acutPrintf(_T("\nFailed to add text annotation. Error status: %d\n"), es);
    }
    pText->close();  // Decrement reference count

    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}

// PLACE ASSETS AT DETECTED CORNERS EDIT TO USE GLOBAL VARIABLES FROM __ FUNCTION
void CornerAssetPlacer::placeAssetsAtCorners() {
    acutPrintf(_T("\nPlacing assets at corners..."));
    std::vector<AcGePoint3d> corners = detectPolylines();
    acutPrintf(_T("\nDetected %d corners from lines."), corners.size());

    AcDbObjectId cornerPostId = loadAsset(L"128286X");
    AcDbObjectId panelId = loadAsset(L"128285X");

    if (cornerPostId == AcDbObjectId::kNull || panelId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load assets."));
        return;
    }

    // Iterate through all detected corners and place assets accordingly
    for (size_t i = 0; i < corners.size(); ++i) {
        double rotation = 0.0;
        if (i < corners.size() - 1) {
            AcGeVector3d direction = corners[i + 1] - corners[i];
            rotation = atan2(direction.y, direction.x);
        }
        else {
            AcGeVector3d direction = corners[i-4] - corners[i]; //PROBLEM IS PROBABLY HERE
            rotation = atan2(direction.y, direction.x);
        }

        // Determine if the corner is inside or outside
        bool isInside = false;
        if (i < corners.size() - 1) {
            AcGeVector3d prevDirection = corners[i] - corners[i - 1];
            AcGeVector3d nextDirection = corners[i + 1] - corners[i];
            double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;
            isInside = crossProductZ < 0; // Change this logic based on your coordinate system
        }
        else {
            AcGeVector3d prevDirection = corners[i] - corners[i - 1];
            AcGeVector3d nextDirection = corners[0] - corners[i];
            double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;
            isInside = crossProductZ < 0; // Change this logic based on your coordinate system
        }

        acutPrintf(_T("\corners[i]: %d,"), corners[i]); // Debug
        acutPrintf(_T(" rotation: %f,"), rotation); // Debug
        acutPrintf(_T(" cornerPostId: %d,"), cornerPostId); // Debug
        acutPrintf(_T(" panelId: %d"), panelId); // Debug

        if (isInside) {
            placeInsideCornerPostAndPanels(corners[i], rotation, cornerPostId, panelId);
            addTextAnnotation(corners[i], L"Inside Corner");
        }
        else {
            placeOutsideCornerPostAndPanels(corners[i], rotation, cornerPostId, panelId);
            addTextAnnotation(corners[i], L"Outside Corner");
        }
    }

    acutPrintf(_T("\nCompleted placing assets."));
}

// LOAD ASSET FROM BLOCK TABLE
AcDbObjectId CornerAssetPlacer::loadAsset(const wchar_t* blockName) {
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

// PLACE INSIDE CORNER POST AND PANELS
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

    // Fetch this variable from DefineHeight
    int wallHeight = globalVarHeight;

    int currentHeight = 0;
    int panelHeights[] = { 135, 60 };


    // Iterate through 135 and 60 height
    for (int panelNum = 0; panelNum < 2; panelNum++) {

        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);  // Calculate the number of panels that fit vertically

        if (panelNum == 1) {
            cornerPostId = loadAsset(L"129864X");
            panelId = loadAsset(L"129842X");
        }

        for (int x = 0; x < numPanelsHeight; x++) {
            // Place the corner post at the detected corner
            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d cornerWithHeight = corner;
            cornerWithHeight.z += currentHeight;
            pCornerPostRef->setPosition(cornerWithHeight);
            pCornerPostRef->setBlockTableRecord(cornerPostId);
            pCornerPostRef->setRotation(rotation);
            pCornerPostRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

            if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
                acutPrintf(_T("\nCorner post placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place corner post."));
            }
            pCornerPostRef->close();  // Decrement reference count

            // Determine panel placement positions based on the rotation
            AcGeVector3d panelAOffset, panelBOffset;

            acutPrintf(_T("\nRotation angle: %f radians"), rotation);  // Debug rotation angle

            rotation = normalizeAngle(rotation);  // Normalize the rotation angle

            switch (static_cast<int>(rotation * 180 / M_PI)) {
            case 0:
                if (areAnglesEqual(rotation, 0, TOLERANCE)) {
					panelAOffset = AcGeVector3d(10.0, 0.0, 0.0);  // Panel A along the X-axis
					panelBOffset = AcGeVector3d(0.0, -25.0, 0.0);  // Panel B along the Y-axis
				}
                break;
            case 90:
                if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                    panelAOffset = AcGeVector3d(0.0, 10.0, 0.0);  // Panel A along the Y-axis
                    panelBOffset = AcGeVector3d(25.0, 0.0, 0.0);  // Panel B along the X-axis
                }
                break;
            case 180:
                if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
					panelAOffset = AcGeVector3d(-10.0, 0.0, 0.0);  // Panel A along the X-axis
					panelBOffset = AcGeVector3d(0.0, 25.0, 0.0);  // Panel B along the Y-axis
				}
                break;
            case 270:
                if (areAnglesEqual(rotation, 3 * M_PI_2, TOLERANCE)) {
					panelAOffset = AcGeVector3d(0.0, -10.0, 0.0);  // Panel A along the Y-axis
					panelBOffset = AcGeVector3d(-25.0, 0.0, 0.0);  // Panel B along the X-axis
				}
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle detected: %f "), rotation);
                continue;
            }

            AcGePoint3d panelPositionA = cornerWithHeight + panelAOffset;
            AcGePoint3d panelPositionB = cornerWithHeight + panelBOffset;

            // Place Panel A
            AcDbBlockReference* pPanelARef = new AcDbBlockReference();
            pPanelARef->setPosition(panelPositionA);
            pPanelARef->setBlockTableRecord(panelId);
            pPanelARef->setRotation(rotation);
            pPanelARef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

            if (pModelSpace->appendAcDbEntity(pPanelARef) == Acad::eOk) {
                acutPrintf(_T("\nPanel A placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel A."));
            }
            pPanelARef->close();  // Decrement reference count

            // Place Panel B
            AcDbBlockReference* pPanelBRef = new AcDbBlockReference();
            pPanelBRef->setPosition(panelPositionB);
            pPanelBRef->setBlockTableRecord(panelId);
            pPanelBRef->setRotation(rotation + M_PI_2);  // Panel B is perpendicular to the corner post
            pPanelBRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure no scaling

            if (pModelSpace->appendAcDbEntity(pPanelBRef) == Acad::eOk) {
                acutPrintf(_T("\nPanel B placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel B."));
            }
            pPanelBRef->close();  // Decrement reference count
            currentHeight += panelHeights[panelNum];
        }
    }

    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}

// PLACE OUTSIDE CORNER POST AND PANELS
void CornerAssetPlacer::placeOutsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelId) {
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

    // Fetch this variable from DefineHeight
    int wallHeight = globalVarHeight;

    int currentHeight = 0;
    int panelHeights[] = { 135, 60 };

    AcGePoint3d cornerWithHeight = corner;
    cornerWithHeight.z += currentHeight;

    acutPrintf(_T("\nDebug 1")); // Debug

    // Apply the correct offset based on rotation

    acutPrintf(_T("\nrotation: %f"), rotation); // Debug

    int rotationDegrees = static_cast<int>(rotation * 180 / M_PI);

    acutPrintf(_T("\nrotationDegrees: %d"), rotationDegrees); // Debug

    switch (rotationDegrees) {
    case 0:
        cornerWithHeight.x -= 10.0;
        cornerWithHeight.y -= 10.0;
        break;
    case 90:
        cornerWithHeight.x += 10.0;
        cornerWithHeight.y -= 10.0;
        break;
    case 180:
        cornerWithHeight.x += 10.0;
        cornerWithHeight.y += 10.0;
        break;
    case 270:
        cornerWithHeight.x -= 10.0;
        cornerWithHeight.y += 10.0;
        break;
    default:
        acutPrintf(_T("\nInvalid rotation angle detected."));
        return;
    }

    // Add 90 degrees to the rotation
    rotation += M_PI_2;
    if (rotation >= 2 * M_PI) {
        rotation -= 2 * M_PI;
    }

    // Iterate through 135 and 60 height
    for (int panelNum = 0; panelNum < 2; panelNum++) {
        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

        if (panelNum == 1) {
            cornerPostId = loadAsset(L"129864X");
            panelId = loadAsset(L"129842X");
        }

        for (int x = 0; x < numPanelsHeight; x++) {
            // Place the corner post at the detected corner
            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d currentCornerWithHeight = cornerWithHeight;
            currentCornerWithHeight.z += currentHeight;
            pCornerPostRef->setPosition(currentCornerWithHeight);
            pCornerPostRef->setBlockTableRecord(cornerPostId);
            pCornerPostRef->setRotation(rotation);
            pCornerPostRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
                acutPrintf(_T("\nCorner post placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place corner post."));
            }
            pCornerPostRef->close();

            // Determine panel placement positions based on the rotation
            AcGeVector3d panelAOffset, panelBOffset;

            rotation = normalizeAngle(rotation);  // Normalize the rotation angle

            acutPrintf(_T("\nDebug 2")); // Debug

            switch (static_cast<int>(rotation * 180 / M_PI)) {
            case 0:
                if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                    panelAOffset = AcGeVector3d(25.0, -10.0, 0.0);  // Panel A along the X-axis
                    panelBOffset = AcGeVector3d(10.0, -10.0, 0.0);  // Panel B along the Y-axis
                }
                break;
            case 90:
                if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
					panelAOffset = AcGeVector3d(10.0, 25.0, 0.0);  // Panel A along the Y-axis
					panelBOffset = AcGeVector3d(10.0, 10.0, 0.0);  // Panel B along the X-axis
				}
                break;
            case 180:
                if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
					panelAOffset = AcGeVector3d(-25.0, 10.0, 0.0);  // Panel A along the X-axis
					panelBOffset = AcGeVector3d(-10.0, 10.0, 0.0);  // Panel B along the Y-axis
				}
                break;
            case 270:
                if (areAnglesEqual(rotation, 3 * M_PI_2, TOLERANCE)) {
                    panelAOffset = AcGeVector3d(-10.0, -25.0, 0.0);  // Panel A along the Y-axis
                    panelBOffset = AcGeVector3d(-10.0, -10.0, 0.0);  // Panel B along the X-axis
                }
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle detected."));
                continue;
            }

            AcGePoint3d panelPositionA = currentCornerWithHeight + panelAOffset;
            AcGePoint3d panelPositionB = currentCornerWithHeight + panelBOffset;

            // Place Panel A
            AcDbBlockReference* pPanelARef = new AcDbBlockReference();
            pPanelARef->setPosition(panelPositionA);
            pPanelARef->setBlockTableRecord(panelId);
            pPanelARef->setRotation(rotation + M_PI);
            pPanelARef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanelARef) == Acad::eOk) {
                acutPrintf(_T("\nPanel A placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel A."));
            }
            pPanelARef->close();

            // Place Panel B
            AcDbBlockReference* pPanelBRef = new AcDbBlockReference();
            pPanelBRef->setPosition(panelPositionB);
            pPanelBRef->setBlockTableRecord(panelId);
            pPanelBRef->setRotation(rotation + M_PI_2 + M_PI);  // Panel B is perpendicular to the corner post
            pPanelBRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanelBRef) == Acad::eOk) {
                acutPrintf(_T("\nPanel B placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel B."));
            }
            pPanelBRef->close();
            currentHeight += panelHeights[panelNum];
        }
    }

    pModelSpace->close();
    pBlockTable->close();
}
