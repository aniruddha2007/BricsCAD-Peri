
#include "stdAfx.h"
#include "GeometryUtils.h"
#include "SharedDefinations.h"
#include "CornerAssetPlacer.h"
#include "OutsideCorner.h"
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <limits>
#include <string>
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
#include "aced.h"

// Static member definition
std::map<AcGePoint3d, std::vector<AcGePoint3d>, OutsideCorner::Point3dComparator> OutsideCorner::wallMap;

const int BATCH_SIZE = 30; // Process 30 entities at a time

const double TOLERANCE = 0.19; // Tolerance for angle comparison

// Function to handle user polyline selection, process corners, and return distance
struct PolylineSelectionResult {
    std::vector<AcGePoint3d> corners;
    double distance;
};

// Function to handle user polyline selection, process corners, and return distance
PolylineSelectionResult handleOutsidePolylineSelectionForOutside() {
    // Declare variables for storing the selection
    ads_name selectedEntityA;
    ads_point ptA;
    ads_point firstPoint = { 0.0, 0.0, 0.0 };  // Default to (0, 0, 0) if not selected
    ads_point SecondPoint = { 0.0, 0.0, 0.0 }; // Default to (0, 0, 0) if not selected
    std::vector<AcGePoint3d> cornersA;
    double distance = -1.0;  // Initialize distance with an invalid value

    // Prompt the user to select the first polyline
    if (acedEntSel(L"\nSelect the first polyline: ", selectedEntityA, ptA) != RTNORM) {
        acutPrintf(L"\nError in selecting the first polyline.");
        return PolylineSelectionResult{ {}, distance };
    }

    // Prompt the user to select the first point (mandatory selection)
    if (acedGetPoint(NULL, L"\nSelect the first point: ", firstPoint) != RTNORM) {
        acutPrintf(L"\nError in selecting the first point.\n");
    }

    // Prompt the user to select the second point (mandatory selection)
    if (acedGetPoint(NULL, L"\nSelect the second point: ", SecondPoint) != RTNORM) {
        acutPrintf(L"\nError in selecting the second point.\n");
    }

    // Open the selected entity for read
    AcDbObjectId objIdA;
    acdbGetObjectId(objIdA, selectedEntityA);

    AcDbEntity* pEntityA = nullptr;
    if (acdbOpenAcDbEntity(pEntityA, objIdA, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(L"\nError in opening the first polyline.");
        return PolylineSelectionResult{ {}, distance };
    }

    // Check if the selected entity is a polyline
    if (pEntityA->isKindOf(AcDbPolyline::desc())) {
        AcDbPolyline* pPolylineA = AcDbPolyline::cast(pEntityA);

        if (pPolylineA) {
            processPolyline(pPolylineA, cornersA, 45.0, TOLERANCE);

            // Calculate DeltaX and DeltaY
            double deltaX = firstPoint[X] - SecondPoint[X];
            double deltaY = SecondPoint[Y] - firstPoint[Y];

            // Define a small tolerance for comparison
            const double tolerance = 1.0;

            // Check if DeltaX and DeltaY are equal within the tolerance
            if (std::fabs(deltaX - deltaY) <= tolerance) {
                //acutPrintf(L"\nDeltaX and DeltaY are considered equal within the tolerance.\n");
                distance = snapToPredefinedValues(deltaX);
            }
            else {
                distance = snapToPredefinedValues(std::sqrt(deltaX * deltaX + deltaY * deltaY));
                //acutPrintf(L"\nDistance between points A and B: %lf\n", distance);
            }
        }

        // Output detected corners for debugging or visualization
        //acutPrintf(_T("Detected corners:\n"));
        //for (const auto& corner : cornersA) {
        //    //acutPrintf(_T("Corner at: %.2f, %.2f\n"), corner.x, corner.y);
        //}

    }
    else {
        acutPrintf(L"\nThe selected entity is not a polyline.");
    }

    // Close the entity
    pEntityA->close();


    // Return the corners and the distance
    return PolylineSelectionResult{ cornersA, distance };
}

// PLACE ASSETS AT INSIDE CORNERS
void OutsideCorner::placeInsideCornerPostAndPanels(
    const AcGePoint3d& corner,
    double rotation,
    AcDbObjectId cornerPostId,
    AcDbObjectId panelIdA,
    AcDbObjectId panelIdB,
    double distance,
    AcDbObjectId compensatorIdA,
    AcDbObjectId compensatorIdB) {
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
    int panelHeights[] = { 1350, 600 };

    for (int panelNum = 0; panelNum < 2; panelNum++) {
        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

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

            AcGeVector3d panelAOffset, panelBOffset, compensatorOffsetA, compensatorOffsetB;
            rotation = normalizeAngle(rotation);
            rotation = snapToExactAngle(rotation, TOLERANCE);

            if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                panelAOffset = AcGeVector3d(100.0, 0.0, 0.0);
                panelBOffset = AcGeVector3d(0.0, -250.0, 0.0);
                compensatorOffsetA = AcGeVector3d(250.0, 0.0, 0.0);
                compensatorOffsetB = AcGeVector3d(0.0, -300.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(0.0, 100.0, 0.0);
                panelBOffset = AcGeVector3d(250.0, 0.0, 0.0);
                compensatorOffsetA = AcGeVector3d(0.0, 250.0, 0.0);
                compensatorOffsetB = AcGeVector3d(300.0, 0.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
                panelAOffset = AcGeVector3d(-100.0, 0.0, 0.0);
                panelBOffset = AcGeVector3d(0.0, 250.0, 0.0);
                compensatorOffsetA = AcGeVector3d(-250.0, 0.0, 0.0);
                compensatorOffsetB = AcGeVector3d(0.0, 300.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_3PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(0.0, -100.0, 0.0);
                panelBOffset = AcGeVector3d(-250.0, 0.0, 0.0);
                compensatorOffsetA = AcGeVector3d(0.0, -250.0, 0.0);
                compensatorOffsetB = AcGeVector3d(-300.0, 0.0, 0.0);
            }
            else {
                acutPrintf(_T("\nInvalid rotation angle detected: %f at %f"), rotation, corner);
                continue;
            }

            AcGePoint3d panelPositionA = cornerWithHeight + panelAOffset;
            AcGePoint3d panelPositionB = cornerWithHeight + panelBOffset;

            AcDbBlockReference* pPanelARef = new AcDbBlockReference();
            pPanelARef->setPosition(panelPositionA);
            pPanelARef->setBlockTableRecord(panelIdA);
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
            pPanelBRef->setBlockTableRecord(panelIdB);
            pPanelBRef->setRotation(rotation + M_PI_2);
            pPanelBRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanelBRef) == Acad::eOk) {
                //acutPrintf(_T("\nPanel B placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel B."));
            }
            pPanelBRef->close();

            // Place compensators only if distance is 150
            if (distance == 150) {
                AcGePoint3d compensatorPositionA = cornerWithHeight + compensatorOffsetA;
                AcGePoint3d compensatorPositionB = cornerWithHeight + compensatorOffsetB;

                AcDbBlockReference* pCompensatorARef = new AcDbBlockReference();
                pCompensatorARef->setPosition(compensatorPositionA);
                pCompensatorARef->setBlockTableRecord(compensatorIdA);
                pCompensatorARef->setRotation(rotation);
                pCompensatorARef->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorARef) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator A placed successfully."));
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator A."));
                }
                pCompensatorARef->close();

                AcDbBlockReference* pCompensatorBRef = new AcDbBlockReference();
                pCompensatorBRef->setPosition(compensatorPositionB);
                pCompensatorBRef->setBlockTableRecord(compensatorIdB);
                pCompensatorBRef->setRotation(rotation + M_PI_2);
                pCompensatorBRef->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorBRef) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator B placed successfully."));
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator B."));
                }
                pCompensatorBRef->close();
            }

            currentHeight += panelHeights[panelNum];
        }
    }

    pModelSpace->close();
    pBlockTable->close();
}

