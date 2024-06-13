// Created by:Ani  (2024-05-31)
// WallAssetPlacer.cpp

#include "StdAfx.h"
#include "WallAssetPlacer.h"
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


std::vector<AcGePoint3d> WallPlacer::detectPolylines() {
    acutPrintf(_T("\nDetecting polylines..."));
    std::vector<AcGePoint3d> vertices;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return vertices;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return vertices;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return vertices;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return vertices;
    }

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            acutPrintf(_T("\nEntity type: %s"), pEnt->isA()->name());
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPolyline = AcDbPolyline::cast(pEnt);
                if (pPolyline) {
                    int numVerts = pPolyline->numVerts();
                    for (int i = 0; i < numVerts; i++) {
                        AcGePoint3d pt;
                        pPolyline->getPointAt(i, pt);
                        vertices.push_back(pt);
                    }
                }
            }
            pEnt->close();
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    acutPrintf(_T("\nDetected %d vertices from polylines."), vertices.size());
    return vertices;
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

	double distance = start.distanceTo(end);
	int numPanels = static_cast<int>(distance / 90);  // Each panel is 0.1 units long
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

		currentPoint += direction * 90;  // Move to the next panel

		if (currentPoint.distanceTo(end) < 90) {
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
    std::vector<AcGePoint3d> vertices = detectPolylines();

    if (vertices.empty()) {
        acutPrintf(_T("\nNo polylines detected."));
        return;
    }

    AcDbObjectId assetId = loadAsset(L"128280X");

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (size_t i = 0; i < vertices.size() - 1; ++i) {
        placeWallSegment(vertices[i], vertices[i + 1], assetId);
    }

    acutPrintf(_T("\nCompleted placing walls."));
}
// Path: WallAssetPlacer.h