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
#include <typeinfo>
#include <algorithm>
#include <AcDb.h>
#include <AcDb/AcDbBlockTable.h>
#include <AcDb/AcDbBlockTableRecord.h>
#include <AcDb/AcDbPolyline.h>
#include <AcGe/AcGeMatrix3d.h>
#include "AcDb/AcDbBlockReference.h"
#include <sstream>
#include <iostream>

// Tolerance for comparing angles
const double TOLERANCE = 0.19;

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

// Function to calculate cardinal direction between start and end points
AcGeVector3d calculateCardinalDirection(const AcGePoint3d& start, const AcGePoint3d& end, double tolerance) {
    double dx = end.x - start.x;
    double dy = end.y - start.y;

    // Snap the direction to the closest cardinal direction (horizontal or vertical)
    if (std::abs(dx) < tolerance) dx = 0;  // Snap to vertical direction if difference is small
    if (std::abs(dy) < tolerance) dy = 0;  // Snap to horizontal direction if difference is small

    // Normalize the direction vector to cardinal directions (-1, 0, or 1)
    if (dx != 0) dx /= std::abs(dx);  // Normalize to either -1 or 1 for horizontal direction
    if (dy != 0) dy /= std::abs(dy);  // Normalize to either -1 or 1 for vertical direction

    return AcGeVector3d(dx, dy, 0); // Return the normalized direction vector (x, y direction)
}

// Adjust the start and end points to snap them to a grid or cardinal directions
void adjustStartAndEndPoints(AcGePoint3d& start, AcGePoint3d& end, double tolerance) {
    // Snap both start and end points to nearest grid or cardinal direction
    start.x = std::round(start.x / tolerance) * tolerance;
    start.y = std::round(start.y / tolerance) * tolerance;

    end.x = std::round(end.x / tolerance) * tolerance;
    end.y = std::round(end.y / tolerance) * tolerance;

    //// Debug output to confirm snapping
    //acutPrintf(L"\nSnapped Start: %.2f, %.2f, Snapped End: %.2f, %.2f", start.x, start.y, end.x, end.y);
}

// Normalize an angle to the range [0, 2*PI]
double normalizeAngle(double angle) {
    angle = fmod(angle, 2 * M_PI); // Reduce angle to within one full rotation

    if (angle < 0) {
        angle += 2 * M_PI; // Ensure the angle is positive
    }

    return angle;
}

double snapToPredefinedValues(double distance) {
    // Predefined snap values
    std::vector<double> predefinedValues = {
        150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800, 850,
        900, 950, 1000, 1050, 1100, 1150, 1200, 1250, 1300, 1350, 1400, 1450, 1500,
        1550, 1600, 1650, 1700, 1750, 1800, 1850, 1900, 1950, 2000, 2050, 2100
    };

    // Find the nearest predefined value
    auto closest = *std::min_element(predefinedValues.begin(), predefinedValues.end(),
        [distance](double a, double b) {
            return std::abs(a - distance) < std::abs(b - distance);
        });
	//print the distance we are snapping to
	//acutPrintf(_T("\nDistance snapped to: %f\n"), closest);

    return closest;
}

//Snap to predefined angles
double snapToExactAngle(double angle, double tolerance = 0.01) {
    const double snapAngles[] = { 0, M_PI_2, M_PI, 3 * M_PI_2, 2 * M_PI };

    // Normalize the angle to the range [0, 2 * PI]
    angle = fmod(angle, 2 * M_PI);
    if (angle < 0) angle += 2 * M_PI;

    // Snap to nearest exact angle within tolerance
    for (double snapAngle : snapAngles) {
        if (fabs(angle - snapAngle) < tolerance) {
            return snapAngle;
        }
    }

    // Handle special case where angle is near 2 * PI or 0
    if (fabs(angle - 2 * M_PI) < tolerance || fabs(angle) < tolerance) {
        return 0;
    }

    acutPrintf(_T("\nAngle %f not snapped to exact angle.\n"), angle);
    return angle;
}

// Determine if a corner is inside or outside based on the polyline direction
bool determineIfInsideCorner(const std::vector<AcGePoint3d>& polylinePoints, size_t currentIndex, bool isClockwise) {
    // Calculate previous and next indices
    size_t prevIndex = (currentIndex == 0) ? polylinePoints.size() - 1 : currentIndex - 1;
    size_t nextIndex = (currentIndex + 1) % polylinePoints.size();

    // Create vectors for current and next segments
    AcGeVector3d vec1 = polylinePoints[currentIndex] - polylinePoints[prevIndex];
    AcGeVector3d vec2 = polylinePoints[nextIndex] - polylinePoints[currentIndex];

    //Normalize vectors
    vec1.normalize();
    vec2.normalize();

    double crossProductZ = vec1.crossProduct(vec2).z;

    bool isConvex = crossProductZ > 0;

    // Return true if the corner is inside, considering convexity and winding direction
    return isConvex ? !isClockwise : isClockwise;
}

