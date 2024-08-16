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
#include <AcGe/AcGeMatrix3d.h>
#include "AcDb/AcDbBlockReference.h"

const double TOLERANCE = 0.19; // Tolerance for comparing angles

// Manual clamp function
template <typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Calculate the dot product and return the angle in radians
double calculateAngle(const AcGeVector3d& v1, const AcGeVector3d& v2) {
    // Normalize the vectors
    AcGeVector3d normV1 = v1.normal();
    AcGeVector3d normV2 = v2.normal();

    double dotProduct = normV1.dotProduct(normV2);

    // Clamp the dot product to the range [-1, 1] to avoid issues with acos
    dotProduct = clamp(dotProduct, -1.0, 1.0);

    return acos(dotProduct);
}

// Determine if an angle is a corner based on a threshold
bool isCorner(double angle, double threshold) {
    return fabs(angle - threshold) <= TOLERANCE;
}

// Normalize an angle to the range [0, 2*PI]
double normalizeAngle(double angle) {
    angle = fmod(angle, 2 * M_PI); // Reduce angle to within one full rotation

    if (angle < 0) {
        angle += 2 * M_PI; // Ensure the angle is positive
    }

    return angle;
}

// Snap an angle to the nearest exact angle (0, 90, 180, 270 degrees)
double snapToExactAngle(double angle, double tolerance = 0.19) {
    const double snapAngles[] = { 0, M_PI_2, M_PI, 3 * M_PI_2 };

    for (double snapAngle : snapAngles) {
        if (fabs(angle - snapAngle) < tolerance) {
            return snapAngle;
        }
    }

    // Special case handling
    if (fabs(angle - 2 * M_PI) < tolerance || fabs(angle) < tolerance) {
        return 0;
    }

    acutPrintf(_T("\nAngle %f not snapped to exact angle.\n"), angle);
    return angle;
}

bool determineIfInsideCorner(const std::vector<AcGePoint3d>& polylinePoints, size_t currentIndex, bool isClockwise) {
    // Calculate previous and next indices
    size_t prevIndex = (currentIndex == 0) ? polylinePoints.size() - 1 : currentIndex - 1;
    size_t nextIndex = (currentIndex + 1) % polylinePoints.size();

    // Create vectors for current and next segments
    AcGeVector3d vec1 = polylinePoints[currentIndex] - polylinePoints[prevIndex];
    AcGeVector3d vec2 = polylinePoints[nextIndex] - polylinePoints[currentIndex];

    // Calculate the cross product to determine convexity
    double crossProductZ = vec1.x * vec2.y - vec1.y * vec2.x;

    // Calculate the angle between the two vectors
    double angle = acos(vec1.dotProduct(vec2) / (vec1.length() * vec2.length()));

    // Determine if the corner is convex
    bool isConvex = (crossProductZ > 0) ? (angle < M_PI) : (angle > M_PI);

    // Return true if the corner is inside, considering convexity and winding direction
    return isConvex ? !isClockwise : isClockwise;
}

bool directionOfPolyline(const std::vector<AcGePoint3d>& polylinePoints) {
    double totalTurns = 0.0;

    for (size_t i = 0; i < polylinePoints.size(); ++i) {
        size_t nextIndex = (i + 1) % polylinePoints.size();
        size_t nextNextIndex = (i + 2) % polylinePoints.size();

        AcGeVector3d vec1 = polylinePoints[nextIndex] - polylinePoints[i];
        AcGeVector3d vec2 = polylinePoints[nextNextIndex] - polylinePoints[nextIndex];

        totalTurns += vec1.x * vec2.y - vec1.y * vec2.x;
    }

    return totalTurns > 0;
}

// Determine if two angles are equal within a tolerance
bool areAnglesEqual(double angle1, double angle2, double tolerance = 0.1) {
    // Normalize angles to the range [0, 2*PI]
    angle1 = normalizeAngle(angle1);
    angle2 = normalizeAngle(angle2);
    return fabs(angle1 - angle2) < tolerance;
}

