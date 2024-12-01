#include "stdAfx.h"
#include "TiePlacer.h"
#include "SharedDefinations.h"  
#include "DefineScale.h"        
#include <vector>               
#include <algorithm>            
#include <tuple>                
#include "dbapserv.h"           
#include "dbents.h"             
#include "dbsymtb.h"            
#include "AssetPlacer/GeometryUtils.h" 
#include <array>
#include <cmath>
#include <map>
#include <thread>
#include <chrono>
#include "DefineHeight.h"
#include <string>


std::map<AcGePoint3d, std::vector<AcGePoint3d>, TiePlacer::Point3dComparator> TiePlacer::wallMap;

bool isThisInteger(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

const double TOLERANCE = 0.1; 

const int BATCH_SIZE = 1000; 

double distanceBetweenPoly;


struct Panel {
    int length;
    std::wstring id[2];
};


struct Tie {
    int length;
    std::wstring id;
};

bool isCornerConcaveTie(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
    
    AcGeVector3d v1 = current - prev;
    AcGeVector3d v2 = next - current;
    double cross = v1.x * v2.y - v1.y * v2.x;

    double tolerance = 1e-6;

    bool isConcave = cross < -tolerance;

    
    
    
    

    return isConcave;
}

bool isCornerConvexTie(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
    AcGeVector3d v1 = current - prev;
    AcGeVector3d v2 = next - current;
    double cross = v1.x * v2.y - v1.y * v2.x;

    
    double tolerance = 1e-6;

    bool isConvex = cross > tolerance;

    
    
    

    return isConvex;
}


std::vector<AcGePoint3d> TiePlacer::detectPolylines() {
    
    std::vector<AcGePoint3d> corners;
    wallMap.clear();  

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
                    processPolyline(pPolyline, corners, 90.0, TOLERANCE);  
                }
            }
            pEnt->close();
            entityCount++;

            if (entityCount % BATCH_SIZE == 0) {
                acutPrintf(_T("\nProcessed %d entities. Pausing to avoid resource exhaustion.\n"), entityCount);
                std::this_thread::sleep_for(std::chrono::seconds(1));  
            }
        }
        else {
            acutPrintf(_T("\nFailed to get entity. Error status: %d\n"), es);
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    
    return corners;

}


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
                if (!pFirstPolyline) {
                    pFirstPolyline = AcDbPolyline::cast(pEnt);
                }
                else {
                    pSecondPolyline = AcDbPolyline::cast(pEnt);
                    pEnt->close();
                    break; 
                }
            }
            pEnt->close();
        }
    }

    
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

    
    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    return positions;
}


std::vector<std::tuple<AcGePoint3d, double>> calculateTiePositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    return {};
}


AcDbObjectId TiePlacer::LoadTieAsset(const wchar_t* blockName) {
    
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
        
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    
    return blockId;
}


void TiePlacer::placeTieAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId) {
    
}

double crossProduct3(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool directionOfDrawing3(std::vector<AcGePoint3d>& points) {
    
    if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
        points.push_back(points.front());
    }

    double totalTurns = 0.0;

    for (size_t i = 1; i < points.size() - 1; ++i) {
        totalTurns += crossProduct3(points[i - 1], points[i], points[i + 1]);
    }

    
    if (totalTurns < 0) {
        return true;  
    }
    
    else if (totalTurns > 0) {
        return false; 
    }
    
    else {
        acutPrintf(_T("Warning: The shape does not have a defined direction. Defaulting to clockwise.\n"));
        return true;  
    }
}

bool isThisClockwise(const AcGePoint3d& p0, const AcGePoint3d& p1, const AcGePoint3d& p2) {
    
    AcGeVector3d v1 = p1 - p0;  
    AcGeVector3d v2 = p2 - p1;  

    
    AcGeVector3d crossProduct = v1.crossProduct(v2);

    
    
    
    return crossProduct.z < 0;
}


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
                    break; 
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


