// Created by:Ani  (2024-05-31)
// Modified on (2024-07-11)
// TODO:
// GeometryUtils.cpp
// This file contains utility functions for geometry calculations.
// The calculateAngle function calculates the angle between two vectors.
// The isCorner function determines if an angle is a corner based on a threshold.
// The normalizeAngle function normalizes an angle to the range [0, 2*PI].
// The snapToExactAngle function snaps an angle to the nearest exact angle (0, 90, 180, 270).
// The areAnglesEqual function determines if two angles are equal within a tolerance.
// The detectVertices function detects the vertices of a polyline.
// The processPolyline function processes a polyline to detect corners.
// The classifyPolylineEntities function classifies polyline entities.
// The classifyPolylineEntities function is used to classify polyline entities in BricsCAD.
/////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GeometryUtils.h"
#include "SharedDefinations.h"
#include <cmath>
#include <algorithm>
#include <AcDb.h>
#include <AcDb/AcDbBlockTable.h>
#include <AcDb/AcDbBlockTableRecord.h>
#include <AcDb/AcDbPolyline.h>

const double TOLERANCE = 0.1; // Tolerance for comparing angles

// Calculate angle between two vectors
double calculateAngle(const AcGeVector3d& v1, const AcGeVector3d& v2) {
    double dotProduct = v1.dotProduct(v2);
    double lengthsProduct = v1.length() * v2.length();
    return acos(dotProduct / lengthsProduct) * (180.0 / M_PI);
}

// Determine if an angle is a corner based on a threshold
bool isCorner(double angle, double threshold) {
    return fabs(angle - threshold) <= TOLERANCE;
}

double normalizeAngle(double angle) {
    while (angle < 0) {
        angle += 2 * M_PI;
    }
    while (angle >= 2 * M_PI) {
        angle -= 2 * M_PI;
    }
    return angle;
}

double snapToExactAngle(double angle, double TOLERANCE) {
    if (fabs(angle - 0) < TOLERANCE) return 0;
    if (fabs(angle - M_PI_2) < TOLERANCE) return M_PI_2;
    if (fabs(angle - M_PI) < TOLERANCE) return M_PI;
    if (fabs(angle - 3 * M_PI_2) < TOLERANCE) return 3 * M_PI_2;
    return angle;
}

// Determine if two angles are equal within a tolerance
bool areAnglesEqual(double angle1, double angle2, double tolerance) {
    return std::abs(angle1 - angle2) < tolerance;
}

// Detect vertices of the polyline
void detectVertices(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& vertices) {
    int numVerts = pPolyline->numVerts();
    for (int i = 0; i < numVerts; ++i) {
        AcGePoint3d point;
        pPolyline->getPointAt(i, point);
        vertices.push_back(point);
    }

    // If polyline has 4 vertices, simulate closure by adding the first vertex as the closing vertex
    if (numVerts == 4) {
        vertices.push_back(vertices[0]);
    }
}

// Process the polyline to detect corners
void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold, double tolerance) {
    std::vector<AcGePoint3d> vertices;
    detectVertices(pPolyline, vertices);

    size_t numVerts = vertices.size();
    if (numVerts < 3) {
        // If there are less than 3 vertices, no corners can be detected
        return;
    }

    for (size_t i = 0; i < numVerts - 1; ++i) {
        AcGeVector3d currentDirection, nextDirection;
        currentDirection = vertices[i + 1] - vertices[i];
        currentDirection.normalize();

        if (i > 0) {
            nextDirection = vertices[i] - vertices[i - 1];
        }
        else {
            nextDirection = vertices[i] - vertices[numVerts - 2];
        }
        nextDirection.normalize();

        double angle = calculateAngle(currentDirection, nextDirection);
        if (areAnglesEqual(angle, angleThreshold, tolerance)) {
            corners.push_back(vertices[i]);
        }
        else {
            // Still add the vertex if it's not considered a corner to ensure all vertices are included
            corners.push_back(vertices[i]);
        }
    }

    // Ensure the last vertex is included
    if (std::find(corners.begin(), corners.end(), vertices.back()) == corners.end()) {
        corners.push_back(vertices.back());
    }
}

// Function to classify polyline entities
void classifyPolylineEntities(AcDbDatabase* pDb, std::vector<AcGePoint3d>& detectedCorners, double angleThreshold) {
    AcDbBlockTable* pBlockTable;
    AcDbBlockTableRecord* pBlockTableRecord;

    acutPrintf(_T("\nAttempting to get block table.\n"));
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table. Error status: %d\n"), es);
        return;
    }

    acutPrintf(_T("\nAttempting to get model space.\n"));
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space. Error status: %d\n"), es);
        pBlockTable->close();
        return;
    }

    acutPrintf(_T("\nSuccessfully accessed model space.\n"));

    AcDbBlockTableRecordIterator* pIterator;
    es = pBlockTableRecord->newIterator(pIterator);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to create block table record iterator. Error status: %d\n"), es);
        pBlockTableRecord->close();
        pBlockTable->close();
        return;
    }

    acutPrintf(_T("\nIterating through entities.\n"));
    for (; !pIterator->done(); pIterator->step()) {
        AcDbEntity* pEntity;
        es = pIterator->getEntity(pEntity, AcDb::kForRead);
        if (es != Acad::eOk) {
            acutPrintf(_T("\nFailed to get entity. Error status: %d\n"), es);
            continue;
        }

        if (pEntity->isKindOf(AcDbPolyline::desc())) {
            AcDbPolyline* pPolyline = AcDbPolyline::cast(pEntity);
            acutPrintf(_T("\nPolyline detected.\n"));
            if (pPolyline->isClosed() || pPolyline->numVerts() == 4) { // Treat 4-vertex polylines as closed rectangles
                acutPrintf(_T("\nClosed or 4-vertex polyline detected. Processing...\n"));
                processPolyline(pPolyline, detectedCorners, angleThreshold, TOLERANCE);
            }
            else {
                acutPrintf(_T("\nDetected open polyline. Skipping...\n"));
            }
        }
        pEntity->close();
    }

    delete pIterator;
    pBlockTableRecord->close();
    pBlockTable->close();

    acutPrintf(_T("\nDetected %d corners from lines.\n"), detectedCorners.size());
}
