

#include "StdAfx.h"
#include "CornerAssetPlacer.h"
#include "SharedDefinations.h"
#include "GeometryUtils.h"
#include "SharedConfigs.h"
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

// Static member definition
std::map<AcGePoint3d, std::vector<AcGePoint3d>, CornerAssetPlacer::Point3dComparator> CornerAssetPlacer::wallMap;

const int BATCH_SIZE = 30; // Process 30 entities at a time

const double TOLERANCE = 0.19; // Tolerance for angle comparison

// PLACE ASSETS AT INSIDE CORNERS
void CornerAssetPlacer::placeInsideCornerPostAndPanels(
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

// Function to place the corner post and panels for an inside corner
void CornerAssetPlacer::placeAssetsAtCorners() {
    std::vector<AcGePoint3d> corners = detectPolylines();

    //print all corners numbers with it's coordinates
    for (size_t i = 0; i < corners.size(); ++i) {
        acutPrintf(_T("\nCorner %d: %f, %f"), i, corners[i].x, corners[i].y);
    }

    if (corners.empty()) {
        acutPrintf(_T("\nNo corners detected."));
        return;
    }

    double distance = calculateDistanceBetweenPolylines();
    if (distance < 0) {
        acutPrintf(_T("\nFailed to calculate distance between polylines."));
        return;
    }

    PanelDimensions panelDims;
    PanelConfig config = getPanelConfig(distance, panelDims);

    if (!config.panelIdA || !config.panelIdB) {
        acutPrintf(_T("\nFailed to load panels for distance %f"), distance);
        return;
    }

    std::vector<CornerConfig> cornerConfigs = generateCornerConfigs(corners, config);
    g_cornerConfigs = cornerConfigs;  // Store the corner configurations for later use

    // Debug output to verify corner configurations
    //for (size_t i = 0; i < cornerConfigs.size(); ++i) {
    //    acutPrintf(_T("\nCorner %d: Position: %f, %f, Is Inside: %d, Adjustment: %f"),
    //        i, cornerConfigs[i].position.x, cornerConfigs[i].position.y, cornerConfigs[i].isInside, cornerConfigs[i].outsideCornerAdjustment);
    //}


    // Load the corner post asset
    AcDbObjectId cornerPostId = loadAsset(L"128286X");
    if (cornerPostId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load corner post asset."));
        return;
    }

    // Convert Panel pointers to AcDbObjectId using loadAsset
    AcDbObjectId panelIdA = loadAsset(config.panelIdA->blockName.c_str());
    AcDbObjectId panelIdB = loadAsset(config.panelIdB->blockName.c_str());

    if (panelIdA == AcDbObjectId::kNull || panelIdB == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load panel assets."));
        return;
    }

    // Load outside panels and compensators
    AcDbObjectId outsidePanelIds[6];
    for (int i = 0; i < 6; ++i) {
        if (config.outsidePanelIds[i] && config.outsidePanelIds[i]->width > 0) {  // Skip dummy panels
            outsidePanelIds[i] = loadAsset(config.outsidePanelIds[i]->blockName.c_str());
            if (outsidePanelIds[i] == AcDbObjectId::kNull) {
                acutPrintf(_T("\nFailed to load outside panel asset %d."), i);
            }
        }
        else {
            outsidePanelIds[i] = AcDbObjectId::kNull;  // Assign null for dummy panels
        }
    }

    AcDbObjectId compensatorIdA = loadAsset(config.compensatorIdA ? config.compensatorIdA->blockName.c_str() : L"");
    AcDbObjectId compensatorIdB = loadAsset(config.compensatorIdB ? config.compensatorIdB->blockName.c_str() : L"");

    int firstLoopEnd = identifyFirstLoopEnd(corners);
    std::pair<std::vector<AcGePoint3d>, std::vector<AcGePoint3d>> loops = splitLoops(corners, firstLoopEnd);
    std::vector<AcGePoint3d>& firstLoop = loops.first;
    std::vector<AcGePoint3d>& secondLoop = loops.second;

    int loopIndex = 0;
    int loopIndexLastPanel = 0;
    int closeLoopCounter = -1;
    int outerLoopIndexValue = 0;

    bool isClockwise = isPolylineClockwise(corners);

    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        double rotation = 0.0;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[(cornerNum + 1) % corners.size()];
        AcGeVector3d direction = (end - start).normal();

        adjustRotationForCorner(rotation, corners, cornerNum);

        placeInsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, distance, compensatorIdA, compensatorIdB);

        loopIndex = loopIndexLastPanel;
    }
}