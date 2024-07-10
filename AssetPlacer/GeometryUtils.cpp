//TODO: Missing side for rectangle

#include "stdafx.h"
#include "GeometryUtils.h"
#include "SharedDefinations.h"
#include <cmath>

double calculateAngle(const AcGeVector3d& v1, const AcGeVector3d& v2) {
    double dotProduct = v1.dotProduct(v2);
    double lengthsProduct = v1.length() * v2.length();
    return acos(dotProduct / lengthsProduct) * (180.0 / M_PI);
}

bool isCorner(double angle, double threshold) {
    return angle > threshold;
}

void detectVertices(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& vertices) {
    int numVerts = pPolyline->numVerts();
    for (int i = 0; i < numVerts; ++i) {
        AcGePoint3d point;
        pPolyline->getPointAt(i, point);
        vertices.push_back(point);
    }
}

void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold) {
    std::vector<AcGePoint3d> vertices;
    detectVertices(pPolyline, vertices);

    size_t numVerts = vertices.size();
    if (numVerts < 3) {
        // If there are less than 3 vertices, no corners can be detected
        return;
    }

    for (size_t i = 0; i < numVerts; ++i) {
        AcGeVector3d currentDirection, nextDirection;
        if (i < numVerts - 1) {
            currentDirection = vertices[i + 1] - vertices[i];
        }
        else {
            currentDirection = vertices[0] - vertices[i];
        }
        currentDirection.normalize();

        if (i > 0) {
            nextDirection = vertices[i] - vertices[i - 1];
        }
        else {
            nextDirection = vertices[i] - vertices[numVerts - 1];
        }
        nextDirection.normalize();

        double angle = calculateAngle(currentDirection, nextDirection);
        if (isCorner(angle, angleThreshold)) {
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

