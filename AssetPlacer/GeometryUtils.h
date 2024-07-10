#pragma once

#include "gepnt3d.h"
#include "dbents.h"
#include <vector>

double calculateAngle(const AcGeVector3d& v1, const AcGeVector3d& v2);
bool areAnglesEqual(double angle1, double angle2, double tolerance);
bool isCorner(double angle, double threshold = 45.0);
void detectVertices(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& vertices);
void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold, double tolerance);