//// Use the cross product to check for perpendicularity
//bool arePerpendicular(const AcGeVector3d& v1, const AcGeVector3d& v2, double tolerance = TOLERANCE) {
//    double crossProductZ = v1.crossProduct(v2).z;
//    return fabs(crossProductZ) < tolerance;
//}

AcGeVector3d calculateDirection(const AcGePoint3d& start, const AcGePoint3d& end) {
    AcGeVector3d direction = end - start;
    direction.normalize();
    return direction;
}

double calculateAngleBetweenVectors(const AcGeVector3d& v1, const AcGeVector3d& v2) {
    double dotProduct = v1.dotProduct(v2);
    dotProduct = clamp(dotProduct, -1.0, 1.0); // Ensure dot product is within valid range
    return acos(dotProduct);
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

void logVector(const AcGeVector3d& vector, const char* vectorName) {
    acutPrintf(_T("\n%s: (%f, %f, %f)"), vectorName, vector.x, vector.y, vector.z);
}

void logAngle(double angle, const char* message) {
    acutPrintf(_T("\n%s: %f radians (%f degrees)"), message, angle, angle * 180.0 / M_PI);
}

// Process the polyline to detect corners
void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold, double tolerance) {
    // Extract vertices from the polyline
    std::vector<AcGePoint3d> vertices;
    detectVertices(pPolyline, vertices);

    size_t numVerts = vertices.size();
    if (numVerts < 3) {
        // If there are less than 3 vertices, no corners can be detected
        return;
    }

    // Iterate over the vertices to calculate the angle between adjacent segments
    for (size_t i = 0; i < numVerts - 1; ++i) {
        AcGeVector3d currentDirection, nextDirection;

        // Calculate the current direction vector (from current vertex to next)
        currentDirection = vertices[i + 1] - vertices[i];
        currentDirection.normalize();  // Normalize the direction vector

        // Calculate the next direction vector (from previous vertex to current)
        if (i > 0) {
            nextDirection = vertices[i] - vertices[i - 1];
        }
        else {
            nextDirection = vertices[i] - vertices[numVerts - 2];  // Handle the wrap-around case
        }
        nextDirection.normalize();  // Normalize the direction vector

        // Calculate the angle between the current and next direction vectors
        double angle = calculateAngle(currentDirection, nextDirection);

        // Log the calculated angle for debugging
        logAngle(angle, "Calculated Angle");

        // If the calculated angle is close to the threshold, consider it a corner
        if (areAnglesEqual(angle, angleThreshold, tolerance)) {
            corners.push_back(vertices[i]);
        }
        else {
            // Optionally, include all vertices even if they are not considered corners
            corners.push_back(vertices[i]);
        }
    }

    // Ensure the last vertex is included in the corners vector
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

// Function to apply rotation around the x-axis
void rotateAroundXAxis(AcDbBlockReference* pBlockRef, double angle) {
    // Get the current position of the block
    AcGePoint3d position = pBlockRef->position();

    // Create a translation matrix to move the block to the origin
    AcGeMatrix3d translateToOrigin;
    translateToOrigin.setToTranslation(-position.asVector());

    // Create the rotation matrix around the X-axis
    AcGeMatrix3d rotationMatrix;
    rotationMatrix.setToRotation(angle, AcGeVector3d::kXAxis, AcGePoint3d::kOrigin);

    // Create a translation matrix to move the block back to its original position
    AcGeMatrix3d translateBack;
    translateBack.setToTranslation(position.asVector());

    // Combine all transformations
    AcGeMatrix3d finalTransform = translateBack * rotationMatrix * translateToOrigin;

    // Apply the final transformation
    pBlockRef->transformBy(finalTransform);
}

// Function to apply rotation around the y-axis
void rotateAroundYAxis(AcDbBlockReference* pBlockRef, double angle) {
    AcGePoint3d position = pBlockRef->position();
    AcGeMatrix3d translateToOrigin;
    translateToOrigin.setToTranslation(-position.asVector());

    AcGeMatrix3d rotationMatrix;
    rotationMatrix.setToRotation(angle, AcGeVector3d::kYAxis, AcGePoint3d::kOrigin);

    AcGeMatrix3d translateBack;
    translateBack.setToTranslation(position.asVector());

    AcGeMatrix3d finalMatrix = translateBack * rotationMatrix * translateToOrigin;
    pBlockRef->transformBy(finalMatrix);
}

// Function to apply rotation around the z-axis
void rotateAroundZAxis(AcDbBlockReference* pBlockRef, double angle) {
    //enable next line if this implementation does not work
    // AcGePoint3d position = pBlockRef->position();

    // Get the current position of the block
    AcGePoint3d position = pBlockRef->position();

    // Create a translation matrix to move the block to the origin
    AcGeMatrix3d translateToOrigin;
    translateToOrigin.setToTranslation(-position.asVector());

    // Create the rotation matrix around the Z-axis
    AcGeMatrix3d rotationMatrix;
    rotationMatrix.setToRotation(angle, AcGeVector3d::kZAxis, AcGePoint3d::kOrigin);

    // Create a translation matrix to move the block back to its original position
    AcGeMatrix3d translateBack;
    translateBack.setToTranslation(position.asVector());

    // Combine all transformations
    AcGeMatrix3d finalTransform = translateBack * rotationMatrix * translateToOrigin;

    // Apply the final transformation
    pBlockRef->transformBy(finalTransform);
}

// Function to get the vertices of an AcDbPolyline
std::vector<AcGePoint3d> getPolylineVertices(AcDbPolyline* pPolyline) {
    std::vector<AcGePoint3d> vertices;
    int numVerts = pPolyline->numVerts();
    for (int i = 0; i < numVerts; ++i) {
        AcGePoint3d point;
        pPolyline->getPointAt(i, point);
        vertices.push_back(point);
    }
    return vertices;
}

// Function to calculate the shift between corresponding vertices of two polylines
double getPolylineDistance(AcDbPolyline* pPolyline1, AcDbPolyline* pPolyline2) {
    acutPrintf(_T("\nEntering getPolylineDistance...\n"));

    // Get vertices of the first polyline
    std::vector<AcGePoint3d> vertices1 = getPolylineVertices(pPolyline1);
    acutPrintf(_T("Number of vertices in first polyline: %d\n"), vertices1.size());

    // Get vertices of the second polyline
    std::vector<AcGePoint3d> vertices2 = getPolylineVertices(pPolyline2);
    acutPrintf(_T("Number of vertices in second polyline: %d\n"), vertices2.size());

    // Determine the minimum number of vertices to use for comparison
    size_t minSize = std::min(vertices1.size(), vertices2.size());
    size_t maxSize = std::max(vertices1.size(), vertices2.size());

    // If the difference in vertex counts is greater than 1, log an error and return
    if (maxSize - minSize > 1) {
        acutPrintf(_T("\nThe polylines have significantly different numbers of vertices and cannot be directly compared.\n"));
        return -1.0;
    }

    acutPrintf(_T("\nCalculating shift between corresponding vertices...\n"));

    for (size_t i = 0; i < minSize; ++i) {
        double deltaX = vertices2[i].x - vertices1[i].x;
        double deltaY = vertices2[i].y - vertices1[i].y;

        // Check if the absolute values of deltaX and deltaY are the same
        if (fabs(deltaX) != fabs(deltaY)) {
            acutPrintf(_T("\nError: Absolute deltaX and deltaY are not the same. deltaX = %f, deltaY = %f\n"), deltaX, deltaY);
            return -1.0; // Return an error if they are not the same
        }

        // If they are the same, return either deltaX or deltaY (since they should be the same in magnitude)
        acutPrintf(_T("Vertex %d: deltaX = %f, deltaY = %f\n"), i, deltaX, deltaY);
        return fabs(deltaX); // Return the absolute value of deltaX or deltaY
    }

    acutPrintf(_T("Exiting getPolylineDistance...\n"));
    return -1.0; // In case no valid vertices are found
}

