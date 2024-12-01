















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


const double TOLERANCE = 0.19;


template <typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}


double calculateAngle(const AcGeVector3d& v1, const AcGeVector3d& v2) {
    
    AcGeVector3d normV1 = v1.normal();
    AcGeVector3d normV2 = v2.normal();

    double dotProduct = normV1.dotProduct(normV2);

    
    dotProduct = clamp(dotProduct, -1.0, 1.0);

    return acos(dotProduct);
}


AcGeVector3d calculateCardinalDirection(const AcGePoint3d& start, const AcGePoint3d& end, double tolerance) {
    double dx = end.x - start.x;
    double dy = end.y - start.y;

    
    if (std::abs(dx) < tolerance) dx = 0;  
    if (std::abs(dy) < tolerance) dy = 0;  

    
    if (dx != 0) dx /= std::abs(dx);  
    if (dy != 0) dy /= std::abs(dy);  

    return AcGeVector3d(dx, dy, 0); 
}


void adjustStartAndEndPoints(AcGePoint3d& start, AcGePoint3d& end, double tolerance) {
    
    start.x = std::round(start.x / tolerance) * tolerance;
    start.y = std::round(start.y / tolerance) * tolerance;

    end.x = std::round(end.x / tolerance) * tolerance;
    end.y = std::round(end.y / tolerance) * tolerance;

    
    
}


double normalizeAngle(double angle) {
    angle = fmod(angle, 2 * M_PI); 

    if (angle < 0) {
        angle += 2 * M_PI; 
    }

    return angle;
}

double snapToPredefinedValues(double distance) {
    
    std::vector<double> predefinedValues = {
        150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800, 850,
        900, 950, 1000, 1050, 1100, 1150, 1200, 1250, 1300, 1350, 1400, 1450, 1500,
        1550, 1600, 1650, 1700, 1750, 1800, 1850, 1900, 1950, 2000, 2050, 2100
    };

    
    auto closest = *std::min_element(predefinedValues.begin(), predefinedValues.end(),
        [distance](double a, double b) {
            return std::abs(a - distance) < std::abs(b - distance);
        });
	
	

    return closest;
}


double snapToExactAngle(double angle, double tolerance = 0.01) {
    const double snapAngles[] = { 0, M_PI_2, M_PI, 3 * M_PI_2, 2 * M_PI };

    
    angle = fmod(angle, 2 * M_PI);
    if (angle < 0) angle += 2 * M_PI;

    
    for (double snapAngle : snapAngles) {
        if (fabs(angle - snapAngle) < tolerance) {
            return snapAngle;
        }
    }

    
    if (fabs(angle - 2 * M_PI) < tolerance || fabs(angle) < tolerance) {
        return 0;
    }

    acutPrintf(_T("\nAngle %f not snapped to exact angle.\n"), angle);
    return angle;
}


