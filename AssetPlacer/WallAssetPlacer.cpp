// Created by:Ani  (2024-05-31)
// WallAssetPlacer.cpp

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

//// Calculate the angle between two vectors
//double calculateAngle(const AcGeVector3d& v1, const AcGeVector3d& v2) {
//    double dotProduct = v1.dotProduct(v2);
//    double lengthsProduct = v1.length() * v2.length();
//    return acos(dotProduct / lengthsProduct) * (180.0 / M_PI);
//}
//
//// Determine if an angle represents a corner
//bool isCorner(double angle, double threshold = 45.0) {
//    return angle > threshold;
//}
//
//// Extract vertices from a polyline
//void detectVertices(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& vertices) {
//    int numVerts = pPolyline->numVerts();
//    for (int i = 0; i < numVerts; ++i) {
//        AcGePoint3d point;
//        pPolyline->getPointAt(i, point);
//        vertices.push_back(point);
//    }
//}
//
//// Process polyline to detect corners
//void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold = 45.0) {
//    std::vector<AcGePoint3d> vertices;
//    detectVertices(pPolyline, vertices);
//
//    size_t numVerts = vertices.size();
//    for (size_t i = 0; i < numVerts; ++i) {
//        AcGeVector3d currentDirection, nextDirection;
//        if (i < numVerts - 1) {
//            currentDirection = vertices[i + 1] - vertices[i];
//        }
//        else {
//            currentDirection = vertices[0] - vertices[i];
//        }
//        currentDirection.normalize();
//
//        if (i > 0) {
//            nextDirection = vertices[i] - vertices[i - 1];
//        }
//        else {
//            nextDirection = vertices[i] - vertices[numVerts - 1];
//        }
//        nextDirection.normalize();
//
//        double angle = calculateAngle(currentDirection, nextDirection);
//        if (isCorner(angle, angleThreshold)) {
//            corners.push_back(vertices[i]);
//        }
//    }
//}

std::vector<AcGePoint3d> WallPlacer::detectPolylines() {
    acutPrintf(_T("\nDetecting polylines..."));
    std::vector<AcGePoint3d> corners;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return corners;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return corners;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return corners;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return corners;
    }

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            acutPrintf(_T("\nEntity type: %s"), pEnt->isA()->name());
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPolyline = AcDbPolyline::cast(pEnt);
                if (pPolyline) {
                    processPolyline(pPolyline, corners);
                }
            }
            pEnt->close();
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;
}

AcDbObjectId WallPlacer::loadAsset(const wchar_t* blockName) {
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

void WallPlacer::placeWallSegment(const AcGePoint3d& start, const AcGePoint3d& end, AcDbObjectId assetId) {
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
    double distance = start.distanceTo(end);
    int numPanels = static_cast<int>(distance / 60);  // Calculate the number of panels
    AcGeVector3d direction = (end - start).normal();
    AcGePoint3d currentPoint = start + direction * 25;

    for (int i = 0; i < numPanels; i++) {
        double rotation = atan2(direction.y, direction.x);

        // Place the wall segment without scaling
        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
        pBlockRef->setPosition(currentPoint);
        pBlockRef->setBlockTableRecord(assetId);
        pBlockRef->setRotation(rotation);  // Apply rotation
        pBlockRef->setScaleFactors(AcGeScale3d(0.1, 0.1, 0.1));  // Ensure no scaling

        if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
            acutPrintf(_T("\nWall segment placed successfully."));
        }
        else {
            acutPrintf(_T("\nFailed to place wall segment."));
        }
        pBlockRef->close();  // Decrement reference count

        currentPoint += direction * 60;  // Move to the next panel

        if (currentPoint.distanceTo(end) < 60) {
            break;  // Stop if the remaining distance is less than a panel length
        }
    }

    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}

void WallPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
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

    AcDbText* pText = new AcDbText(position, text, AcDbObjectId::kNull, 0.2, 0);
    if (pModelSpace->appendAcDbEntity(pText) == Acad::eOk) {
        acutPrintf(_T("\nAdded text annotation: %s"), text);
    }
    else {
        acutPrintf(_T("\nFailed to add text annotation."));
    }
    pText->close();  // Decrement reference count

    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}

void WallPlacer::placeWalls() {
    acutPrintf(_T("\nPlacing walls..."));
    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo polylines detected."));
        return;
    }

    AcDbObjectId assetId = loadAsset(L"128282X");

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (size_t i = 0; i < corners.size() - 1; ++i) {
        placeWallSegment(corners[i], corners[i + 1], assetId);
    }

    acutPrintf(_T("\nCompleted placing walls."));
}