// PLACE ASSETS AT OUTSIDE CORNERS
void OutsideCorner::placeOutsideCornerPostAndPanels(
    const AcGePoint3d& corner,
    double rotation,
    AcDbObjectId cornerPostId,
    const PanelConfig& config,
    AcDbObjectId outsidePanelIdA,
    AcDbObjectId outsidePanelIdB,
    AcDbObjectId outsidePanelIdC,
    AcDbObjectId outsidePanelIdD,
    AcDbObjectId outsidePanelIdE,
    AcDbObjectId outsidePanelIdF,
    AcDbObjectId outsideCompensatorIdA,
    AcDbObjectId outsideCompensatorIdB,
    double distance)
{
    //acutPrintf(_T("\nStarting placeOutsideCornerPostAndPanels function."));

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
    int panelHeights[] = { 1350, 600 };

    rotation = normalizeAngle(rotation);
    rotation = snapToExactAngle(rotation, TOLERANCE);

    for (int panelNum = 0; panelNum < 2; panelNum++) {
        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);
        //acutPrintf(_T("\nPlacing panels of height %d. Number of panels: %d"), panelHeights[panelNum], numPanelsHeight);

        for (int x = 0; x < numPanelsHeight; x++) {
            //acutPrintf(_T("\nPlacing corner post at height %d"), currentHeight);

            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d cornerWithHeight = corner;
            cornerWithHeight.z += currentHeight;

            // Offsets for corner post based on rotation
            double offset = 100.0;  // This is the corner width
            int rotationDegrees = static_cast<int>(rotation * 180 / M_PI);
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
            case 270:
                cornerWithHeight.x += offset;
                cornerWithHeight.y += offset;
                break;
            case 0:
                cornerWithHeight.x -= offset;
                cornerWithHeight.y += offset;
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle detected: %f at (%f, %f, %f)"), rotation, corner.x, corner.y, corner.z);
                continue;
            }

            pCornerPostRef->setPosition(cornerWithHeight);
            pCornerPostRef->setBlockTableRecord(cornerPostId);
            pCornerPostRef->setRotation(rotation);
            pCornerPostRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
                //acutPrintf(_T("\nCorner post placed successfully at (%f, %f, %f)"), cornerWithHeight.x, cornerWithHeight.y, cornerWithHeight.z);
            }
            else {
                acutPrintf(_T("\nFailed to place corner post at (%f, %f, %f)"), cornerWithHeight.x, cornerWithHeight.y, cornerWithHeight.z);
            }
            pCornerPostRef->close();

            // Correctly declare and initialize outsidePanelIds array
            AcDbObjectId outsidePanelIds[] = { outsidePanelIdA, outsidePanelIdB, outsidePanelIdC, outsidePanelIdD, outsidePanelIdE, outsidePanelIdF };

            // Check if there are any valid panels to place
            bool hasValidPanels = false;
            for (int i = 0; i < 6; ++i) {
                if (!outsidePanelIds[i].isNull()) {
                    hasValidPanels = true;
                    break;
                }
            }

            if (!hasValidPanels) {
                acutPrintf(_T("\nNo valid outside panel IDs found. Skipping placement for this corner."));
                continue;
            }

            AcGeVector3d panelOffsets[6], compensatorOffsetA, compensatorOffsetB;
            double panelWidths[] = {
                config.outsidePanelIds[0] ? config.outsidePanelIds[0]->width : 0,
                config.outsidePanelIds[1] ? config.outsidePanelIds[1]->width : 0,
                config.outsidePanelIds[2] ? config.outsidePanelIds[2]->width : 0,
                config.outsidePanelIds[3] ? config.outsidePanelIds[3]->width : 0,
                config.outsidePanelIds[4] ? config.outsidePanelIds[4]->width : 0,
                config.outsidePanelIds[5] ? config.outsidePanelIds[5]->width : 0
            };
            double compensatorIdA = config.compensatorIdA ? config.compensatorIdA->width : 0;
            double compensatorIdB = config.compensatorIdB ? config.compensatorIdB->width : 0;
            double cornerWidth = 100.0;  // Width of the corner post

            //acutPrintf(_T("\nCalculated panel widths: %f, %f, %f, %f, %f, %f"),
                //panelWidths[0], panelWidths[1], panelWidths[2], panelWidths[3], panelWidths[4], panelWidths[5]);

            // Define panel offsets based on rotation and number of panels
            if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                panelOffsets[0] = AcGeVector3d(cornerWidth + panelWidths[0], -(cornerWidth), 0.0);
                panelOffsets[1] = AcGeVector3d(cornerWidth, -(cornerWidth), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x + panelWidths[2]), panelOffsets[0].y, 0.0);
                panelOffsets[3] = AcGeVector3d(panelOffsets[1].x, ((panelOffsets[1].y) - panelWidths[1]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d(panelOffsets[2].x + panelWidths[4], panelOffsets[2].y, 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x, ((panelOffsets[3].y) - panelWidths[3]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x + compensatorIdA, panelOffsets[4].y, 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x, ((panelOffsets[5].y - panelWidths[5])), 0.0);
            }

            else if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                // Adjust for 90-degree rotation
                panelOffsets[0] = AcGeVector3d(cornerWidth, cornerWidth + panelWidths[0], 0.0);
                panelOffsets[1] = AcGeVector3d(cornerWidth, cornerWidth, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x), (panelWidths[2] + panelOffsets[0].y), 0.0);
                panelOffsets[3] = AcGeVector3d((panelOffsets[1].x + panelWidths[1]), panelOffsets[1].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d((panelOffsets[2].x), (panelWidths[4] + panelOffsets[2].y), 0.0);
                panelOffsets[5] = AcGeVector3d((panelOffsets[3].x + panelWidths[3]), panelOffsets[3].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x, (compensatorIdA + panelOffsets[4].y), 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x + panelWidths[5], panelOffsets[5].y, 0.0);
            }

            else if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
                // Adjust for 180-degree rotation
                panelOffsets[0] = AcGeVector3d(-(cornerWidth + panelWidths[0]), cornerWidth, 0.0);
                panelOffsets[1] = AcGeVector3d(-cornerWidth, cornerWidth, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x - panelWidths[2]), panelOffsets[0].y, 0.0);
                panelOffsets[3] = AcGeVector3d(panelOffsets[1].x, (panelOffsets[1].y + panelWidths[1]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d((panelOffsets[2].x - panelWidths[4]), panelOffsets[2].y, 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x, (panelOffsets[3].y + panelWidths[3]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d((panelOffsets[4].x - compensatorIdA), panelOffsets[4].y, 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x, (panelOffsets[5].y + panelWidths[5]), 0.0);
            }

            else if (areAnglesEqual(rotation, M_3PI_2, TOLERANCE)) {
                // Adjust for 270-degree rotation
                panelOffsets[0] = AcGeVector3d(-cornerWidth, -(cornerWidth + panelWidths[0]), 0.0);
                panelOffsets[1] = AcGeVector3d(-cornerWidth, -cornerWidth, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d(panelOffsets[0].x, (panelOffsets[0].y - panelWidths[2]), 0.0);
                panelOffsets[3] = AcGeVector3d((panelOffsets[1].x - panelWidths[1]), panelOffsets[1].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d(panelOffsets[2].x, (panelOffsets[2].y - panelWidths[4]), 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x - panelWidths[3], panelOffsets[3].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x, (panelOffsets[4].y - compensatorIdA), 0.0);
                compensatorOffsetB = AcGeVector3d((panelOffsets[5].x - panelWidths[5]), panelOffsets[5].y, 0.0);
            }
            else {
                acutPrintf(_T("\nInvalid rotation angle detected: %f at (%f, %f, %f)"), rotation, corner.x, corner.y, corner.z);
                continue;
            }

            //acutPrintf(_T("\nCalculated panel offsets:"));
            for (int i = 0; i < 6; ++i) {
                //acutPrintf(_T("\nPanel %d offset: (%f, %f, %f)"), i, panelOffsets[i].x, panelOffsets[i].y, panelOffsets[i].z);
            }

            // Place panels using the calculated offsets
            //AcDbObjectId outsidePanelIds[] = { outsidePanelIdA, outsidePanelIdB, outsidePanelIdC, outsidePanelIdD, outsidePanelIdE, outsidePanelIdF };
            for (int i = 0; i < 6; i++) {
                if (!outsidePanelIds[i].isNull()) {
                    AcGePoint3d panelPosition = cornerWithHeight + panelOffsets[i];
                    AcDbBlockReference* pPanelRef = new AcDbBlockReference();
                    pPanelRef->setPosition(panelPosition);
                    pPanelRef->setBlockTableRecord(outsidePanelIds[i]);

                    // Add rotation to outside panels
                    double panelRotation = (i % 2 == 0) ? rotation : rotation + M_PI_2;
                    pPanelRef->setRotation(panelRotation + M_PI);
                    pPanelRef->setScaleFactors(AcGeScale3d(globalVarScale));

                    if (pModelSpace->appendAcDbEntity(pPanelRef) == Acad::eOk) {
                        //acutPrintf(_T("\nPanel %d placed successfully at (%f, %f, %f)"), i, panelPosition.x, panelPosition.y, panelPosition.z);
                    }
                    else {
                        acutPrintf(_T("\nFailed to place Panel %d at (%f, %f, %f)"), i, panelPosition.x, panelPosition.y, panelPosition.z);
                    }
                    pPanelRef->close();
                }
                else {
                    //acutPrintf(_T("\nOutside Panel ID %d is a dummy or null panel, skipping."), i);
                }
            }

            if (distance != 150) {
                // Place compensators at the defined positions
                AcGePoint3d compensatorPositionA = cornerWithHeight + compensatorOffsetA;
                AcGePoint3d compensatorPositionB = cornerWithHeight + compensatorOffsetB;

                AcDbBlockReference* pCompensatorRefA = new AcDbBlockReference();
                pCompensatorRefA->setPosition(compensatorPositionA);
                pCompensatorRefA->setBlockTableRecord(outsideCompensatorIdA);
                pCompensatorRefA->setRotation(rotation + M_PI);
                pCompensatorRefA->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorRefA) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator A placed successfully at (%f, %f, %f)"), compensatorPositionA.x, compensatorPositionA.y, compensatorPositionA.z);
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator A at (%f, %f, %f)"), compensatorPositionA.x, compensatorPositionA.y, compensatorPositionA.z);
                }
                pCompensatorRefA->close();

                AcDbBlockReference* pCompensatorRefB = new AcDbBlockReference();
                pCompensatorRefB->setPosition(compensatorPositionB);
                pCompensatorRefB->setBlockTableRecord(outsideCompensatorIdB);
                pCompensatorRefB->setRotation(rotation + M_PI_2 + M_PI);
                pCompensatorRefB->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorRefB) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator B placed successfully at (%f, %f, %f)"), compensatorPositionB.x, compensatorPositionB.y, compensatorPositionB.z);
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator B at (%f, %f, %f)"), compensatorPositionB.x, compensatorPositionB.y, compensatorPositionB.z);
                }
                pCompensatorRefB->close();

                //acutPrintf(_T("\nFinished placing outside corner post and compensators."));
            }
            else {
                acutPrintf(_T("\nDistance is 150, skipping compensator placement."));
            }


            currentHeight += panelHeights[panelNum];
            //acutPrintf(_T("\nCompleted placement for height %d. Moving to the next height."), currentHeight);
        }

        //acutPrintf(_T("\nFinished placing outside corner post, panels, and compensators."));

        pModelSpace->close();
        pBlockTable->close();
    }
}