bool determineIfInsideCorner(const std::vector<AcGePoint3d>& polylinePoints, size_t currentIndex, bool isClockwise) {
    
    size_t prevIndex = (currentIndex == 0) ? polylinePoints.size() - 1 : currentIndex - 1;
    size_t nextIndex = (currentIndex + 1) % polylinePoints.size();

    
    AcGeVector3d vec1 = polylinePoints[currentIndex] - polylinePoints[prevIndex];
    AcGeVector3d vec2 = polylinePoints[nextIndex] - polylinePoints[currentIndex];

    
    vec1.normalize();
    vec2.normalize();

    double crossProductZ = vec1.crossProduct(vec2).z;

    bool isConvex = crossProductZ > 0;

    
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


bool areAnglesEqual(double angle1, double angle2, double tolerance = 0.1) {
    
    angle1 = normalizeAngle(angle1);
    angle2 = normalizeAngle(angle2);
    return fabs(angle1 - angle2) < tolerance;
}


AcGeVector3d calculateDirection(const AcGePoint3d& start, const AcGePoint3d& end) {
    AcGeVector3d direction = end - start;
    direction.normalize();
    return direction;
}


double calculateAngleBetweenVectors(const AcGeVector3d& v1, const AcGeVector3d& v2) {
    double dotProduct = v1.dotProduct(v2);
    dotProduct = clamp(dotProduct, -1.0, 1.0); 
    return acos(dotProduct);
}


void detectVertices(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& vertices) {
    int numVerts = pPolyline->numVerts();
    for (int i = 0; i < numVerts; ++i) {
        AcGePoint3d point;
        pPolyline->getPointAt(i, point);
        vertices.push_back(point);
    }

    
    if (numVerts == 4) {
        vertices.push_back(vertices[0]);
    }
}


void logVector(const AcGeVector3d& vector, const char* vectorName) {
    
}


void logAngle(double angle, const char* message) {
    
}


void processPolyline(const AcDbPolyline* pPolyline, std::vector<AcGePoint3d>& corners, double angleThreshold, double tolerance) {
    
    std::vector<AcGePoint3d> vertices;
    detectVertices(pPolyline, vertices);

    filterClosePoints(vertices, tolerance);

    size_t numVerts = vertices.size();
    if (numVerts < 3) {
        
        return;
    }

    
    for (size_t i = 0; i < numVerts; ++i) {  
        AcGeVector3d currentDirection, nextDirection;

        
        currentDirection = vertices[(i + 1) % numVerts] - vertices[i];
        currentDirection.normalize();  

        
        if (i > 0) {
            nextDirection = vertices[i] - vertices[i - 1];
        }
        else {
            nextDirection = vertices[i] - vertices[numVerts - 1];  
        }
        nextDirection.normalize();  

        
        double angle = calculateAngle(currentDirection, nextDirection);

        
        if (areAnglesEqual(angle, angleThreshold, tolerance)) {
            corners.push_back(vertices[i]);
        }
        else {
            
            corners.push_back(vertices[i]);
        }
    }

    
    if (std::find(corners.begin(), corners.end(), vertices.back()) == corners.end()) {
        corners.push_back(vertices.back());
    }
}


void rotateAroundXAxis(AcDbBlockReference* pBlockRef, double angle) {
    
    AcGePoint3d position = pBlockRef->position();

    
    AcGeMatrix3d translateToOrigin;
    translateToOrigin.setToTranslation(-position.asVector());

    
    AcGeMatrix3d rotationMatrix;
    rotationMatrix.setToRotation(angle, AcGeVector3d::kXAxis, AcGePoint3d::kOrigin);

    
    AcGeMatrix3d translateBack;
    translateBack.setToTranslation(position.asVector());

    
    AcGeMatrix3d finalTransform = translateBack * rotationMatrix * translateToOrigin;

    
    pBlockRef->transformBy(finalTransform);
}


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


void rotateAroundZAxis(AcDbBlockReference* pBlockRef, double angle) {
    
    

    
    AcGePoint3d position = pBlockRef->position();

    
    AcGeMatrix3d translateToOrigin;
    translateToOrigin.setToTranslation(-position.asVector());

    
    AcGeMatrix3d rotationMatrix;
    rotationMatrix.setToRotation(angle, AcGeVector3d::kZAxis, AcGePoint3d::kOrigin);

    
    AcGeMatrix3d translateBack;
    translateBack.setToTranslation(position.asVector());

    
    AcGeMatrix3d finalTransform = translateBack * rotationMatrix * translateToOrigin;

    
    pBlockRef->transformBy(finalTransform);
}


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


double getPolylineDistance(AcDbPolyline* pPolyline1, AcDbPolyline* pPolyline2) {
    
    std::vector<AcGePoint3d> vertices1 = getPolylineVertices(pPolyline1);

    
    std::vector<AcGePoint3d> vertices2 = getPolylineVertices(pPolyline2);

    
    size_t minSize = std::min(vertices1.size(), vertices2.size());

    
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
            
            totalDistance = deltaX; 
        }
        else {
            
            double distance = sqrt(deltaX * deltaX + deltaY * deltaY);
            double closestValue = predeterminedValues[0];
            double minDifference = fabs(distance - closestValue);
            
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
    
	
    

    return totalDistance;
}


std::vector<AcGePoint3d> forcePolylineClockwise(std::vector<AcGePoint3d>& points) {
    if (points.empty()) return points;

    
    if (!isPolylineClockwise(points)) {
        
        std::reverse(points.begin(), points.end());
    }

    return points;
}


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
        
        return true;
    }
    else if (sum < -tolerance) {
        
        return false;
    }
    else {
        
        return false; 
    }
}


bool isInsideCorner(const std::vector<AcGePoint3d>& polylinePoints, size_t currentIndex, bool isClockwise) {
    
    size_t prevIndex = (currentIndex == 0) ? polylinePoints.size() - 1 : currentIndex - 1;
    size_t nextIndex = (currentIndex + 1) % polylinePoints.size();

    
    AcGeVector3d vec1 = polylinePoints[currentIndex] - polylinePoints[prevIndex];
    AcGeVector3d vec2 = polylinePoints[nextIndex] - polylinePoints[currentIndex];

    
    double crossProductZ = vec1.x * vec2.y - vec1.y * vec2.x;

    
    
        
        
        

   
   

    
    bool isInside = isClockwise ? (crossProductZ < 0) : (crossProductZ > 0);

    
    

    return isInside;
}


void filterClosePoints(std::vector<AcGePoint3d>& vertices, double tolerance) {
    std::vector<AcGePoint3d> filteredVertices;
    

    for (size_t i = 0; i < vertices.size(); ++i) {
        
        if (i == 0 || vertices[i].distanceTo(vertices[i - 1]) > tolerance) {
            filteredVertices.push_back(vertices[i]);
        }
        else {
            
                
        }
    }

    
    if (filteredVertices.size() > 1 && filteredVertices.back().distanceTo(filteredVertices.front()) <= tolerance) {
        
        filteredVertices.pop_back();
    }

    vertices.swap(filteredVertices);
}


void adjustRotationForCorner(double& rotation, const std::vector<AcGePoint3d>& corners, size_t cornerNum) {
    AcGeVector3d prevDirection = corners[cornerNum] - corners[cornerNum > 0 ? cornerNum - 1 : corners.size() - 1];
    AcGeVector3d nextDirection = corners[(cornerNum + 1) % corners.size()] - corners[cornerNum];
    double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;

    if (crossProductZ > 0) {
        rotation += M_PI_2;
    }
}


bool isItInteger(double value, double tolerance) {
    return std::abs(value - std::round(value)) < tolerance;
}


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
                    break; 
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