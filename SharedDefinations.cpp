#include "StdAfx.h"
#include "SharedDefinations.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include "acutads.h"

namespace SharedDefinations {

    // Function to calculate the area of a right-angled polygon using the Shoelace formula
    double calculateRightAngledPolygonArea(const std::vector<Point2D>& vertices) {
        double area = 0.0;
        for (size_t i = 0; i < vertices.size(); ++i) {
            Point2D current = vertices[i];
            Point2D next = vertices[(i + 1) % vertices.size()];
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

    // Function to process polygons and store in respective variables
    void processPolygons(const std::vector<std::vector<AcGePoint3d>>& polygonData,
        std::vector<std::vector<AcGePoint3d>>& outer,
        std::vector<std::vector<AcGePoint3d>>& inner) {

        std::vector<std::pair<double, std::vector<AcGePoint3d>>> polygonAreas;

        // Calculate areas and store with corresponding polygons
        for (const auto& polygon : polygonData) {
            std::vector<Point2D> vertices;
            for (const auto& point : polygon) {
                vertices.push_back(convertTo2D(point));
            }
            double area = calculateRightAngledPolygonArea(vertices);
            polygonAreas.push_back({ area, polygon });
        }

        // Sort polygons based on area
        std::sort(polygonAreas.begin(), polygonAreas.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        // Store the polygons in outer and inner based on the sorted areas
        if (polygonAreas.size() >= 2) {
            outer.push_back(polygonAreas[0].second); // Largest polygon
            inner.push_back(polygonAreas[1].second); // Second largest polygon
        }
    }

    // Example function to demonstrate usage with BricsCAD SDK
    void exampleUsage() {
        // Placeholder for actual polygon data extraction from BricsCAD
        std::vector<std::vector<AcGePoint3d>> polygonData; // Extracted polygon data

        // Variables to store larger and smaller polygons
        std::vector<std::vector<AcGePoint3d>> outer;
        std::vector<std::vector<AcGePoint3d>> inner;

        // Process polygons and store in respective variables
        processPolygons(polygonData, outer, inner);

        // Output results using acutPrintf (for demonstration purposes)
        acutPrintf(L"Outer Polygons: %d\n", outer.size());
        acutPrintf(L"Inner Polygons: %d\n", inner.size());

        for (size_t i = 0; i < outer.size(); ++i) {
            acutPrintf(L"Outer Polygon %d:\n", i + 1);
            for (const auto& point : outer[i]) {
                acutPrintf(L"(%f, %f, %f)\n", point.x, point.y, point.z);
            }
        }

        for (size_t i = 0; i < inner.size(); ++i) {
            acutPrintf(L"Inner Polygon %d:\n", i + 1);
            for (const auto& point : inner[i]) {
                acutPrintf(L"(%f, %f, %f)\n", point.x, point.y, point.z);
            }
        }
    }

}

// Function to convert a string to uppercase
std::wstring toUpperCase(const std::wstring& str) {
    std::wstring upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::towupper);
    return upperStr;
}