// Determine the direction of a polyline based on the total turns
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

// Calculate the direction vector between two points
AcGeVector3d calculateDirection(const AcGePoint3d& start, const AcGePoint3d& end) {
    AcGeVector3d direction = end - start;
    direction.normalize();
    return direction;
}

// Calculate the angle between two vectors
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

// Log a point with a message
void logVector(const AcGeVector3d& vector, const char* vectorName) {
    //acutPrintf(_T("\n%s: (%f, %f, %f)"), vectorName, vector.x, vector.y, vector.z);
}

// Log an angle with a message
void logAngle(double angle, const char* message) {
    //acutPrintf(_T("\n%s: %f radians (%f degrees)"), message, angle, angle * 180.0 / M_PI);
}

// Process the polyline to detect corners
void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold, double tolerance) {
    // Extract vertices from the polyline
    std::vector<AcGePoint3d> vertices;
    detectVertices(pPolyline, vertices);

    filterClosePoints(vertices, tolerance);

    size_t numVerts = vertices.size();
    if (numVerts < 3) {
        // If there are less than 3 vertices, no corners can be detected
        return;
    }

    // Iterate over the vertices to calculate the angle between adjacent segments
    for (size_t i = 0; i < numVerts; ++i) {  // Note: Iterate through all vertices to ensure wrap-around handling
        AcGeVector3d currentDirection, nextDirection;

        // Calculate the current direction vector (from current vertex to next)
        currentDirection = vertices[(i + 1) % numVerts] - vertices[i];
        currentDirection.normalize();  // Normalize the direction vector

        // Calculate the next direction vector (from previous vertex to current)
        if (i > 0) {
            nextDirection = vertices[i] - vertices[i - 1];
        }
        else {
            nextDirection = vertices[i] - vertices[numVerts - 1];  // Handle the wrap-around case
        }
        nextDirection.normalize();  // Normalize the direction vector

        // Calculate the angle between the current and next direction vectors
        double angle = calculateAngle(currentDirection, nextDirection);

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
    // Get vertices of the first polyline
    std::vector<AcGePoint3d> vertices1 = getPolylineVertices(pPolyline1);

    // Get vertices of the second polyline
    std::vector<AcGePoint3d> vertices2 = getPolylineVertices(pPolyline2);

    // Determine the minimum number of vertices
    size_t minSize = std::min(vertices1.size(), vertices2.size());

    // Predetermined distances
    std::vector<double> predeterminedValues = { 150, 200, 250, 300, 350, 400, 450, 500, 550, 600,
                                                650, 700, 750, 800, 850, 900, 950, 1000, 1050, 1100,
                                                1150, 1200, 1250, 1300, 1350, 1400, 1450, 1500,
                                                1550, 1600, 1650, 1700, 1750, 1800, 1850, 1900, 1950, 2000,
                                                2050, 2100 };

    double totalDistance = 0.0;

    for (size_t i = 0; i < minSize; ++i) {
        double deltaX = vertices2[i].x - vertices1[i].x;
        double deltaY = vertices2[i].y - vertices1[i].y;

        if (fabs(deltaX - deltaY) < 1e-6) {
            // If deltaX and deltaY are effectively the same, use that value as totalDistance
            totalDistance = deltaX; // Could also use deltaY since they are the same
        }
        else {
            // Find the closest value from predeterminedValues
            double distance = sqrt(deltaX * deltaX + deltaY * deltaY);
            double closestValue = predeterminedValues[0];
            double minDifference = fabs(distance - closestValue);
            //acutPrintf(_T("\nDeltaX: %f, DeltaY: %f"), deltaX, deltaY);
            for (double value : predeterminedValues) {
                double difference = fabs(distance - value);
                if (difference < minDifference) {
                    minDifference = difference;
                    closestValue = value;
                }
            }
            totalDistance = closestValue;
        }
    }
    //print deltaX and deltaY
	
    //acutPrintf(_T("\nClosest matching distance: %f"), totalDistance);

    return totalDistance;
}

// Function to ensure polyline is clockwise
std::vector<AcGePoint3d> forcePolylineClockwise(std::vector<AcGePoint3d>& points) {
    if (points.empty()) return points;

    // Check if the polyline is already clockwise
    if (!isPolylineClockwise(points)) {
        // If not, reverse the points to make it clockwise
        std::reverse(points.begin(), points.end());
    }

    return points;
}

// Function to determine if a polyline is clockwise
bool isPolylineClockwise(const std::vector<AcGePoint3d>& points) {
    double sum = 0.0;
    const double tolerance = 1e-6;

    for (size_t i = 0; i < points.size(); ++i) {
        AcGePoint3d current = points[i];
        AcGePoint3d next = points[(i + 1) % points.size()];

        double contribution = (next.x - current.x) * (next.y + current.y);
        sum += contribution;
    }

    if (sum > tolerance) {
        //acutPrintf(_T("\nPolyline is Clockwise\n"));
        return true;
    }
    else if (sum < -tolerance) {
        //acutPrintf(_T("\nPolyline is Counterclockwise\n"));
        return false;
    }
    else {
        //acutPrintf(_T("\nPolyline is nearly collinear\n"));
        return false; // Handle as needed
    }
}

// Function to determine if a corner is inside or outside based on the polyline direction
bool isInsideCorner(const std::vector<AcGePoint3d>& polylinePoints, size_t currentIndex, bool isClockwise) {
    // Calculate previous and next indices
    size_t prevIndex = (currentIndex == 0) ? polylinePoints.size() - 1 : currentIndex - 1;
    size_t nextIndex = (currentIndex + 1) % polylinePoints.size();

    // Create vectors for current and next segments
    AcGeVector3d vec1 = polylinePoints[currentIndex] - polylinePoints[prevIndex];
    AcGeVector3d vec2 = polylinePoints[nextIndex] - polylinePoints[currentIndex];

    // Calculate the cross product to determine convexity
    double crossProductZ = vec1.x * vec2.y - vec1.y * vec2.x;

    // Print debug information for each corner
    //acutPrintf(_T("\nCorner %d: Prev(%.2f, %.2f), Current(%.2f, %.2f), Next(%.2f, %.2f)"),
        //currentIndex, polylinePoints[prevIndex].x, polylinePoints[prevIndex].y,
        //polylinePoints[currentIndex].x, polylinePoints[currentIndex].y,
        //polylinePoints[nextIndex].x, polylinePoints[nextIndex].y);

   //acutPrintf(_T("\nVector 1: (%.2f, %.2f), Vector 2: (%.2f, %.2f)"), vec1.x, vec1.y, vec2.x, vec2.y);
   //acutPrintf(_T("\nCross Product Z: %.2f"), crossProductZ);

    // Inside/Outside determination based on polyline direction
    bool isInside = isClockwise ? (crossProductZ < 0) : (crossProductZ > 0);

    // Print the final determination for this corner
    //acutPrintf(_T("\nCorner %d is %s"), currentIndex, isInside ? "Inside" : "Outside");

    return isInside;
}

// Function to filter out points that are too close to each other
void filterClosePoints(std::vector<AcGePoint3d>& vertices, double tolerance) {
    std::vector<AcGePoint3d> filteredVertices;
    //acutPrintf(_T("\nFiltering points with tolerance: %f"), tolerance);

    for (size_t i = 0; i < vertices.size(); ++i) {
        // Check if the current vertex is too close to the previous vertex
        if (i == 0 || vertices[i].distanceTo(vertices[i - 1]) > tolerance) {
            filteredVertices.push_back(vertices[i]);
        }
        else {
            //acutPrintf(_T("\nSkipping vertex at (%f, %f, %f) due to proximity to the previous vertex"),
                //vertices[i].x, vertices[i].y, vertices[i].z);
        }
    }

    // Handle case where the last vertex might be too close to the first one (for closed polylines)
    if (filteredVertices.size() > 1 && filteredVertices.back().distanceTo(filteredVertices.front()) <= tolerance) {
        //acutPrintf(_T("\nSkipping last vertex due to proximity to the first vertex"));
        filteredVertices.pop_back();
    }

    vertices.swap(filteredVertices);
}

// Function to adjust the rotation for a corner based on the direction of the corner
void adjustRotationForCorner(double& rotation, const std::vector<AcGePoint3d>& corners, size_t cornerNum) {
    AcGeVector3d prevDirection = corners[cornerNum] - corners[cornerNum > 0 ? cornerNum - 1 : corners.size() - 1];
    AcGeVector3d nextDirection = corners[(cornerNum + 1) % corners.size()] - corners[cornerNum];
    double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;

    if (crossProductZ > 0) {
        rotation += M_PI_2;
    }
}

// Function to determine the direction of the drawing
bool isItInteger(double value, double tolerance) {
    return std::abs(value - std::round(value)) < tolerance;
}

// Function to calculate the distance between the first two polylines in the drawing
double calculateDistanceBetweenPolylines() {
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

    // Find the first two polylines
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
                    break; // Found both polylines, no need to continue
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