#include "StdAfx.h"
#include "CornerAssetPlacer.h"
#include "SharedDefinations.h"
#include "GeometryUtils.h"
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <limits>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include "gepnt3d.h"

// Static member definition
std::map<AcGePoint3d, std::vector<AcGePoint3d>, CornerAssetPlacer::Point3dComparator> CornerAssetPlacer::wallMap;

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

// ADD TEXT ANNOTATION TO DRAWING
void CornerAssetPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
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

// PLACE ASSETS AT DETECTED CORNERS
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

    // Define tolerance value for identifying potential corners
    const double tol = 1.0;

    // Calculate the bounding box
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    for (const auto& pt : corners) {
        if (pt.x < minX) minX = pt.x;
        if (pt.x > maxX) maxX = pt.x;
        if (pt.y < minY) minY = pt.y;
        if (pt.y > maxY) maxY = pt.y;
    }

    // Initialize candidate sets
    std::vector<AcGePoint3d> topLeftCandidates;
    std::vector<AcGePoint3d> bottomLeftCandidates;
    std::vector<AcGePoint3d> bottomRightCandidates;
    std::vector<AcGePoint3d> topRightCandidates;

    // Classify points into quadrants
    for (const auto& pt : corners) {
        if (fabs(pt.x - minX) < tol && fabs(pt.y - maxY) < tol) {
            topLeftCandidates.push_back(pt);
        }
        if (fabs(pt.x - minX) < tol && fabs(pt.y - minY) < tol) {
            bottomLeftCandidates.push_back(pt);
        }
        if (fabs(pt.x - maxX) < tol && fabs(pt.y - minY) < tol) {
            bottomRightCandidates.push_back(pt);
        }
        if (fabs(pt.x - maxX) < tol && fabs(pt.y - maxY) < tol) {
            topRightCandidates.push_back(pt);
        }
    }

    // Function to find the closest point to the specified corner
    auto findClosestPoint = [](const std::vector<AcGePoint3d>& candidates, const AcGePoint3d& corner) -> AcGePoint3d {
        AcGePoint3d closestPoint;
        double minDist = std::numeric_limits<double>::max();

        for (const auto& pt : candidates) {
            double dist = pt.distanceTo(corner);
            if (dist < minDist) {
                minDist = dist;
                closestPoint = pt;
            }
        }
        return closestPoint;
        };

    // Determine the actual corner points
    AcGePoint3d topLeftCorner = findClosestPoint(topLeftCandidates, AcGePoint3d(minX, maxY, 0));
    AcGePoint3d bottomLeftCorner = findClosestPoint(bottomLeftCandidates, AcGePoint3d(minX, minY, 0));
    AcGePoint3d bottomRightCorner = findClosestPoint(bottomRightCandidates, AcGePoint3d(maxX, minY, 0));
    AcGePoint3d topRightCorner = findClosestPoint(topRightCandidates, AcGePoint3d(maxX, maxY, 0));

    // Place corner post and panels at identified corners
    placeCornerPostAndPanels(topLeftCorner, 0.0, cornerPostId, panelId);
    addTextAnnotation(topLeftCorner, L"Top-Left Corner");

    placeCornerPostAndPanels(bottomLeftCorner, M_PI_2, cornerPostId, panelId);
    addTextAnnotation(bottomLeftCorner, L"Bottom-Left Corner");

    placeCornerPostAndPanels(bottomRightCorner, M_PI, cornerPostId, panelId);
    addTextAnnotation(bottomRightCorner, L"Bottom-Right Corner");

    placeCornerPostAndPanels(topRightCorner, M_3PI_2, cornerPostId, panelId);
    addTextAnnotation(topRightCorner, L"Top-Right Corner");

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

// PLACE CORNER POST AND PANELS
void CornerAssetPlacer::placeCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelId) {
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

    // Place the corner post at the detected corner
    AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
    pCornerPostRef->setPosition(corner);
    pCornerPostRef->setBlockTableRecord(cornerPostId);
    pCornerPostRef->setRotation(rotation);
    pCornerPostRef->setScaleFactors(AcGeScale3d(0.1, 0.1, 0.1));  // Ensure no scaling

    if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
        acutPrintf(_T("\nCorner post placed successfully."));
    }
    else {
        acutPrintf(_T("\nFailed to place corner post."));
    }
    pCornerPostRef->close();  // Decrement reference count

    // Determine panel placement positions based on the rotation
    AcGeVector3d panelAOffset, panelBOffset;
    if (rotation == 0.0) {
        panelAOffset = AcGeVector3d(10.0, 0.0, 0.0);  // Panel A along the X-axis
        panelBOffset = AcGeVector3d(0.0, -25.0, 0.0);  // Panel B along the Y-axis
    }
    else if (rotation == M_PI_2) {
        panelAOffset = AcGeVector3d(0.0, 10.0, 0.0);  // Panel A along the Y-axis
        panelBOffset = AcGeVector3d(25.0, 0.0, 0.0);  // Panel B along the X-axis
    }
    else if (rotation == M_PI) {
        panelAOffset = AcGeVector3d(-10.0, 0.0, 0.0);  // Panel A along the X-axis
        panelBOffset = AcGeVector3d(0.0, 25.0, 0.0);  // Panel B along the Y-axis
    }
    else if (rotation == M_3PI_2) {
        panelAOffset = AcGeVector3d(0.0, -10.0, 0.0);  // Panel A along the Y-axis
        panelBOffset = AcGeVector3d(-25.0, 0.0, 0.0);  // Panel B along the X-axis
    }

    AcGePoint3d panelPositionA = corner + panelAOffset;
    AcGePoint3d panelPositionB = corner + panelBOffset;

    // Place Panel A
    AcDbBlockReference* pPanelARef = new AcDbBlockReference();
    pPanelARef->setPosition(panelPositionA);
    pPanelARef->setBlockTableRecord(panelId);
    pPanelARef->setRotation(rotation);
    pPanelARef->setScaleFactors(AcGeScale3d(0.1, 0.1, 0.1));  // Ensure no scaling

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
    pPanelBRef->setScaleFactors(AcGeScale3d(0.1, 0.1, 0.1));  // Ensure no scaling

    if (pModelSpace->appendAcDbEntity(pPanelBRef) == Acad::eOk) {
        acutPrintf(_T("\nPanel B placed successfully."));
    }
    else {
        acutPrintf(_T("\nFailed to place Panel B."));
    }
    pPanelBRef->close();  // Decrement reference count

    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}
