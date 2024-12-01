





#include "StdAfx.h"
#include "InsideCorner.h"
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
#include "aced.h"


std::map<AcGePoint3d, std::vector<AcGePoint3d>, InsideCorner::Point3dComparator> InsideCorner::wallMap;

const int BATCH_SIZE = 30; 

const double TOLERANCE = 0.19; 


struct PolylineSelectionResult {
    std::vector<AcGePoint3d> corners;
    double distance;
};


PolylineSelectionResult handleOutsidePolylineSelectionForInside() {
    
    ads_name selectedEntityA;
    ads_point ptA;
    ads_point firstPoint = { 0.0, 0.0, 0.0 };  
    ads_point SecondPoint = { 0.0, 0.0, 0.0 }; 
    std::vector<AcGePoint3d> cornersA;
    double distance = -1.0;  

    
    if (acedEntSel(L"\nSelect the first polyline: ", selectedEntityA, ptA) != RTNORM) {
        acutPrintf(L"\nError in selecting the first polyline.");
        return PolylineSelectionResult{ {}, distance };
    }

    
    if (acedGetPoint(NULL, L"\nSelect the first point (or press Enter to skip): ", firstPoint) != RTNORM) {
        acutPrintf(L"\nFirst point selection skipped.\n");
    }

    
    if (acedGetPoint(NULL, L"\nSelect the second point (or press Enter to skip): ", SecondPoint) != RTNORM) {
        acutPrintf(L"\nSecond point selection skipped.\n");
    }

    
    AcDbObjectId objIdA;
    acdbGetObjectId(objIdA, selectedEntityA);

    AcDbEntity* pEntityA = nullptr;
    if (acdbOpenAcDbEntity(pEntityA, objIdA, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(L"\nError in opening the first polyline.");
        return PolylineSelectionResult{ {}, distance };
    }

    
    if (pEntityA->isKindOf(AcDbPolyline::desc())) {
        AcDbPolyline* pPolylineA = AcDbPolyline::cast(pEntityA);

        if (pPolylineA) {
            processPolyline(pPolylineA, cornersA, 45.0, TOLERANCE);

            
            if (firstPoint != ads_point{ 0.0, 0.0, 0.0 } && SecondPoint != ads_point{ 0.0, 0.0, 0.0 }) {
                
                double deltaX = firstPoint[X] - SecondPoint[X];
                double deltaY = SecondPoint[Y] - firstPoint[Y];

                
                const double tolerance = 1.0;

                
                if (std::fabs(deltaX - deltaY) <= tolerance) {
                    
                    distance = snapToPredefinedValues(deltaX);
                }
                else {
                    distance = snapToPredefinedValues(std::sqrt(deltaX * deltaX + deltaY * deltaY));
                }
            }
            else {
                acutPrintf(L"\nPoints were not selected. Skipping distance calculation, Defaulting for distance 200.\n");
                distance = 200;
            }
        }

        
        
        
        
        
    }
    else {
        acutPrintf(L"\nThe selected entity is not a polyline.");
        return PolylineSelectionResult{ {}, distance };
    }

    
    pEntityA->close();

    
    return PolylineSelectionResult{ cornersA, distance };
}


void InsideCorner::placeInsideCornerPostAndPanels(
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
                
            }
            else {
                acutPrintf(_T("\nFailed to place Panel B."));
            }
            pPanelBRef->close();

            
            if (distance == 150) {
                AcGePoint3d compensatorPositionA = cornerWithHeight + compensatorOffsetA;
                AcGePoint3d compensatorPositionB = cornerWithHeight + compensatorOffsetB;

                AcDbBlockReference* pCompensatorARef = new AcDbBlockReference();
                pCompensatorARef->setPosition(compensatorPositionA);
                pCompensatorARef->setBlockTableRecord(compensatorIdA);
                pCompensatorARef->setRotation(rotation);
                pCompensatorARef->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorARef) == Acad::eOk) {
                    
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


void InsideCorner::placeOutsideCornerPostAndPanels(
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
        for (int x = 0; x < numPanelsHeight; x++) {

            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d cornerWithHeight = corner;
            cornerWithHeight.z += currentHeight;

            
            double offset = 100.0;  
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
                
            }
            else {
                acutPrintf(_T("\nFailed to place corner post at (%f, %f, %f)"), cornerWithHeight.x, cornerWithHeight.y, cornerWithHeight.z);
            }
            pCornerPostRef->close();

            
            AcDbObjectId outsidePanelIds[] = { outsidePanelIdA, outsidePanelIdB, outsidePanelIdC, outsidePanelIdD, outsidePanelIdE, outsidePanelIdF };

            
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
            double cornerWidth = 100.0;  

            if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                panelOffsets[0] = AcGeVector3d(cornerWidth + panelWidths[0], -(cornerWidth), 0.0);
                panelOffsets[1] = AcGeVector3d(cornerWidth, -(cornerWidth), 0.0);
                
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x + panelWidths[2]), panelOffsets[0].y, 0.0);
                panelOffsets[3] = AcGeVector3d(panelOffsets[1].x, ((panelOffsets[1].y) - panelWidths[1]), 0.0);
                
                panelOffsets[4] = AcGeVector3d(panelOffsets[2].x + panelWidths[4], panelOffsets[2].y, 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x, ((panelOffsets[3].y) - panelWidths[3]), 0.0);
                
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x + compensatorIdA, panelOffsets[4].y, 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x, ((panelOffsets[5].y - panelWidths[5])), 0.0);
            }

            else if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                
                panelOffsets[0] = AcGeVector3d(cornerWidth, cornerWidth + panelWidths[0], 0.0);
                panelOffsets[1] = AcGeVector3d(cornerWidth, cornerWidth, 0.0);
                
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x), (panelWidths[2] + panelOffsets[0].y), 0.0);
                panelOffsets[3] = AcGeVector3d((panelOffsets[1].x + panelWidths[1]), panelOffsets[1].y, 0.0);
                
                panelOffsets[4] = AcGeVector3d((panelOffsets[2].x), (panelWidths[4] + panelOffsets[2].y), 0.0);
                panelOffsets[5] = AcGeVector3d((panelOffsets[3].x + panelWidths[3]), panelOffsets[3].y, 0.0);
                
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x, (compensatorIdA + panelOffsets[4].y), 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x + panelWidths[5], panelOffsets[5].y, 0.0);
            }

            else if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
                
                panelOffsets[0] = AcGeVector3d(-(cornerWidth + panelWidths[0]), cornerWidth, 0.0);
                panelOffsets[1] = AcGeVector3d(-cornerWidth, cornerWidth, 0.0);
                
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x - panelWidths[2]), panelOffsets[0].y, 0.0);
                panelOffsets[3] = AcGeVector3d(panelOffsets[1].x, (panelOffsets[1].y + panelWidths[1]), 0.0);
                
                panelOffsets[4] = AcGeVector3d((panelOffsets[2].x - panelWidths[4]), panelOffsets[2].y, 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x, (panelOffsets[3].y + panelWidths[3]), 0.0);
                
                compensatorOffsetA = AcGeVector3d((panelOffsets[4].x - compensatorIdA), panelOffsets[4].y, 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x, (panelOffsets[5].y + panelWidths[5]), 0.0);
            }

            else if (areAnglesEqual(rotation, M_3PI_2, TOLERANCE)) {
                
                panelOffsets[0] = AcGeVector3d(-cornerWidth, -(cornerWidth + panelWidths[0]), 0.0);
                panelOffsets[1] = AcGeVector3d(-cornerWidth, -cornerWidth, 0.0);
                
                panelOffsets[2] = AcGeVector3d(panelOffsets[0].x, (panelOffsets[0].y - panelWidths[2]), 0.0);
                panelOffsets[3] = AcGeVector3d((panelOffsets[1].x - panelWidths[1]), panelOffsets[1].y, 0.0);
                
                panelOffsets[4] = AcGeVector3d(panelOffsets[2].x, (panelOffsets[2].y - panelWidths[4]), 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x - panelWidths[3], panelOffsets[3].y, 0.0);
                
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x, (panelOffsets[4].y - compensatorIdA), 0.0);
                compensatorOffsetB = AcGeVector3d((panelOffsets[5].x - panelWidths[5]), panelOffsets[5].y, 0.0);
            }
            else {
                acutPrintf(_T("\nInvalid rotation angle detected: %f at (%f, %f, %f)"), rotation, corner.x, corner.y, corner.z);
                continue;
            }

            for (int i = 0; i < 6; i++) {
                if (!outsidePanelIds[i].isNull()) {
                    AcGePoint3d panelPosition = cornerWithHeight + panelOffsets[i];
                    AcDbBlockReference* pPanelRef = new AcDbBlockReference();
                    pPanelRef->setPosition(panelPosition);
                    pPanelRef->setBlockTableRecord(outsidePanelIds[i]);

                    
                    double panelRotation = (i % 2 == 0) ? rotation : rotation + M_PI_2;
                    pPanelRef->setRotation(panelRotation + M_PI);
                    pPanelRef->setScaleFactors(AcGeScale3d(globalVarScale));

                    if (pModelSpace->appendAcDbEntity(pPanelRef) == Acad::eOk) {
                        
                    }
                    else {
                        acutPrintf(_T("\nFailed to place Panel %d at (%f, %f, %f)"), i, panelPosition.x, panelPosition.y, panelPosition.z);
                    }
                    pPanelRef->close();
                }
                else {
                    
                }
            }

            if (distance != 150) {
                
                AcGePoint3d compensatorPositionA = cornerWithHeight + compensatorOffsetA;
                AcGePoint3d compensatorPositionB = cornerWithHeight + compensatorOffsetB;

                AcDbBlockReference* pCompensatorRefA = new AcDbBlockReference();
                pCompensatorRefA->setPosition(compensatorPositionA);
                pCompensatorRefA->setBlockTableRecord(outsideCompensatorIdA);
                pCompensatorRefA->setRotation(rotation + M_PI);
                pCompensatorRefA->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorRefA) == Acad::eOk) {
                    
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
                    
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator B at (%f, %f, %f)"), compensatorPositionB.x, compensatorPositionB.y, compensatorPositionB.z);
                }
                pCompensatorRefB->close();

                
            }
            else {
                acutPrintf(_T("\nDistance is 150, skipping compensator placement."));
            }


            currentHeight += panelHeights[panelNum];
            
        }

        

        pModelSpace->close();
        pBlockTable->close();
    }
}