void TiePlacer::placeTies() {
    
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    
    if (panelPositions.empty()) {
        
    }

    double distanceBetweenPolylines = calculateDistanceBetweenPolylines();

    
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

    

    for (const auto& tie : tieSizes) {
        if (tie.length >= ((int)distanceBetweenPoly + 300)) {
            
            tieAssetId = LoadTieAsset(tie.id.c_str());  
            break;
        }
    }

    for (const auto& tie : tieSizes) {
        if (tie.length >= ((int)distanceBetweenPoly + 300 + 90)) {
           
            tieAssetWalerId = LoadTieAsset(tie.id.c_str());  
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

    
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[(cornerNum + 1) % corners.size()];  
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

    std::vector<AcGePoint3d> firstLoop(corners.begin(), corners.begin() + firstLoopEnd + 1);
    std::vector<AcGePoint3d> secondLoop(corners.begin() + firstLoopEnd + 1, corners.end());

    bool firstLoopIsClockwise = directionOfDrawing3(firstLoop);
    bool secondLoopIsClockwise = directionOfDrawing3(secondLoop);

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

    
    for (int cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        cornerLocations.push_back(static_cast<int>(totalPanelsPlaced));
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        if (cornerNum == 0) {
            first_start = start;
            
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

        
        AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
        AcGePoint3d current = corners[cornerNum];
        AcGePoint3d next;
        AcGePoint3d nextNext;
        if (cornerNum + 1 < corners.size()) {
            
            next = corners[(cornerNum + 1) % corners.size()];
        }
        else {
            
            next = corners[cornerNum + 1 - closeLoopCounter];
        }
        if (cornerNum + 2 < corners.size()) {
            nextNext = corners[(cornerNum + 2) % corners.size()]; 
        }
        else {
            nextNext = corners[cornerNum + 2 - (corners.size() / 2)];
        }

        bool isConcave = isCornerConcaveTie(prev, current, next);
        bool isConvex = !isConcave && isCornerConvexTie(prev, current, next);

        
        
        
        bool isAdjacentConcave = isCornerConcaveTie(current, next, nextNext);
        bool isAdjacentConvex = !isAdjacentConcave && isCornerConvexTie(current, next, nextNext);

        if (isConvex) {
            
            
            size_t prevIndex = (cornerNum + corners.size() - 1) % corners.size();
            size_t nextIndex = (cornerNum + 1) % corners.size();

            
            
            
        }
        else if (isConcave) {
            
            
            
        }

        bool prevClockwise = isThisClockwise(prev, start, end);
        bool nextClockwise = isThisClockwise(start, end, next);

        bool isInner = loopIndex != outerLoopIndexValue;
        bool isOuter = !isInner;  

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
                    adjustment = 150; 
                }

                adjustment -= 350;

                
                
                
                

                start += direction * adjustment;

            }


            if (!isAdjacentConvex) {
                
                if (distanceBetweenPolylines == 150) {
                    end -= direction * 50;
                }
                else {
                    
                    end -= direction * 0;
                }
            }
            else {
                

                
                
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
                    adjustment = 150; 
                }

                adjustment -= 300;
                end -= direction * adjustment;
            }

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

            
            int panelPosition = panelNum;
            WallPanel detectedPanel = wallPanels[panelPosition];
            AcGePoint3d detectedPanelPosition = detectedPanel.position;
            AcDbObjectId detectedPanelId = detectedPanel.assetId;

            double panelLength = wallPanels[panelPosition].length;

            int startCornerIndex = -1;
            int endCornerIndex = -1;

            for (int j = 0; j < cornerLocations.size(); ++j) {
                if (cornerLocations[j] < panelNum) {
                    startCornerIndex = cornerLocations[j];  
                }
                if (cornerLocations[j] > panelNum) {
                    endCornerIndex = cornerLocations[j];  
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

            
            int centerIndex = (startCornerIndex + endCornerIndex) / 2;

            
            AcGePoint3d centerPanelPosition = wallPanels[centerIndex + movedCompensators].position;

            AcGeVector3d direction = (wallPanels[panelNum].position - wallPanels[centerIndex].position).normal();

            
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
        xOffset = distanceBetweenPoly / 2; 
    }
    else {
        xOffset = distanceBetweenPoly / 2; 
    }
    double yOffset = 25; 
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
                
                currentPointWithHeight = panel.position;
                currentPointWithHeight.z += tieOffsetHeight[tiePlaced];
                
                switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                case 0: 
                    currentPointWithHeight.x += yOffset;
                    currentPointWithHeight.y += xOffset;
                    break;
                case 1: 
                    currentPointWithHeight.x -= xOffset;
                    currentPointWithHeight.y += yOffset;
                    break;
                case 2: 
                    currentPointWithHeight.x -= yOffset;
                    currentPointWithHeight.y -= xOffset;
                    break;
                case 3: 
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
                        case 0: 
                            wingnutPosition.y += (wingtieOffset + walerOffset);
                            wingnutRotation += M_PI;
                            break;
                        case 1: 
                            wingnutPosition.x += (wingtieOffset + walerOffset);
                            break;
                        case 2: 
                            wingnutPosition.y -= (wingtieOffset + walerOffset);
                            wingnutRotation += M_PI;
                            break;
                        case 3: 
                            wingnutPosition.x -= (wingtieOffset + walerOffset);
                            break;
                        case -1:
                            break;
                        }
                    }
                    else {
                        switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                        case 0: 
                            wingnutPosition.y += wingtieOffset;
                            wingnutRotation += M_PI;
                            break;
                        case 1: 
                            wingnutPosition.x += wingtieOffset;
                            break;
                        case 2: 
                            wingnutPosition.y -= wingtieOffset;
                            wingnutRotation += M_PI;
                            break;
                        case 3: 
                            wingnutPosition.x -= wingtieOffset;
                            break;
                        case -1:
                            break;
                        }
                    }
                    

                    pWingnutRef->setPosition(wingnutPosition);
                    pWingnutRef->setBlockTableRecord(assetIdWingnut);
                    if (wingnutNum == 1) {
                        pWingnutRef->setRotation(wingnutRotation);  
                    }
                    else {
                        pWingnutRef->setRotation(wingnutRotation + M_PI);  
                    }
                    pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  

                    if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                        
                    }
                    else {
                        acutPrintf(_T("\nFailed to place wingnut."));
                    }
                    pWingnutRef->close();  
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
                                            case 0: 
                                                wingnutPosition.y += (wingtieOffset + walerOffset);
                                                wingnutRotation += M_PI;
                                                break;
                                            case 1: 
                                                wingnutPosition.x += (wingtieOffset + walerOffset);
                                                break;
                                            case 2: 
                                                wingnutPosition.y -= (wingtieOffset + walerOffset);
                                                wingnutRotation += M_PI;
                                                break;
                                            case 3: 
                                                wingnutPosition.x -= (wingtieOffset + walerOffset);
                                                break;
                                            case -1:
                                                break;
                                            }
                                        }
                                        else {
                                            switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                                            case 0: 
                                                wingnutPosition.y += wingtieOffset;
                                                wingnutRotation += M_PI;
                                                break;
                                            case 1: 
                                                wingnutPosition.x += wingtieOffset;
                                                break;
                                            case 2: 
                                                wingnutPosition.y -= wingtieOffset;
                                                wingnutRotation += M_PI;
                                                break;
                                            case 3: 
                                                wingnutPosition.x -= wingtieOffset;
                                                break;
                                            case -1:
                                                break;
                                            }
                                        }
                                        pWingnutRef->setPosition(wingnutPosition);
                                        pWingnutRef->setBlockTableRecord(assetIdWingnut);
                                        if (wingnutNum == 1) {
                                            pWingnutRef->setRotation(wingnutRotation);  
                                        }
                                        else {
                                            pWingnutRef->setRotation(wingnutRotation + M_PI);  
                                        }
                                        pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  

                                        if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                                            
                                        }
                                        else {
                                            acutPrintf(_T("\nFailed to place wingnut."));
                                        }
                                        pWingnutRef->close();  

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



    
    for (const auto& panel : cornerTie) {
        if (panel.length > 100 && !panel.firstOrLast) {
            int tiesToPlace = 2;
            if (panel.height == 600) {
                tiesToPlace = 1;
            }
            for (int tiePlaced = 0; tiePlaced < tiesToPlace; tiePlaced++) {
                
                currentPointWithHeight = panel.position;
                currentPointWithHeight.z += tieOffsetHeight[tiePlaced];

                switch (static_cast<int>(round(panel.rotation / M_PI_2))) {
                case 0: 
                    currentPointWithHeight.x += yOffset;
                    currentPointWithHeight.y += xOffset;
                    break;
                case 1: 
                    currentPointWithHeight.x -= xOffset;
                    currentPointWithHeight.y += yOffset;
                    break;
                case 2: 
                    currentPointWithHeight.x -= yOffset;
                    currentPointWithHeight.y -= xOffset;
                    break;
                case 3: 
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
                    case 0: 
                        wingnutPosition.y += wingtieOffset;
                        wingnutRotation += M_PI;
                        break;
                    case 1: 
                        wingnutPosition.x += wingtieOffset;
                        break;
                    case 2: 
                        wingnutPosition.y -= wingtieOffset;
                        wingnutRotation += M_PI;
                        break;
                    case 3: 
                        wingnutPosition.x -= wingtieOffset;
                        break;
                    case -1:
                        break;
                    }

                    pWingnutRef->setPosition(wingnutPosition);
                    pWingnutRef->setBlockTableRecord(assetIdWingnut);
                    if (wingnutNum == 1) {
                        pWingnutRef->setRotation(wingnutRotation);  
                    }
                    else {
                        pWingnutRef->setRotation(wingnutRotation + M_PI);  
                    }
                    pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  

                    if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                        
                    }
                    else {
                        acutPrintf(_T("\nFailed to place wingnut."));
                    }
                    pWingnutRef->close();  
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
                                        case 0: 
                                            wingnutPosition.y += wingtieOffset;
                                            wingnutRotation += M_PI;
                                            break;
                                        case 1: 
                                            wingnutPosition.x += wingtieOffset;
                                            break;
                                        case 2: 
                                            wingnutPosition.y -= wingtieOffset;
                                            wingnutRotation += M_PI;
                                            break;
                                        case 3: 
                                            wingnutPosition.x -= wingtieOffset;
                                            break;
                                        case -1:
                                            break;
                                        }
                                        pWingnutRef->setPosition(wingnutPosition);
                                        pWingnutRef->setBlockTableRecord(assetIdWingnut);
                                        if (wingnutNum == 1) {
                                            pWingnutRef->setRotation(wingnutRotation);  
                                        }
                                        else {
                                            pWingnutRef->setRotation(wingnutRotation + M_PI);  
                                        }
                                        pWingnutRef->setScaleFactors(AcGeScale3d(globalVarScale));  

                                        if (pModelSpace->appendAcDbEntity(pWingnutRef) == Acad::eOk) {
                                            
                                        }
                                        else {
                                            acutPrintf(_T("\nFailed to place wingnut."));
                                        }
                                        pWingnutRef->close();  

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