//Function to place the corner assets
void OutsideCorner::placeAssetsAtCorners() {
	//Get Corners and distance from handleOutsidePolylineSelection

    PolylineSelectionResult result = handleOutsidePolylineSelectionForOutside();

    if (result.corners.empty()) {
        acutPrintf(_T("No corners detected or an error occurred.\n"));
        return;
    }

	if (result.distance < 0) {
		acutPrintf(_T("Failed to calculate distance between polylines.\n"));
		return;
	}

    //get Panel Configuration
    PanelDimensions panelDims;
    PanelConfig config = CornerAssetPlacer::getPanelConfig(result.distance, panelDims);

    if (!config.panelIdA || !config.panelIdB) {
        acutPrintf(_T("Panel configuration not found for Outside Corners.\n"));
        //return;
    }

    //Load Panels, corner post and compensators
    AcDbObjectId cornerPostId = CornerAssetPlacer::loadAsset(L"128286X");
    if (cornerPostId == AcDbObjectId::kNull) {
        acutPrintf(_T("Failed to load corner post asset.\n"));
        return;
    }
    // Convert Panel pointers to AcDbObjectId using loadAsset
    AcDbObjectId panelIdA = CornerAssetPlacer::loadAsset(config.panelIdA->blockName.c_str());
    AcDbObjectId panelIdB = CornerAssetPlacer::loadAsset(config.panelIdB->blockName.c_str());

    if (panelIdA == AcDbObjectId::kNull || panelIdB == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load panel assets."));
        return;
    }

    // Load outside panels and compensators
    AcDbObjectId outsidePanelIds[6];
    for (int i = 0; i < 6; ++i) {
        if (config.outsidePanelIds[i] && config.outsidePanelIds[i]->width > 0) {  // Skip dummy panels
            outsidePanelIds[i] = CornerAssetPlacer::loadAsset(config.outsidePanelIds[i]->blockName.c_str());
            if (outsidePanelIds[i] == AcDbObjectId::kNull) {
                acutPrintf(_T("\nFailed to load outside panel asset %d."), i);
            }
        }
        else {
            outsidePanelIds[i] = AcDbObjectId::kNull;  // Assign null for dummy panels
        }
    }

    AcDbObjectId compensatorIdA = CornerAssetPlacer::loadAsset(config.compensatorIdA ? config.compensatorIdA->blockName.c_str() : L"");
    AcDbObjectId compensatorIdB = CornerAssetPlacer::loadAsset(config.compensatorIdB ? config.compensatorIdB->blockName.c_str() : L"");

    //do concave/ convex check
    int loopIndex = 0;
    int loopIndexLastPanel = 0;
    int closeLoopCounter = -1;
    int outerLoopIndexValue = 0;

    bool isClockwise = isPolylineClockwise(result.corners);

    for (size_t cornerNum = 0; cornerNum < result.corners.size(); ++cornerNum) {
        double rotation = 0.0;
        AcGePoint3d start = result.corners[cornerNum];
        AcGePoint3d end = result.corners[(cornerNum + 1) % result.corners.size()];
        AcGeVector3d direction = (end - start).normal();

        closeLoopCounter++;
        bool isInside = false;

        if (!isItInteger(direction.x) || !isItInteger(direction.y)) {
            if (cornerNum < result.corners.size() - 1) {
                start = result.corners[cornerNum];
                end = result.corners[cornerNum - closeLoopCounter];
                closeLoopCounter = -1;
                loopIndexLastPanel = 1;
            }
        }

        direction = (end - start).normal();
        rotation = atan2(direction.y, direction.x);
        //acutPrintf(_T("\nCorner %d: %f, %f"), cornerNum, corners[cornerNum].x, corners[cornerNum].y);
        rotation = normalizeAngle(rotation);
        //acutPrintf(_T("\nRotation: %f"), rotation);
        rotation = snapToExactAngle(rotation, TOLERANCE);
        //acutPrintf(_T("\nRotation: %f"), rotation);

        adjustRotationForCorner(rotation, result.corners, cornerNum);
        //place assets using placeInsideCornerPostAndPanels

        // **Special Case**: Determine if the corner is convex or concave
        AcGeVector3d prevDirection = result.corners[cornerNum] - result.corners[cornerNum > 0 ? cornerNum - 1 : result.corners.size() - 1];
        AcGeVector3d nextDirection = result.corners[(cornerNum + 1) % result.corners.size()] - result.corners[cornerNum];
        double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;

        if (crossProductZ < 0) {
            // Convex corner
            //acutPrintf(_T("\nConvex corner detected at %f, %f"), corners[cornerNum].x, corners[cornerNum].y);
            // Add logic specific to convex corners here if needed
            if (!isInside) {
                placeOutsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, config, outsidePanelIds[0], outsidePanelIds[1], outsidePanelIds[2], outsidePanelIds[3], outsidePanelIds[4], outsidePanelIds[5], compensatorIdA, compensatorIdB, result.distance);
            }
            else {
                placeInsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, result.distance, compensatorIdA, compensatorIdB);
            }
        }
        else {
            // Concave corner
            //acutPrintf(_T("\nConcave corner detected at %f, %f"), corners[cornerNum].x, corners[cornerNum].y);
            // Add logic specific to concave corners here if needed
            if (!isInside) {
                placeInsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, result.distance, compensatorIdA, compensatorIdB);
            }
            else {
                placeOutsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, config, outsidePanelIds[0], outsidePanelIds[1], outsidePanelIds[2], outsidePanelIds[3], outsidePanelIds[4], outsidePanelIds[5], compensatorIdA, compensatorIdB, result.distance);

            }
        }

    }
    loopIndex = loopIndexLastPanel;
}