void InsideCorner::placeAssetsAtCorners() {
    
    PolylineSelectionResult result = handleOutsidePolylineSelectionForInside();

    if (result.corners.empty()) {
        acutPrintf(_T("No corners detected or an error occurred.\n"));
        return;
    }

    if (result.distance < 0) {
        acutPrintf(_T("Failed to calculate distance between polylines.\n"));
        return;
    }

    double Insidedistance = 200;
    ACHAR isDistance150[256];

    
    if (acedGetString(Adesk::kFalse, _T("\nInside Corner Distance is 200[Y/N] Default: Y "), isDistance150) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

    
    if (wcscmp(isDistance150, _T("N")) == 0 || wcscmp(isDistance150, _T("n")) == 0) {
        Insidedistance = 150;
    }
    else {
        Insidedistance = 200;
    }

    
    PanelDimensions panelDims;

    PanelConfig config = CornerAssetPlacer::getPanelConfig(result.distance, panelDims);


    if (!config.panelIdA || !config.panelIdB) {
        acutPrintf(_T("Panel configuration not found for Inside Corners.\n"));
        
    }

    
    AcDbObjectId cornerPostId = CornerAssetPlacer::loadAsset(L"128286X");
    if (cornerPostId == AcDbObjectId::kNull) {
        acutPrintf(_T("Failed to load corner post asset.\n"));
        return;
    }
    
    AcDbObjectId panelIdA = CornerAssetPlacer::loadAsset(config.panelIdA->blockName.c_str());
    AcDbObjectId panelIdB = CornerAssetPlacer::loadAsset(config.panelIdB->blockName.c_str());

    if (panelIdA == AcDbObjectId::kNull || panelIdB == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load panel assets."));
        return;
    }

    
    AcDbObjectId outsidePanelIds[6];
    for (int i = 0; i < 6; ++i) {
        if (config.outsidePanelIds[i] && config.outsidePanelIds[i]->width > 0) {  
            outsidePanelIds[i] = CornerAssetPlacer::loadAsset(config.outsidePanelIds[i]->blockName.c_str());
            if (outsidePanelIds[i] == AcDbObjectId::kNull) {
                acutPrintf(_T("\nFailed to load outside panel asset %d."), i);
            }
        }
        else {
            outsidePanelIds[i] = AcDbObjectId::kNull;  
        }
    }

    AcDbObjectId compensatorIdA = CornerAssetPlacer::loadAsset(config.compensatorIdA ? config.compensatorIdA->blockName.c_str() : L"");
    AcDbObjectId compensatorIdB = CornerAssetPlacer::loadAsset(config.compensatorIdB ? config.compensatorIdB->blockName.c_str() : L"");

    
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
        rotation = normalizeAngle(rotation);
        rotation = snapToExactAngle(rotation, TOLERANCE);

        adjustRotationForCorner(rotation, result.corners, cornerNum);
        

        
        AcGeVector3d prevDirection = result.corners[cornerNum] - result.corners[cornerNum > 0 ? cornerNum - 1 : result.corners.size() - 1];
        AcGeVector3d nextDirection = result.corners[(cornerNum + 1) % result.corners.size()] - result.corners[cornerNum];
        double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;

        if (isClockwise) {
            if (crossProductZ > 0) {
				placeOutsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, config, outsidePanelIds[0], outsidePanelIds[1], outsidePanelIds[2], outsidePanelIds[3], outsidePanelIds[4], outsidePanelIds[5], compensatorIdA, compensatorIdB, result.distance);
            }
            else {
                placeInsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, Insidedistance, compensatorIdA, compensatorIdB);
            }
        }
        else {
            if (isInside) {
                placeOutsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, config, outsidePanelIds[0], outsidePanelIds[1], outsidePanelIds[2], outsidePanelIds[3], outsidePanelIds[4], outsidePanelIds[5], compensatorIdA, compensatorIdB, result.distance);
                }
            else {
				placeInsideCornerPostAndPanels(result.corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, Insidedistance, compensatorIdA, compensatorIdB);
            }
        }
        
    }
    loopIndex = loopIndexLastPanel;
}