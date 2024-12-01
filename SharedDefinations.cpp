#include "StdAfx.h"
#include "SharedDefinations.h"
#include <cmath>
#include <vector>
#include <algorithm>


double calculateRightAngledPolygonArea(const std::vector<Point2D>& vertices) {
    double area = 0.0;

    
    for (size_t i = 0; i < vertices.size(); ++i) {
        Point2D current = vertices[i];
        Point2D next = vertices[(i + 1) % vertices.size()];

        
        area += (current.x * next.y) - (current.y * next.x);
    }

    return std::abs(area) / 2.0;
}


std::wstring toUpperCase(const std::wstring& str) {
    std::wstring upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::towupper);
    return upperStr;
}


Point2D convertTo2D(const AcGePoint3d& point) {
    return { point.x, point.y };
}