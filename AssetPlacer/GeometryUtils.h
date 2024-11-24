#pragma once

#include "gepnt3d.h"
#include "dbents.h"
#include <vector>

double calculateAngle(const AcGeVector3d& v1, const AcGeVector3d& v2);
bool areAnglesEqual(double angle1, double angle2, double tolerance);
double normalizeAngle(double angle);
double calculateDistanceBetweenPolylines();
double snapToExactAngle(double angle, double tolerance);
void adjustStartAndEndPoints(AcGePoint3d& start, AcGePoint3d& end, double tolerance = 0.5);
AcGeVector3d calculateCardinalDirection(const AcGePoint3d& start, const AcGePoint3d& end, double tolerance = 0.5);

double snapToPredefinedValues(double distance);
bool isCorner(double angle, double threshold = 45.0);
bool determineIfInsideCorner(const std::vector<AcGePoint3d>& polylinePoints, size_t currentIndex, bool isClockwise);
bool directionOfPolyline(const std::vector<AcGePoint3d>& polylinePoints);
void detectVertices(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& vertices);
void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold, double tolerance);
void rotateAroundXAxis(AcDbBlockReference* pBlockRef, double angle);
void rotateAroundYAxis(AcDbBlockReference* pBlockRef, double angle);
void rotateAroundZAxis(AcDbBlockReference* pBlockRef, double angle);
std::vector<AcGePoint3d> getPolylineVertices(AcDbPolyline* pPolyline);
double getPolylineDistance(AcDbPolyline* pPolyline1, AcDbPolyline* pPolyline2);
bool isPolylineClosed(const AcDbPolyline* pPolyline);
std::vector<AcGePoint3d> forcePolylineClockwise(std::vector<AcGePoint3d>& points);
bool isPolylineClockwise(const std::vector<AcGePoint3d>& points);
double categorizeAngle(double angle);
bool isInsideCorner(const std::vector<AcGePoint3d>& polylinePoints, size_t currentIndex, bool isClockwise);
void filterClosePoints(std::vector<AcGePoint3d>& vertices, double tolerance);
void adjustRotationForCorner(double& rotation, const std::vector<AcGePoint3d>& corners, size_t cornerNum);
// GeometryUtils.h (or wherever the function is declared)
bool isItInteger(double value, double tolerance = 1e-9);