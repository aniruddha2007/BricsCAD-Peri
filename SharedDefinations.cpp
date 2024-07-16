#include "StdAfx.h"
#include "SharedDefinations.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Function to calculate the area of a right-angled polygon using the Shoelace formula
double calculateRightAngledPolygonArea(const std::vector<Point2D>& vertices) {
    double area = 0.0;

    // Traverse each pair of vertices
    for (size_t i = 0; i < vertices.size(); ++i) {
        Point2D current = vertices[i];
        Point2D next = vertices[(i + 1) % vertices.size()];

        // Calculate area using the Shoelace formula for right-angled polygons
        area += (current.x * next.y) - (current.y * next.x);
    }

    return std::abs(area) / 2.0;
}

// Function to convert a string to uppercase
std::wstring toUpperCase(const std::wstring& str) {
    std::wstring upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::towupper);
    return upperStr;
}

// Function to convert AcGePoint3d to Point2D
Point2D convertTo2D(const AcGePoint3d& point) {
    return { point.x, point.y };
}