// Created by: Ani  (2024-05-31)
// Modified by: Ani (2024-06-01)
// TODO:
// WallAssetPlacer.cpp
// This file contains the implementation of the WallPlacer class.
// The WallPlacer class is used to place wall segments in BricsCAD.
/////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "WallAssetPlacer.h"
#include "SharedDefinations.h"
#include "GeometryUtils.h"
#include <vector>
#include <limits>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include "aced.h"
#include <cmath>
#include "acutads.h"
#include "acdocman.h"
#include "rxregsvc.h"
#include "geassign.h"
#include <string>
#include "DefineHeight.h"
#include "DefineScale.h"
#include <thread>
#include <chrono>
#include <map>
#include "Timber/TimberAssetCreator.h"

std::map<AcGePoint3d, std::vector<AcGePoint3d>, WallPlacer::Point3dComparator> WallPlacer::wallMap;
const int BATCH_SIZE = 1000; // Batch size for processing entities

const double TOLERANCE = 0.1; // Tolerance for comparing angles
double proximityTolerance = 1.0; // Adjust this tolerance as needed
std::vector<AcGePoint3d> processedCorners; // To keep track of processed corners

// Structure to hold panel information
struct Panel {
	int length;
	std::wstring id[3];
};

// detect T-joints with threshold function by user input IMPLEMENTED BUT NOT USED
void detectTJointsWithThreshold() {
	// Define the predefined threshold values
	std::vector<double> thresholds = { 150, 200, 250, 300, 350, 400 };

	ads_point pt1Array = { 0.0, 0.0, 0.0 };  // Initialize points
	ads_point pt2Array = { 0.0, 0.0, 0.0 };

	// Initialize a vector to store selected corner pairs
	std::vector<std::pair<AcGePoint3d, AcGePoint3d>> selectedTJointPairs;

	// Prompt the user to select corner points until they decide to stop
	while (true) {
		// Prompt the user to select the first corner point
		if (acedGetPoint(NULL, L"\nSelect the first corner: ", pt1Array) != RTNORM) {
			acutPrintf(L"\nError in selecting the first corner.\n");
			return;
		}

		// Prompt the user to select the second corner point
		if (acedGetPoint(NULL, L"\nSelect the second corner: ", pt2Array) != RTNORM) {
			acutPrintf(L"\nError in selecting the second corner.\n");
			return;
		}

		// Convert to AcGePoint3d for easier distance calculation
		AcGePoint3d point1(pt1Array[0], pt1Array[1], pt1Array[2]);
		AcGePoint3d point2(pt2Array[0], pt2Array[1], pt2Array[2]);

		// Store the selected pair of points
		selectedTJointPairs.push_back(std::make_pair(point1, point2));

		// Get user input for if they want to select another T-joint
		ACHAR userChoice[256];
		if (acedGetString(Adesk::kFalse, _T("\nDo you want to select another T-joint (Y/N)? Default: Y "), userChoice) != RTNORM) {
			acutPrintf(L"\nOperation canceled.");
			return;
		}

		// If user enters 'N' or 'n', break the loop
		if (wcscmp(userChoice, _T("N")) || wcscmp(userChoice, _T("N"))) {
			break;
		}
	}

	// Iterate through all selected T-joint pairs
	for (const auto& pair : selectedTJointPairs) {
		AcGePoint3d point1 = pair.first;
		AcGePoint3d point2 = pair.second;

		// Calculate the distance between the points
		double distance = point1.distanceTo(point2);
		acutPrintf(L"\nDistance between the points: %f\n", distance);

		// Check if the distance matches any predefined thresholds
		bool isTJointDetected = false;
		for (double threshold : thresholds) {
			if (std::fabs(distance - threshold) <= 5.0) {  // Allow a small tolerance of 5 units
				acutPrintf(L"\nT-joint detected between the points, matching threshold: %f\n", threshold);
				isTJointDetected = true;
				break;
			}
		}

		if (!isTJointDetected) {
			acutPrintf(L"\nThe distance between the points does not match any of the predefined thresholds.\n");
		}
	}
}

// Struct to store polyline's ID and its corners IN USE
struct PolylineCorners {
	AcDbObjectId polylineId;          // Unique identifier for the polyline
	std::vector<AcGePoint3d> corners; // List of corners of the polyline

	PolylineCorners(AcDbObjectId id) : polylineId(id) {}
};

bool isInteger(double value, double tolerance = 1e-9) {
	return std::abs(value - std::round(value)) < tolerance;
}

bool isCloseToProcessedCorners(const AcGePoint3d& point, const std::vector<AcGePoint3d>& processedCorners, double tolerance) {
	for (const auto& processedCorner : processedCorners) {
		if (point.distanceTo(processedCorner) < tolerance) {
			return true; // Point is too close to a processed corner
		}
	}
	return false;
}

bool isCornerConcave(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
	// Calculate cross product to determine corner type
	AcGeVector3d v1 = current - prev;
	AcGeVector3d v2 = next - current;
	double cross = v1.x * v2.y - v1.y * v2.x;

	double tolerance = 1e-6;

	bool isConcave = cross < -tolerance;

	//// Debugging information
	//acutPrintf(_T("\nChecking corner at (%f, %f): "), current.x, current.y);
	//acutPrintf(_T("Previous Point: (%f, %f), Next Point: (%f, %f)"), prev.x, prev.y, next.x, next.y);
	//acutPrintf(_T("Cross Product: %f, Identified as Concave: %d"), cross, isConcave);

	return isConcave;
}

bool isCornerConvex(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
	AcGeVector3d v1 = current - prev;
	AcGeVector3d v2 = next - current;
	double cross = v1.x * v2.y - v1.y * v2.x;

	// Tolerance to handle floating-point errors
	double tolerance = 1e-6;

	bool isConvex = cross > tolerance;

	//acutPrintf(_T("\nChecking corner at (%f, %f): Previous Point: (%f, %f), Next Point: (%f, %f)"),
	//    current.x, current.y, prev.x, prev.y, next.x, next.y);
	//acutPrintf(_T("Cross Product: %f, Identified as Convex: %d"), cross, isConvex);

	return isConvex;
}

// Function to compute if the corner is turning clockwise or counterclockwise
bool isClockwise(const AcGePoint3d& p0, const AcGePoint3d& p1, const AcGePoint3d& p2) {
	// Compute the vectors for the edges
	AcGeVector3d v1 = p1 - p0;  // Vector from p0 to p1
	AcGeVector3d v2 = p2 - p1;  // Vector from p1 to p2

	// Compute the cross product
	AcGeVector3d crossProduct = v1.crossProduct(v2);

	// Determine the direction of the turn
	// If cross product z-component is positive, the turn is clockwise
	// If cross product z-component is negative, the turn is counterclockwise
	return crossProduct.z < 0;
}

//detectPolylines function to detect all closed polylines in the drawing IN USE
void detectClosedPolylinesAndCorners(std::vector<PolylineCorners>& polylineCornerGroups) {
	polylineCornerGroups.clear();

	// Open the model space for read
	AcDbBlockTable* pBlockTable = nullptr;
	acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead);

	// Retrieve the model space block record ID
	AcDbObjectId blockId;
	pBlockTable->getAt(ACDB_MODEL_SPACE, blockId);
	pBlockTable->close();
	// Open the model space block record for reading
	AcDbBlockTableRecord* pBlockTableRecord = nullptr;
	acdbOpenObject(pBlockTableRecord, blockId, AcDb::kForRead);

	// Iterate through all the entities in the model space
	AcDbEntity* pEntity = nullptr;
	AcDbBlockTableRecordIterator* pIterator = nullptr;
	pBlockTableRecord->newIterator(pIterator);
	pBlockTableRecord->close();

	for (pIterator->start(); !pIterator->done(); pIterator->step()) {
		pIterator->getEntity(pEntity, AcDb::kForRead);

		if (pEntity && pEntity->isKindOf(AcDbPolyline::desc())) {
			AcDbPolyline* pPolyline = AcDbPolyline::cast(pEntity);
			if (pPolyline && pPolyline->isClosed()) {
				std::vector<AcGePoint3d> corners;
				int numVerts = pPolyline->numVerts();
				for (int i = 0; i < numVerts; ++i) {
					AcGePoint3d corner;
					if (pPolyline->getPointAt(i, corner) == Acad::eOk) {
						corners.push_back(corner);
					}
				}
				PolylineCorners polylineGroup(pEntity->objectId());
				polylineGroup.corners = corners;
				polylineCornerGroups.push_back(polylineGroup);
			}
		}
		//close the entity and model space

		pEntity->close();

	}

	delete pIterator;

	// Output the detected corners for debugging or verification
	//acutPrintf(L"\nDetected closed polyline corners grouped by polyline:\n");
	//for (size_t i = 0; i < polylineCornerGroups.size(); ++i) {
	//	acutPrintf(L"Polyline ID: %s\n", polylineCornerGroups[i].polylineId.handle());
	//	for (size_t j = 0; j < polylineCornerGroups[i].corners.size(); ++j) {
	//		acutPrintf(L"Corner %zu at: %.2f, %.2f\n", j + 1, polylineCornerGroups[i].corners[j].x, polylineCornerGroups[i].corners[j].y);
	//	}
	//}
}

// Function to calculate the signed area of a polyline NOT IN USE
double calculateSignedArea(const std::vector<AcGePoint3d>& vertices) {
	double area = 0.0;
	size_t n = vertices.size();
	for (size_t i = 0; i < n - 1; ++i) {
		area += (vertices[i].x * vertices[i + 1].y - vertices[i + 1].x * vertices[i].y);
	}
	area += (vertices[n - 1].x * vertices[0].y - vertices[0].x * vertices[n - 1].y); // Close the loop
	return area / 2.0;
}

// Function to determine if a corner is convex or concave IN USE
double calculatePolygonArea(const std::vector<AcGePoint3d>& polygon) {
	double area = 0.0;
	for (size_t i = 0; i < polygon.size(); ++i) {
		const AcGePoint3d& p1 = polygon[i];
		const AcGePoint3d& p2 = polygon[(i + 1) % polygon.size()];
		area += (p1.x * p2.y - p2.x * p1.y);  // Shoelace formula
	}
	return fabs(area) / 2.0;
}

// Function to determine if a corner is convex or concave IN USE
bool isPointInsidePolygon(const AcGePoint3d& point, const std::vector<AcGePoint3d>& polygon) {
	int crossings = 0;
	for (size_t i = 0; i < polygon.size(); ++i) {
		const AcGePoint3d& p1 = polygon[i];
		const AcGePoint3d& p2 = polygon[(i + 1) % polygon.size()];

		if (((p1.y > point.y) != (p2.y > point.y)) &&
			(point.x < (p2.x - p1.x) * (point.y - p1.y) / (p2.y - p1.y) + p1.x)) {
			crossings++;
		}
	}
	return (crossings % 2) == 1;  // Odd number of crossings means inside
}


double crossProduct(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
	return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Function to get two points from the user and calculate the distance
double getDistanceFromUser() {
	ads_point firstPoint, secondPoint;
	double distance = 0.0;

	// Step 1: Prompt the user to select the first point
	if (acedGetPoint(NULL, _T("Select the first point: "), firstPoint) != RTNORM) {
		acutPrintf(_T("\nError: Failed to get the first point.\n"));
		return -1;  // Return a negative value to indicate error
	}

	// Step 2: Prompt the user to select the second point
	if (acedGetPoint(NULL, _T("Select the second point: "), secondPoint) != RTNORM) {
		acutPrintf(_T("\nError: Failed to get the second point.\n"));
		return -1;  // Return a negative value to indicate error
	}

	// Step 3: Calculate DeltaX and DeltaY
	double deltaX = firstPoint[X] - secondPoint[X];
	double deltaY = secondPoint[Y] - firstPoint[Y];

	// Step 4: Define a small tolerance for comparison
	const double tolerance = 1.0;

	// Step 5: Calculate the distance based on the comparison of DeltaX and DeltaY
	if (firstPoint[X] != 0.0 && firstPoint[Y] != 0.0 && secondPoint[X] != 0.0 && secondPoint[Y] != 0.0) {
		// Check if DeltaX and DeltaY are equal within the tolerance
		if (std::fabs(deltaX - deltaY) <= tolerance) {
			// Snap distance to predefined values
			distance = snapToPredefinedValues(deltaX);
		}
		else {
			// Calculate the Euclidean distance between the points
			distance = snapToPredefinedValues(std::sqrt(deltaX * deltaX + deltaY * deltaY));
		}
	}
	else {
		acutPrintf(_T("\nPoints were not selected properly. Skipping distance calculation.\n"));
		distance = 200;  // Default fallback distance
	}

	// Step 6: Return the calculated distance
	acutPrintf(_T("\nDistance between the points: %.2f\n"), distance);
	return distance;
}

bool directionOfDrawing(std::vector<AcGePoint3d>& points) {
	// Ensure the shape is closed
	if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
		points.push_back(points.front());
	}

	double totalTurns = 0.0;

	// Calculate the total turns using cross products
	for (size_t i = 1; i < points.size() - 1; ++i) {
		totalTurns += crossProduct(points[i - 1], points[i], points[i + 1]);
	}

	// If totalTurns is negative, the shape is drawn clockwise
	if (totalTurns < 0) {
		return true;  // Clockwise
	}
	// If totalTurns is positive, the shape is drawn counterclockwise
	else if (totalTurns > 0) {
		return false; // Counterclockwise
	}
	// Handle the case where totalTurns is zero (indicating an undefined direction)
	else {
		acutPrintf(_T("Warning: The shape does not have a defined direction. Defaulting to clockwise.\n"));
		return true;  // Default to clockwise if direction cannot be determined
	}
}

// Function to classify loops as outer or inner based on containment IN USE
void classifyLoopsMultiCheck(const std::vector<std::vector<AcGePoint3d>>& allPolylines,
	std::vector<std::vector<AcGePoint3d>>& outerLoops,
	std::vector<std::vector<AcGePoint3d>>& innerLoops, std::vector<bool>& loopIsClockwise) {
	std::vector<bool> isProcessed(allPolylines.size(), false);

	// Step 1: Identify the largest area loop as the outermost loop
	size_t outermostIndex = -1;
	double maxArea = -1.0;

	for (size_t i = 0; i < allPolylines.size(); ++i) {
		double area = calculatePolygonArea(allPolylines[i]);
		if (area > maxArea) {
			maxArea = area;
			outermostIndex = i;
		}
	}

	if (outermostIndex != -1) {
		outerLoops.push_back(allPolylines[outermostIndex]);
		isProcessed[outermostIndex] = true;
	}

	// Step 2: Iteratively classify remaining loops
	for (size_t i = 0; i < allPolylines.size(); ++i) {
		if (isProcessed[i]) continue;
		bool isClockwise = directionOfDrawing(const_cast<std::vector<AcGePoint3d>&>(allPolylines[i]));
		loopIsClockwise.push_back(isClockwise); // Store the direction for each polyline

		bool isOuter = false;
		for (size_t j = 0; j < outerLoops.size(); ++j) {
			if (isPointInsidePolygon(allPolylines[i][0], outerLoops[j])) {
				// Check containment within current outer loop
				isOuter = true;

				// Check if this loop contains another loop
				bool containsInner = false;
				for (size_t k = 0; k < allPolylines.size(); ++k) {
					if (k == i || isProcessed[k]) continue;

					if (isPointInsidePolygon(allPolylines[k][0], allPolylines[i])) {
						containsInner = true;
						break;
					}
				}

				if (containsInner) {
					outerLoops.push_back(allPolylines[i]);
				}
				else {
					innerLoops.push_back(allPolylines[i]);
				}

				isProcessed[i] = true;
				break;
			}
		}

		// If it's not inside any outer loop, it could still be an outer loop
		if (!isOuter) {
			outerLoops.push_back(allPolylines[i]);
			isProcessed[i] = true;
		}
	}
}

int calculatedAdjustment(int distanceBetweenPolylines) {
	int adjustment;
	if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
		adjustment = 450;
	}
	else if (distanceBetweenPolylines == 250) {
		adjustment = 600;
	}
	else if (distanceBetweenPolylines == 300) {
		adjustment = 650;
	}
	else if (distanceBetweenPolylines == 350) {
		adjustment = 700;
	}
	else if (distanceBetweenPolylines == 400) {
		adjustment = 750;
	}
	else if (distanceBetweenPolylines == 450) {
		adjustment = 800;
	}
	else if (distanceBetweenPolylines == 500) {
		adjustment = 850;
	}
	else if (distanceBetweenPolylines == 550) {
		adjustment = 900;
	}
	else if (distanceBetweenPolylines == 600) {
		adjustment = 950;
	}
	else if (distanceBetweenPolylines == 650) {
		adjustment = 1000;
	}
	else if (distanceBetweenPolylines == 700) {
		adjustment = 1050;
	}
	else if (distanceBetweenPolylines == 750) {
		adjustment = 1100;
	}
	else if (distanceBetweenPolylines == 800) {
		adjustment = 1150;
	}
	else if (distanceBetweenPolylines == 850) {
		adjustment = 1200;
	}
	else if (distanceBetweenPolylines == 900) {
		adjustment = 1250;
	}
	else if (distanceBetweenPolylines == 950) {
		adjustment = 1300;
	}
	else if (distanceBetweenPolylines == 1000) {
		adjustment = 1350;
	}
	else if (distanceBetweenPolylines == 1050) {
		adjustment = 1400;
	}
	else if (distanceBetweenPolylines == 1100) {
		adjustment = 1450;
	}
	else if (distanceBetweenPolylines == 1150) {
		adjustment = 1500;
	}
	else if (distanceBetweenPolylines == 1200) {
		adjustment = 1550;
	}
	else if (distanceBetweenPolylines == 1250) {
		adjustment = 1600;
	}
	else if (distanceBetweenPolylines == 1300) {
		adjustment = 1650;
	}
	else if (distanceBetweenPolylines == 1350) {
		adjustment = 1700;
	}
	else if (distanceBetweenPolylines == 1400) {
		adjustment = 1750;
	}
	else if (distanceBetweenPolylines == 1450) {
		adjustment = 1800;
	}
	else if (distanceBetweenPolylines == 1500) {
		adjustment = 1850;
	}
	else if (distanceBetweenPolylines == 1550) {
		adjustment = 1900;
	}
	else if (distanceBetweenPolylines == 1600) {
		adjustment = 1950;
	}
	else if (distanceBetweenPolylines == 1650) {
		adjustment = 2000;
	}
	else if (distanceBetweenPolylines == 1700) {
		adjustment = 2050;
	}
	else if (distanceBetweenPolylines == 1750) {
		adjustment = 2100;
	}
	else if (distanceBetweenPolylines == 1800) {
		adjustment = 2150;
	}
	else if (distanceBetweenPolylines == 1850) {
		adjustment = 2200;
	}
	else if (distanceBetweenPolylines == 1900) {
		adjustment = 2250;
	}
	else if (distanceBetweenPolylines == 1950) {
		adjustment = 2300;
	}
	else if (distanceBetweenPolylines == 2000) {
		adjustment = 2350;
	}
	else if (distanceBetweenPolylines == 2050) {
		adjustment = 2400;
	}
	else if (distanceBetweenPolylines == 2100) {
		adjustment = 2450;
	}
	else {
		adjustment = 150; // Default case for any unexpected distance value
	}
	return adjustment;
}

// Calculate the minimum distance between the two polylines (outer and inner)
double calculateMinimumDistance(const std::vector<AcGePoint3d>& outer, const std::vector<AcGePoint3d>& inner) {
	double minDistance = std::numeric_limits<double>::max();

	// Iterate through each point in the outer polyline and find the minimum distance to any point in the inner polyline
	for (const auto& outerPoint : outer) {
		for (const auto& innerPoint : inner) {
			double distance = outerPoint.distanceTo(innerPoint);
			minDistance = std::min(minDistance, distance);
		}
	}

	return minDistance;
}

// Function to calculate wall thickness
double calculateWallThickness(const std::vector<std::vector<AcGePoint3d>>& outerLoops,
	const std::vector<std::vector<AcGePoint3d>>& innerLoops) {
	double minThickness = std::numeric_limits<double>::max();  // Initialize to a large value

	// For each outer loop, find the nearest inner loop and calculate the distance
	for (const auto& outer : outerLoops) {
		for (const auto& inner : innerLoops) {
			double distance = calculateMinimumDistance(outer, inner);
			minThickness = std::min(minThickness, distance);  // Keep the minimum distance found
		}
	}

	return minThickness;
}

// Function to calculate the distance between two points
double calculateDistance(const AcGePoint3d& point1, const AcGePoint3d& point2) {
	return (point1 - point2).length();
}

// Function to check if two points are aligned along the same axis (X or Y) and are at least 150 units apart
bool areCornersAligned(const AcGePoint3d& corner1, const AcGePoint3d& corner2, double minDistance = 150.0) {
	// Check if they are aligned on the same axis (X or Y)
	bool alignedOnX = std::abs(corner1.y - corner2.y) < 1e-6; // Same Y-axis
	bool alignedOnY = std::abs(corner1.x - corner2.x) < 1e-6; // Same X-axis

	if (alignedOnX || alignedOnY) {
		// Check if they are at least 150 units apart
		double distance = alignedOnX ? std::abs(corner1.x - corner2.x) : std::abs(corner1.y - corner2.y);
		return distance >= minDistance;
	}

	return false;
}

// Function to check if the polyline is perpendicular to the line formed by the two corners
bool isPolylinePerpendicularToCorners(const AcGePoint3d& corner1, const AcGePoint3d& corner2, const AcGePoint3d& polylineStart, const AcGePoint3d& polylineEnd, double minLength = 150.0) {
	// Determine the direction vector of the corners (from corner1 to corner2)
	AcGeVector3d cornerDirection = corner2 - corner1;

	// Calculate the vector of the polyline segment
	AcGeVector3d polylineVector = polylineEnd - polylineStart;

	// Check if the polyline is perpendicular to the line formed by the corners
	double dotProduct = cornerDirection.dotProduct(polylineVector);
	bool isPerpendicular = std::abs(dotProduct) < 1e-6; // Should be zero for perpendicular vectors

	// Check if the polyline is long enough
	double polylineLength = polylineVector.length();
	bool isLongEnough = polylineLength >= minLength;

	return isPerpendicular && isLongEnough;
}

// Function to check if the T-joint is valid based on the above criteria
bool isValidTJoint(const AcGePoint3d& corner1, const AcGePoint3d& corner2, const AcGePoint3d& polylineStart, const AcGePoint3d& polylineEnd) {
	// Ensure corners are aligned and at least 150 units apart
	if (!areCornersAligned(corner1, corner2)) {
		return false; // Corners are not aligned or too close
	}

	// Ensure the polyline is perpendicular to the corners and sufficiently long
	if (!isPolylinePerpendicularToCorners(corner1, corner2, polylineStart, polylineEnd)) {
		return false; // Polyline is not perpendicular or too short
	}

	// If both conditions are met, it's a valid T-joint
	return true;
}

// Struct to store T-joint information IN USE
struct TJoint {
	AcGePoint3d position;      // The position of the corner point forming the T-joint
	AcGePoint3d segmentStart;  // The starting point of the segment in the other loop
	AcGePoint3d segmentEnd;    // The ending point of the segment in the other loop

	TJoint() = default;

	TJoint(const AcGePoint3d& pos, const AcGePoint3d& start, const AcGePoint3d& end)
		: position(pos), segmentStart(start), segmentEnd(end) {}
};

// Function to check if a point is near a segment IN USE
bool isPointNearSegment(const AcGePoint3d& point,
	const AcGePoint3d& start,
	const AcGePoint3d& end,
	double threshold) {
	// Create a vector for the segment
	AcGeVector3d segmentVector = end - start;
	AcGeVector3d startToPoint = point - start;

	double lengthSq = segmentVector.length() * segmentVector.length();

	// Project the point onto the segment vector
	double projection = startToPoint.dotProduct(segmentVector) / lengthSq;


	// Clamp the projection between 0 and 1 (inside the segment)
	projection = std::max(0.0, std::min(1.0, projection));

	// Find the closest point on the segment
	AcGePoint3d closestPoint = start + segmentVector * projection;

	// Calculate distance from the point to the closest point
	double distance = (point - closestPoint).length();

	return distance <= threshold;
}

// Function to detect T-joints between loops IN USE
void detectTJoints(const std::vector<std::vector<AcGePoint3d>>& allLoops,
	std::vector<TJoint>& detectedTJoints,
	double threshold = 150.0) {
	// Reserve space for detected T-joints based on an estimate
	detectedTJoints.reserve(allLoops.size() * 10); // Adjust estimate as needed

	// Iterate through each loop
	for (size_t i = 0; i < allLoops.size(); ++i) {
		const auto& currentLoop = allLoops[i];

		// Check every corner in the current loop
		for (const AcGePoint3d& corner : currentLoop) {
			// Compare with segments of other loops
			for (size_t j = 0; j < allLoops.size(); ++j) {
				if (i == j) continue; // Skip comparison within the same loop

				const auto& comparisonLoop = allLoops[j];
				size_t segmentCount = comparisonLoop.size();

				// Iterate through segments in the comparison loop
				for (size_t segIdx = 0; segIdx < segmentCount; ++segIdx) {
					const AcGePoint3d& start = comparisonLoop[segIdx];
					const AcGePoint3d& end = comparisonLoop[(segIdx + 1) % segmentCount];

					// Check if the corner is near the current segment
					if (isPointNearSegment(corner, start, end, threshold)) {
						// Add a new T-joint if detected
						detectedTJoints.emplace_back(corner, start, end);
					}
				}
			}
		}
	}
}

void snapToGrid(AcGePoint3d& point, double gridSize = 1.0) {
	point.x = std::round(point.x / gridSize) * gridSize;
	point.y = std::round(point.y / gridSize) * gridSize;
	point.z = std::round(point.z / gridSize) * gridSize;

	// Debug output for verification
	acutPrintf(L"\nSnapped Point: (%f, %f, %f)", point.x, point.y, point.z);
}


// Load asset
AcDbObjectId WallPlacer::loadAsset(const wchar_t* blockName) {
	// acutPrintf(_T("\nLoading asset: %s"), blockName);
	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
	if (!pDb) return AcDbObjectId::kNull;

	AcDbBlockTable* pBlockTable;
	if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) return AcDbObjectId::kNull;

	AcDbObjectId blockId;
	if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
		pBlockTable->close();
		return AcDbObjectId::kNull;
	}

	pBlockTable->close();
	// acutPrintf(_T(" Loaded block: %s"), blockName);
	return blockId;
}

// Function to adjust the start and end points based on wall thickness
void adjustPointsForWallThickness(double distanceBetweenPolylines, bool isOuter, bool isConvex, bool isAdjacentConvex,
	AcGePoint3d& start, AcGePoint3d& end, int direction) {

	int adjustment = 0;

	// Adjust start and end points based on the calculated or user-provided thickness
	if (isOuter) {
		isConvex = !isConvex;
		isAdjacentConvex = !isAdjacentConvex;
	}

	if (!isConvex) {
		if (distanceBetweenPolylines == 150) {
			start.x += direction * 300;
		}
		else {
			start.x += direction * 250;
		}
	}
	else {
		// Using the provided table for outer loop adjustments

		if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
			adjustment = 550;
		}
		else if (distanceBetweenPolylines == 250) {
			adjustment = 600;
		}
		else if (distanceBetweenPolylines == 300) {
			adjustment = 650;
		}
		else if (distanceBetweenPolylines == 350) {
			adjustment = 700;
		}
		else if (distanceBetweenPolylines == 400) {
			adjustment = 750;
		}
		else if (distanceBetweenPolylines == 450) {
			adjustment = 800;
		}
		else if (distanceBetweenPolylines == 500) {
			adjustment = 850;
		}
		else if (distanceBetweenPolylines == 550) {
			adjustment = 900;
		}
		else if (distanceBetweenPolylines == 600) {
			adjustment = 950;
		}
		else if (distanceBetweenPolylines == 650) {
			adjustment = 1000;
		}
		else if (distanceBetweenPolylines == 700) {
			adjustment = 1050;
		}
		else if (distanceBetweenPolylines == 750) {
			adjustment = 1100;
		}
		else if (distanceBetweenPolylines == 800) {
			adjustment = 1150;
		}
		else if (distanceBetweenPolylines == 850) {
			adjustment = 1200;
		}
		else if (distanceBetweenPolylines == 900) {
			adjustment = 1250;
		}
		else if (distanceBetweenPolylines == 950) {
			adjustment = 1300;
		}
		else if (distanceBetweenPolylines == 1000) {
			adjustment = 1350;
		}
		else if (distanceBetweenPolylines == 1050) {
			adjustment = 1400;
		}
		else if (distanceBetweenPolylines == 1100) {
			adjustment = 1450;
		}
		else if (distanceBetweenPolylines == 1150) {
			adjustment = 1500;
		}
		else if (distanceBetweenPolylines == 1200) {
			adjustment = 1550;
		}
		else if (distanceBetweenPolylines == 1250) {
			adjustment = 1600;
		}
		else if (distanceBetweenPolylines == 1300) {
			adjustment = 1650;
		}
		else if (distanceBetweenPolylines == 1350) {
			adjustment = 1700;
		}
		else if (distanceBetweenPolylines == 1400) {
			adjustment = 1750;
		}
		else if (distanceBetweenPolylines == 1450) {
			adjustment = 1800;
		}
		else if (distanceBetweenPolylines == 1500) {
			adjustment = 1850;
		}
		else if (distanceBetweenPolylines == 1550) {
			adjustment = 1900;
		}
		else if (distanceBetweenPolylines == 1600) {
			adjustment = 1950;
		}
		else if (distanceBetweenPolylines == 1650) {
			adjustment = 2000;
		}
		else if (distanceBetweenPolylines == 1700) {
			adjustment = 2050;
		}
		else if (distanceBetweenPolylines == 1750) {
			adjustment = 2100;
		}
		else {
			adjustment = 150; // Default case for any unexpected distance value
		}

		adjustment -= 100;

		start.x += direction * adjustment;
	}

	if (!isAdjacentConvex) {
		if (distanceBetweenPolylines == 150) {
			end.x -= direction * 300;
		}
		else {
			end.x -= direction * 250;
		}
	}
	else {
		if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
			adjustment = 550;
		}
		else if (distanceBetweenPolylines == 250) {
			adjustment = 600;
		}
		else if (distanceBetweenPolylines == 300) {
			adjustment = 650;
		}
		else if (distanceBetweenPolylines == 350) {
			adjustment = 700;
		}
		else if (distanceBetweenPolylines == 400) {
			adjustment = 750;
		}
		else if (distanceBetweenPolylines == 450) {
			adjustment = 800;
		}
		else if (distanceBetweenPolylines == 500) {
			adjustment = 850;
		}
		else if (distanceBetweenPolylines == 550) {
			adjustment = 900;
		}
		else if (distanceBetweenPolylines == 600) {
			adjustment = 950;
		}
		else if (distanceBetweenPolylines == 650) {
			adjustment = 1000;
		}
		else if (distanceBetweenPolylines == 700) {
			adjustment = 1050;
		}
		else if (distanceBetweenPolylines == 750) {
			adjustment = 1100;
		}
		else if (distanceBetweenPolylines == 800) {
			adjustment = 1150;
		}
		else if (distanceBetweenPolylines == 850) {
			adjustment = 1200;
		}
		else if (distanceBetweenPolylines == 900) {
			adjustment = 1250;
		}
		else if (distanceBetweenPolylines == 950) {
			adjustment = 1300;
		}
		else if (distanceBetweenPolylines == 1000) {
			adjustment = 1350;
		}
		else if (distanceBetweenPolylines == 1050) {
			adjustment = 1400;
		}
		else if (distanceBetweenPolylines == 1100) {
			adjustment = 1450;
		}
		else if (distanceBetweenPolylines == 1150) {
			adjustment = 1500;
		}
		else if (distanceBetweenPolylines == 1200) {
			adjustment = 1550;
		}
		else if (distanceBetweenPolylines == 1250) {
			adjustment = 1600;
		}
		else if (distanceBetweenPolylines == 1300) {
			adjustment = 1650;
		}
		else if (distanceBetweenPolylines == 1350) {
			adjustment = 1700;
		}
		else if (distanceBetweenPolylines == 1400) {
			adjustment = 1750;
		}
		else if (distanceBetweenPolylines == 1450) {
			adjustment = 1800;
		}
		else if (distanceBetweenPolylines == 1500) {
			adjustment = 1850;
		}
		else if (distanceBetweenPolylines == 1550) {
			adjustment = 1900;
		}
		else if (distanceBetweenPolylines == 1600) {
			adjustment = 1950;
		}
		else if (distanceBetweenPolylines == 1650) {
			adjustment = 2000;
		}
		else if (distanceBetweenPolylines == 1700) {
			adjustment = 2050;
		}
		else if (distanceBetweenPolylines == 1750) {
			adjustment = 2100;
		}
		else {
			adjustment = 150; // Default case for any unexpected distance value
		}

		adjustment -= 100;

		end.x -= direction * adjustment;
	}
}

//void WallPlacer::placeWalls()
//{
//	// Initialize variables to store detected corners and T-joints
//	std::vector<PolylineCorners> polylineCornerGroups;
//	std::vector<TJoint> detectedTJoints;
//	std::vector<std::vector<AcGePoint3d>> outerLoops;
//	std::vector<std::vector<AcGePoint3d>> innerLoops;
//	std::vector<std::vector<AcGePoint3d>> allPolylines;
//	struct WallPanel {
//		AcGePoint3d position;
//		AcDbObjectId assetId;
//		double rotation;
//		int length;
//		int height;
//		int loopIndex;
//		bool isOuterLoop;
//
//	};
//
//	std::vector<WallPanel> wallPanels;
//	std::vector<std::pair<AcGePoint3d, AcGePoint3d>> Rawsegments;
//	std::vector<std::pair<AcGePoint3d, AcGePoint3d>> segments;
//
//	// Print the detected corners
//	// Detect closed polylines and group their corners
//	detectClosedPolylinesAndCorners(polylineCornerGroups);
//
//	// Check if polylines were detected correctly
//	if (polylineCornerGroups.empty()) {
//		acutPrintf(L"\nNo closed polylines detected.\n");
//		return;
//	}
//
//	// Initialize corners and get them from the polylineCornerGroups
//	std::vector<AcGePoint3d> corners;
//	for (const auto& polylineGroup : polylineCornerGroups) {
//		corners.insert(corners.end(), polylineGroup.corners.begin(), polylineGroup.corners.end());
//	}
//
//	for (const auto& polylineGroup : polylineCornerGroups) {
//		allPolylines.push_back(polylineGroup.corners);
//	}
//
//	classifyLoopsMultiCheck(allPolylines, outerLoops, innerLoops);
//	//print the classified loops
//	acutPrintf(L"\nOuter loops:\n");
//	for (size_t i = 0; i < outerLoops.size(); ++i) {
//		acutPrintf(L"Outer loop %zu:\n", i + 1);
//		for (size_t j = 0; j < outerLoops[i].size(); ++j) {
//			acutPrintf(L"Corner %zu at: %.2f, %.2f\n", j + 1, outerLoops[i][j].x, outerLoops[i][j].y);
//		}
//	}
//
//	acutPrintf(L"\nInner loops:\n");
//	for (size_t i = 0; i < innerLoops.size(); ++i) {
//		acutPrintf(L"Inner loop %zu:\n", i + 1);
//		for (size_t j = 0; j < innerLoops[i].size(); ++j) {
//			acutPrintf(L"Corner %zu at: %.2f, %.2f\n", j + 1, innerLoops[i][j].x, innerLoops[i][j].y);
//		}
//	}
//
//	bool isOuter = false;
//
//	// Convert std::vector<AcGePoint3d> to AcDbPolyline
//	AcDbPolyline* currentPolyline = nullptr;  // Declare currentPolyline
//	for (size_t i = 0; i < outerLoops.size(); ++i) {
//		if (outerLoops[i].size() >= 2) {
//			// Create an AcDbPolyline from the vector of AcGePoint3d
//			currentPolyline = new AcDbPolyline();
//
//			// Add points to the AcDbPolyline
//			for (size_t j = 0; j < outerLoops[i].size(); ++j) {
//				currentPolyline->addVertexAt(j, AcGePoint2d(outerLoops[i][j].x, outerLoops[i][j].y), 0, 0, 0);
//			}
//
//			// Convert currentPolyline to a vector of AcGePoint3d for comparison
//			std::vector<AcGePoint3d> currentPoints;
//			for (size_t j = 0; j < currentPolyline->numVerts(); ++j) {
//				AcGePoint3d point;
//				currentPolyline->getPointAt(j, point);
//				currentPoints.push_back(point);
//			}
//
//			// Compare the current polyline points with the outerLoops[i] points
//			if (currentPoints == outerLoops[i]) {
//				isOuter = true;
//				break;
//			}
//		}
//	}
//
//	// T-Joint Detection
//	detectTJoints(allPolylines, detectedTJoints);
//
//	// Set the wall height and initialize panel configurations
//	int wallHeight = globalVarHeight;
//	int currentHeight = 0;
//	int panelHeights[] = { 1350, 1200, 600 };
//
//	std::vector<Panel> panelSizes = {
//		{600, {L"128282X", L"136096X", L"129839X"}},
//		{450, {L"128283X", L"Null", L"129840X"}},
//		{300, {L"128284X", L"Null", L"129841X"}},
//		{150, {L"128285X", L"Null", L"129842X"}},
//		{100, {L"128292X", L"Null", L"129884X"}},
//		{50, {L"128287X", L"Null", L"129879X"}}
//	};
//	int closeLoopCounter = 0;
//	int outerLoopIndexValue = 0;
//
//	// Third Pass: Save all positions, asset IDs, and rotations
//	int loopIndex = 0;
//	int loopIndexLastPanel = 0;
//	closeLoopCounter = -1;
//	double totalPanelsPlaced = 0;
//	std::vector<int> cornerLocations;
//
//	int distanceBetweenPolylines = getDistanceFromUser();
//	//acutPrintf(L"\nDistance between polylines: %.2f", distanceBetweenPolylines);
//
//	for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
//		//acutPrintf(L"\nProcessing corner %zu", cornerNum + 1);
//		AcGePoint3d Rawstart = corners[cornerNum];
//		AcGePoint3d Rawend = corners[(cornerNum + 1) % corners.size()];
//		snapToGrid(Rawstart);
//		snapToGrid(Rawend);
//
//		//print Raw start and end points
//		acutPrintf(L"\nRaw Start: %.2f, %.2f, End: %.2f, %.2f", Rawstart.x, Rawstart.y, Rawend.x, Rawend.y);
//
//		if (Rawstart.isEqualTo(Rawend)) {
//			acutPrintf(L"\nSkipping corner %zu due to identical start and end points", cornerNum + 1);
//			continue;
//		}
//
//		AcGeVector3d direction = (Rawend - Rawstart);
//		acutPrintf(L"\nbefore normalize Direction with Raw start and end: %.2f, %.2f", direction.x, direction.y);
//		if (direction.length() == 0) {
//			acutPrintf(L"\nSkipping corner %zu due to zero-length vector", cornerNum + 1);
//			continue;
//		}
//		direction.normalize(); //1
//		acutPrintf(L"\nAfter normalize Direction with Raw start and end: %.2f, %.2f", direction.x, direction.y);
//
//		AcGeVector3d reverseDirection = (Rawstart - Rawend).normal();
//		reverseDirection.normalize();
//		AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
//		AcGePoint3d current = corners[cornerNum];
//		AcGePoint3d next;
//		AcGePoint3d nextNext;
//		bool isConcave = isCornerConcave(prev, current, next);
//		bool isConvex = !isConcave && isCornerConvex(prev, current, next);
//
//		bool isAdjacentConcave = isCornerConcave(current, next, nextNext);
//		bool isAdjacentConvex = !isAdjacentConcave && isCornerConvex(current, next, nextNext);
//
//		if (isOuter) {
//			//print if outer
//			//acutPrintf(L"\nOuter loop detected");
//			isConvex = !isConvex;
//			isAdjacentConvex = !isAdjacentConvex;
//		}
//		AcGePoint3d start = Rawstart;
//		AcGePoint3d end = Rawend;
//
//		//acutPrintf(L"\nStart: %.2f, %.2f, End: %.2f, %.2f, Direction: %.2f, %.2f",
//		//	start.x, start.y, end.x, end.y, direction.x, direction.y);
//		int adjustment = calculatedAdjustment(distanceBetweenPolylines);
//		if (isOuter) {
//			start += direction * adjustment;
//			end -= direction * adjustment;
//		}
//		else {
//			start += direction * adjustment;
//			end -= direction * adjustment;
//		}
//
//		//acutPrintf(_T("\nadjustment: %d"), adjustment);
//		// Save the segment
//		Rawsegments.push_back(std::make_pair(Rawstart, Rawend));
//		segments.push_back(std::make_pair(start, end));
//		//acutPrintf(L"\nSegment %zu added: Start (%f, %f), End (%f, %f)",
//		//	segments.size(), start.x, start.y, end.x, end.y);
//
//	}
//
//	// Verify that segments are populated
//	if (segments.size() < corners.size()) {
//		acutPrintf(L"\nError: Segments size (%zu) is smaller than corners size (%zu)",
//			segments.size(), corners.size());
//		return;
//	}
//
//	for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
//		auto segment = segments[cornerNum];
//		auto Rawsegment = Rawsegments[cornerNum];
//		AcGePoint3d start = segment.first;
//		AcGePoint3d end = segment.second;
//		// Snap the points to the grid
//		snapToGrid(start);
//		snapToGrid(end);
//		AcGePoint3d Rawstart = Rawsegment.first;
//		AcGePoint3d Rawend = Rawsegment.second;
//		adjustStartAndEndPoints(start, end);
//		//print adjusted start and end points
//		acutPrintf(L"\nAdjusted Start: %.2f, %.2f, End: %.2f, %.2f", start.x, start.y, end.x, end.y);
//
//		AcGeVector3d direction = calculateCardinalDirection(start, end);
//		//Print the direction
//		acutPrintf(L"\nbefore normalize Direction with adjusted start and end: %.2f, %.2f", direction.x, direction.y);
//		if (direction.length() == 0) {
//			acutPrintf(L"\nSkipping corner %zu due to zero-length vector", cornerNum + 1);
//			continue;
//		}
//		//direction.normalize(); //2
//		//Print the direction
//		acutPrintf(L"\nAfter Normalize Direction with adjusted start and end: %.2f, %.2f", direction.x, direction.y);
//
//		AcGeVector3d reverseDirection = (Rawstart - Rawend).normal();
//
//
//		if (!isInteger(direction.x) || !isInteger(direction.y)) {
//			if (cornerNum < corners.size() - 1) {
//				start = corners[cornerNum];
//				end = corners[cornerNum - closeLoopCounter];
//				closeLoopCounter = -1;
//				loopIndexLastPanel = 1;
//			}
//			else {
//				start = corners[cornerNum];
//				end = corners[cornerNum - closeLoopCounter];
//			}
//		}
//
//		AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
//		AcGePoint3d current = corners[cornerNum];
//		AcGePoint3d next;
//		AcGePoint3d nextNext;
//
//		if (cornerNum + 1 < corners.size()) {
//			next = corners[(cornerNum + 1) % corners.size()];
//		}
//		else {
//			next = corners[cornerNum + 1 - closeLoopCounter];
//		}
//
//		if (cornerNum + 2 < corners.size()) {
//			nextNext = corners[(cornerNum + 2) % corners.size()];
//		}
//		else {
//			nextNext = corners[cornerNum + 2 - (corners.size() / 2)];
//		}
//
//		// Get previous and next corners for concave/convex check
//		if (isCloseToProcessedCorners(current, processedCorners, proximityTolerance)) {
//			acutPrintf(L"\nCorner %zu is close to a processed corner. Skipping.", cornerNum + 1);
//			continue; // Skip this corner if it is close to a processed corner
//		}
//
//		bool isConcave = isCornerConcave(prev, current, next);
//		bool isConvex = !isConcave && isCornerConvex(prev, current, next);
//
//		bool isAdjacentConcave = isCornerConcave(current, next, nextNext);
//		bool isAdjacentConvex = !isAdjacentConcave && isCornerConvex(current, next, nextNext);
//
//		bool prevClockwise = isClockwise(prev, start, end);
//		bool nextClockwise = isClockwise(start, end, next);
//
//		direction = (Rawend - Rawstart).normal();
//		reverseDirection = (Rawstart - Rawend).normal();
//		AcGePoint3d startDistance = corners[cornerNum];
//		AcGePoint3d endDistance = corners[(cornerNum + 1) % corners.size()];
//
//		double distance = start.distanceTo(end);
//		//acutPrintf(L"\nDistance between corners: %f", distance);
//		//double distance = 3100;
//		//int adjustment = calculatedAdjustment(distanceBetweenPolylines);
//		
//		//if (isOuter) {
//		//	AcGePoint3d startDistance = corners[cornerNum];
//		//	AcGePoint3d endDistance = corners[(cornerNum + 1) % corners.size()];
//		//	acutPrintf(L"\nOuter loop detected");
//		//	distance = startDistance.distanceTo(endDistance);
//		//	distance -= 2*(450);
//		//}
//		//else {
//		//	distance = start.distanceTo(end);
//		//}
//		//acutPrintf(L"\nDistance between corners: %f", distance);
//
//		processedCorners.push_back(current); // Mark the corner as processed
//		AcGePoint3d currentPoint = start;
//		double rotation = atan2(direction.y, direction.x);
//		//double panelLength;
//		bool flagInitialPanelLength = false;
//
//		if (isOuter) {
//			rotation += M_PI;
//		}
//
//		int panelIndex = 1; // Initialize panel index before the loop
//
//		for (const auto& panel : panelSizes) {
//			currentHeight = 0; // Reset current height for each configuration
//			//currentPoint += direction * panel.length; should only run once at start
//			
//			for (int panelNum = 0; panelNum < 3; panelNum++) { // Iterate through heights
//				AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());
//
//
//				if (assetId != AcDbObjectId::kNull) {
//					// Calculate number of vertical panels required
//					int numPanelsHeight = static_cast<int>(wallHeight / panelHeights[panelNum]);
//
//					if (numPanelsHeight > 0) {
//						// Calculate how many horizontal panels fit with this panel length
//						int numPanels = static_cast<int>(distance / panel.length);
//						double remainingDistance = distance - (numPanels * panel.length);
//						//acutPrintf(L"\nRemaining distance: %f, and Distance: %f", remainingDistance, distance);
//						
//						//// Debug output
//						//acutPrintf(L"\nUsing panel length: %f, Number of panels: %d, Remaining distance: %f",
//						//	panel.length, numPanels, distance);
//
//						for (int i = 0; i < numPanels; i++) {
//							if (i == 0 && flagInitialPanelLength == false) {
//								currentPoint += direction * panel.length;
//								flagInitialPanelLength = true;
//							}
//							// Print the current panel index
//							//acutPrintf(L"\npanelIndex: %d", panelIndex);
//
//							// Calculate the panel's position
//							AcGePoint3d currentPointWithHeight = currentPoint;
//							currentPointWithHeight.z += currentHeight;
//
//							// Debug: Print the panel's placement
//							//acutPrintf(L"\nPanel Base Point: (%f, %f, %f)", currentPointWithHeight.x, currentPointWithHeight.y, currentPointWithHeight.z);
//							//acutPrintf(L"\nPanel End Point: (%f, %f, %f)",
//							//	(currentPointWithHeight - direction * panel.length).x,
//							//	(currentPointWithHeight - direction * panel.length).y,
//							//	(currentPointWithHeight - direction * panel.length).z);
//
//							// Normalize and snap rotation
//							//rotation = normalizeAngle(rotation);
//							rotation = snapToExactAngle(rotation, TOLERANCE);
//
//							// Add the panel to the wallPanels vector
//							wallPanels.push_back({
//								currentPointWithHeight,  // AcGePoint3d (base point)
//								assetId,                 // AcDbObjectId (assetId)
//								rotation,                // double (rotation)
//								panel.length,            // double (length)
//								panelHeights[panelNum],  // int (height)
//								loopIndex,               // int (loopIndex)
//								isOuter                  // bool (isOuter)
//								});
//
//							// Update variables
//							totalPanelsPlaced++;
//							panelIndex++;
//
//							// Move currentPoint to the end of the current panel
//							if (i == numPanels - 1) {
//								currentPoint += direction * remainingDistance;
//								//currentPoint += direction * panel.length;
//							}
//							else{
//								currentPoint += direction * panel.length;
//								//acutPrintf(L"\nCurrent point after placement: (%f, %f, %f)", currentPoint.x, currentPoint.y, currentPoint.z);
//							}
//
//						}
//						distance = remainingDistance; // Update distance for the next panel size
//					}
//
//					// Update height for the next row of panels
//					currentHeight += panelHeights[panelNum];
//					//acutPrintf(L"\nCurrent height: %d", currentHeight);
//
//					// Stop if we've reached the required wall height
//					if (currentHeight >= wallHeight) {
//						//acutPrintf(L"\nWall height achieved: %f", currentHeight);
//						break;
//					}
//				}
//			}
//
//
//			// Debug: Print distance after processing this panel size
//			//acutPrintf(L"\nDistance remaining after panel size %f: %f", panel.length, distance);
//		}
//
//
//
//
//
//		// Fourth Pass: Adjust positions for specific asset IDs (compensators)
//		std::vector<AcDbObjectId> centerAssets = {
//			loadAsset(L"128285X"),
//			loadAsset(L"129842X"),
//			loadAsset(L"129879X"),
//			loadAsset(L"129884X"),
//			loadAsset(L"128287X"),
//			loadAsset(L"128292X")
//		};
//
//		//for (int panelNum = 0; panelNum < totalPanelsPlaced; ++panelNum) {
//		//	WallPanel& panel = wallPanels[panelNum];
//
//		//	// Check if the current panel is a compensator
//		//	if (std::find(centerAssets.begin(), centerAssets.end(), panel.assetId) != centerAssets.end()) {
//		//		acutPrintf(L"\nAdjusting compensator at panel %d", panelNum);
//
//		//		// Find the start and end points of the segment
//		//		// Use corner locations to find the segment start and end, assuming `cornerLocations` holds the correct corner indices.
//		//		int startCornerIndex = cornerLocations[panelNum];  // Adjust this if necessary
//		//		int endCornerIndex = cornerLocations[panelNum + 1];  // Adjust this as well
//
//		//		AcGePoint3d startPoint = wallPanels[startCornerIndex].position;  // Use the start point of the segment
//		//		AcGePoint3d endPoint = wallPanels[endCornerIndex].position; // Use the end point of the segment
//
//		//		// Calculate direction vector between start and end points
//		//		AcGeVector3d directionSegment = endPoint - startPoint;
//		//		directionSegment *= 0.5;  // Scale direction vector by 0.5 (half the vector)
//
//		//		// Calculate segment center by moving the startPoint half the distance towards the endPoint
//		//		AcGePoint3d segmentCenter = startPoint + directionSegment;
//
//		//		acutPrintf(L"\nSegment center at: (%f, %f, %f)", segmentCenter.x, segmentCenter.y, segmentCenter.z);
//
//		//		// Move the compensator to the center of the segment
//		//		AcGeVector3d direction = (segmentCenter - panel.position).normal();  // Direction from current panel to the center
//		//		panel.position = segmentCenter;  // Move compensator to the center
//
//		//		acutPrintf(L"\nCompensator placed at: (%f, %f, %f)", panel.position.x, panel.position.y, panel.position.z);
//
//		//		// Move all subsequent panels forward by the length of the compensator
//		//		for (int i = panelNum + 1; i < totalPanelsPlaced; ++i) {
//		//			WallPanel& nextPanel = wallPanels[i];
//		//			nextPanel.position += direction * panel.length;  // Move each subsequent panel forward by the compensator length
//		//			acutPrintf(L"\nPanel %d moved to: (%f, %f, %f)", i, nextPanel.position.x, nextPanel.position.y, nextPanel.position.z);
//		//		}
//		//	}
//		//}
//
//
//	}
//
//	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
//	if (!pDb) {
//		acutPrintf(_T("\nNo working database found."));
//		return;
//	}
//
//	AcDbBlockTable* pBlockTable;
//	if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
//		acutPrintf(_T("\nFailed to get block table."));
//		return;
//	}
//
//	AcDbBlockTableRecord* pModelSpace;
//	if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
//		acutPrintf(_T("\nFailed to get model space."));
//		pBlockTable->close();
//		return;
//	}
//
//	wallHeight = globalVarHeight;
//	currentHeight = globalVarHeight;
//
//	// Fifth Pass: Place all wall panels
//
//	for (const auto& panel : wallPanels) {
//
//		// Create the first wall panel block reference
//		AcDbBlockReference* pBlockRef = new AcDbBlockReference();
//		pBlockRef->setPosition(panel.position);
//		pBlockRef->setBlockTableRecord(panel.assetId);
//		pBlockRef->setRotation(panel.rotation);
//
//		if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
//			acutPrintf(_T("\nFailed to place wall segment."));
//		}
//		pBlockRef->close();
//
//		// Set current height and timber height
//		currentHeight = panel.height;
//
//		// Iterate over the panel sizes and place additional panels
//		for (const auto& panel2 : panelSizes) {
//			if (panel2.length == panel.length) {
//				// Handle panel height calculation and placement
//				for (int panelNum = 0; panelNum < 3; panelNum++) {
//					AcDbObjectId assetId = loadAsset(panel2.id[panelNum].c_str());
//
//					if (assetId != AcDbObjectId::kNull) {
//						int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);
//
//						// Place additional panels as needed
//						for (int x = 0; x < numPanelsHeight; x++) {
//							AcGePoint3d currentPointWithHeight = panel.position;
//							currentPointWithHeight.z += currentHeight;
//
//							AcDbBlockReference* pBlockRef = new AcDbBlockReference();
//							pBlockRef->setPosition(currentPointWithHeight);
//							pBlockRef->setBlockTableRecord(assetId);
//							pBlockRef->setRotation(panel.rotation);
//
//							if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
//								acutPrintf(_T("\nFailed to place wall segment."));
//							}
//							pBlockRef->close();
//
//							// Update current height for next placement
//							currentHeight += panelHeights[panelNum];
//						}
//					}
//				}
//			}
//		}
//	}
//
//	pModelSpace->close();
//	pBlockTable->close();
//}

void WallPlacer::placeWalls() {
	// Initialize variables to store detected corners and T-joints
	std::vector<PolylineCorners> polylineCornerGroups;
	std::vector<TJoint> detectedTJoints;
	std::vector<std::vector<AcGePoint3d>> outerLoops;
	std::vector<std::vector<AcGePoint3d>> innerLoops;
	std::vector<std::vector<AcGePoint3d>> allPolylines;
	std::vector<bool> loopIsClockwise;
	struct WallPanel {
		AcGePoint3d position;
		AcDbObjectId assetId;
		double rotation;
		int length;
		int height;
		int loopIndex;
		bool isOuterLoop;

	};

	std::vector<WallPanel> wallPanels;
	std::vector<std::pair<AcGePoint3d, AcGePoint3d>> Rawsegments;
	std::vector<std::pair<AcGePoint3d, AcGePoint3d>> segments;

	// Print the detected corners
	// Detect closed polylines and group their corners
	detectClosedPolylinesAndCorners(polylineCornerGroups);

	// Check if polylines were detected correctly
	if (polylineCornerGroups.empty()) {
		acutPrintf(L"\nNo closed polylines detected.\n");
		return;
	}

	// Initialize corners and get them from the polylineCornerGroups
	std::vector<AcGePoint3d> corners;
	for (const auto& polylineGroup : polylineCornerGroups) {
		corners.insert(corners.end(), polylineGroup.corners.begin(), polylineGroup.corners.end());
	}

	for (const auto& polylineGroup : polylineCornerGroups) {
		allPolylines.push_back(polylineGroup.corners);
	}

	classifyLoopsMultiCheck(allPolylines, outerLoops, innerLoops, loopIsClockwise);
	// Loop through all polylines
	for (size_t i = 0; i < allPolylines.size(); ++i) {
		bool isOuter = false;

		// Check if the current polyline is an outer loop
		// Assuming the outermost loop is determined by the largest area (as an example)
		double currentArea = calculatePolygonArea(allPolylines[i]);  // Assuming you have this function to calculate the area
		double maxArea = -1.0;  // Initialize with the smallest possible value
		size_t outerIndex = -1;

		for (size_t j = 0; j < allPolylines.size(); ++j) {
			double area = calculatePolygonArea(allPolylines[j]);
			if (area > maxArea) {
				maxArea = area;
				outerIndex = j;
			}
		}

		if (i == outerIndex) {
			isOuter = true;
		}

		bool isClockwise = loopIsClockwise[i];  // Check if the current loop is clockwise or counterclockwise

	}

	//print the classified loops as outer and inner loops with clockwise or anticlockwise
	//acutPrintf(L"\nOuter loops:\n");
	//for (size_t i = 0; i < outerLoops.size(); ++i) {
	//	acutPrintf(L"Outer loop %zu:\n", i + 1);
	//	for (size_t j = 0; j < outerLoops[i].size(); ++j) {
	//		acutPrintf(L"Corner %zu at: %.2f, %.2f\n", j + 1, outerLoops[i][j].x, outerLoops[i][j].y);
	//	}
	//	acutPrintf(L"Clockwise: %s\n", loopIsClockwise[i] ? L"true" : L"false");  // Correctly print bool
	//}

	//acutPrintf(L"\nInner loops:\n");
	//for (size_t i = 0; i < innerLoops.size(); ++i) {
	//	acutPrintf(L"Inner loop %zu:\n", i + 1);
	//	for (size_t j = 0; j < innerLoops[i].size(); ++j) {
	//		acutPrintf(L"Corner %zu at: %.2f, %.2f\n", j + 1, innerLoops[i][j].x, innerLoops[i][j].y);
	//	}
	//	acutPrintf(L"Clockwise: %s\n", loopIsClockwise[i + outerLoops.size()] ? L"true" : L"false");  // Correctly print bool for inner loops
	//}

	// T-Joint Detection
	detectTJoints(allPolylines, detectedTJoints);

	int wallHeight = globalVarHeight;
	int currentHeight = 0;
	int panelHeights[] = { 1350, 1200, 600 };

	std::vector<Panel> panelSizes = {
		{600, {L"128282X", L"136096X", L"129839X"}},
		{450, {L"128283X", L"Null", L"129840X"}},
		{300, {L"128284X", L"Null", L"129841X"}},
		{150, {L"128285X", L"Null", L"129842X"}},
		{100, {L"128292X", L"Null", L"129884X"}},
		{50, {L"128287X", L"Null", L"129879X"}}
	};

	int closeLoopCounter = 0;
	int outerLoopIndexValue = 0;
	size_t startIndex;
	size_t endIndex;
	int loopIndex = 0;
	int loopIndexLastPanel = 0;
	 closeLoopCounter = -1;
	int distanceBetweenPolylines = getDistanceFromUser();

	double totalPanelsPlaced = 0;
	std::vector<int> cornerLocations;
	AcGePoint3d first_start;

	// Third Pass: Save all positions, asset IDs, and rotations
	for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {

		closeLoopCounter++;
		cornerLocations.push_back(static_cast<int>(totalPanelsPlaced));
		AcGePoint3d start = corners[cornerNum];
		AcGePoint3d end = corners[cornerNum + 1];
		if (cornerNum == 0) {
			first_start = start;
			//acutPrintf(_T("\nFirst Start: %f, %f"), first_start.x, first_start.y);
		}

		AcGeVector3d direction = (end - start).normal();
		direction.normalize();
		AcGeVector3d reverseDirection = (start - end).normal();

		if (!isInteger(direction.x) || !isInteger(direction.y)) {
			if (cornerNum < corners.size() - 1) {
				start = corners[cornerNum];
				end = corners[cornerNum - closeLoopCounter];
				closeLoopCounter = -1;
				loopIndexLastPanel = 1;
			}
			else {
				start = corners[cornerNum];
				end = corners[cornerNum - closeLoopCounter];
			}
		}

			AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
			AcGePoint3d current = corners[cornerNum];
			AcGePoint3d next;
			AcGePoint3d nextNext;
			if (cornerNum + 1 < corners.size()) {
				//acutPrintf(_T("\nIfTest"));
				next = corners[(cornerNum + 1) % corners.size()];
			}
			else {
				//acutPrintf(_T("\nElseTest"));
				next = corners[cornerNum + 1 - closeLoopCounter];
			}
			if (cornerNum + 2 < corners.size()) {
				nextNext = corners[(cornerNum + 2) % corners.size()]; //
			}
			else {
				nextNext = corners[cornerNum + 2 - (corners.size() / 2)];
			}

			//acutPrintf(_T("\nNext: %f, %f"), next.x, next.y);
			//acutPrintf(_T("\nNextnext: %f, %f"), nextNext.x, nextNext.y);


			// Get previous and next corners
			if (isCloseToProcessedCorners(current, processedCorners, proximityTolerance)) {
				//debugging information
				//acutPrintf(_T("\nSkipping corner at (%f, %f) as it is close to a processed corner."), current.x, current.y);
				continue; // Skip this corner as it is close to an already processed one
			}

			bool isConcave = isCornerConcave(prev, current, next);
			bool isConvex = !isConcave && isCornerConvex(prev, current, next);

			// Flagging adjacent corners
			//bool isAdjacentConvex = false;
			//bool isAdjacentConcave = false;
			bool isAdjacentConcave = isCornerConcave(current, next, nextNext);
			bool isAdjacentConvex = !isAdjacentConcave && isCornerConvex(current, next, nextNext);

			if (isConvex) {
				// Flag previous and next corners as adjacent to a convex corner
				//acutPrintf(_T("\nConvex"));
				size_t prevIndex = (cornerNum + corners.size() - 1) % corners.size();
				size_t nextIndex = (cornerNum + 1) % corners.size();

				//isAdjacentConvex = true;
				// Set flag for adjacent corners
				//isAdjacentConcave = false;  // Reset any concave flag if the previous corner was marked incorrectly
			}
			else if (isConcave) {
				//acutPrintf(_T("\nConcave"));
				//isAdjacentConvex = false;  // Reset any convex flag if the previous corner was marked incorrectly
				// Flagging adjacent corners as adjacent to concave is not needed in this approach
			}

			bool prevClockwise = isClockwise(prev, start, end);
			bool nextClockwise = isClockwise(start, end, next);

			bool isInner = loopIndex != outerLoopIndexValue;
			bool isOuter = !isInner;  // Outer loop is the opposite of inner
			if (!loopIsClockwise[loopIndex]) {
				isInner = !isInner;
				isOuter = !isOuter;
			}

			direction = (end - start).normal();
			//acutPrintf(_T("\nDirection: %f, %f"), direction.x, direction.y);
			reverseDirection = (start - end).normal();

			int adjustment = 0;

			if(isOuter) {
				
				if (distanceBetweenPolylines == 150 || distanceBetweenPolylines == 200) {
					adjustment = 550;
				}
				else if (distanceBetweenPolylines == 250) {
					adjustment = 600;
				}
				else if (distanceBetweenPolylines == 300) {
					adjustment = 650;
				}
				else if (distanceBetweenPolylines == 350) {
					adjustment = 700;
				}
				else if (distanceBetweenPolylines == 400) {
					adjustment = 750;
				}
				else if (distanceBetweenPolylines == 450) {
					adjustment = 800;
				}
				else if (distanceBetweenPolylines == 500) {
					adjustment = 850;
				}
				else if (distanceBetweenPolylines == 550) {
					adjustment = 900;
				}
				else if (distanceBetweenPolylines == 600) {
					adjustment = 950;
				}
				else if (distanceBetweenPolylines == 650) {
					adjustment = 1000;
				}
				else if (distanceBetweenPolylines == 700) {
					adjustment = 1050;
				}
				else if (distanceBetweenPolylines == 750) {
					adjustment = 1100;
				}
				else if (distanceBetweenPolylines == 800) {
					adjustment = 1150;
				}
				else if (distanceBetweenPolylines == 850) {
					adjustment = 1200;
				}
				else if (distanceBetweenPolylines == 900) {
					adjustment = 1250;
				}
				else if (distanceBetweenPolylines == 950) {
					adjustment = 1300;
				}
				else if (distanceBetweenPolylines == 1000) {
					adjustment = 1350;
				}
				else if (distanceBetweenPolylines == 1050) {
					adjustment = 1400;
				}
				else if (distanceBetweenPolylines == 1100) {
					adjustment = 1450;
				}
				else if (distanceBetweenPolylines == 1150) {
					adjustment = 1500;
				}
				else if (distanceBetweenPolylines == 1200) {
					adjustment = 1550;
				}
				else if (distanceBetweenPolylines == 1250) {
					adjustment = 1600;
				}
				else if (distanceBetweenPolylines == 1300) {
					adjustment = 1650;
				}
				else if (distanceBetweenPolylines == 1350) {
					adjustment = 1700;
				}
				else if (distanceBetweenPolylines == 1400) {
					adjustment = 1750;
				}
				else if (distanceBetweenPolylines == 1450) {
					adjustment = 1800;
				}
				else if (distanceBetweenPolylines == 1500) {
					adjustment = 1850;
				}
				else if (distanceBetweenPolylines == 1550) {
					adjustment = 1900;
				}
				else if (distanceBetweenPolylines == 1600) {
					adjustment = 1950;
				}
				else if (distanceBetweenPolylines == 1650) {
					adjustment = 2000;
				}
				else if (distanceBetweenPolylines == 1700) {
					adjustment = 2050;
				}
				else if (distanceBetweenPolylines == 1750) {
					adjustment = 2100;
				}
				else if (distanceBetweenPolylines == 1800) {
					adjustment = 2150;
				}
				else if (distanceBetweenPolylines == 1850) {
					adjustment = 2200;
				}
				else if (distanceBetweenPolylines == 1900) {
					adjustment = 2250;
				}
				else if (distanceBetweenPolylines == 1950) {
					adjustment = 2300;
				}
				else if (distanceBetweenPolylines == 2000) {
					adjustment = 2350;
				}
				else if (distanceBetweenPolylines == 2050) {
					adjustment = 2400;
				}
				else if (distanceBetweenPolylines == 2100) {
					adjustment = 2450;
				}
				else {
					adjustment = 150; // Default case for any unexpected distance value
				}
				adjustment -= 100; // Adjust the distance for the corner post
				

				start += direction * adjustment;
				end += reverseDirection * adjustment;

			}
			else {
				if (distanceBetweenPolylines == 150) {
					start += direction * 300;
					end += reverseDirection * 300;
				}
				else {
					start += direction * 250;
					end += reverseDirection * 250;
				}
			}

			processedCorners.push_back(current); // Mark the corner as processed

			double distance = start.distanceTo(end);
			//acutPrintf(_T("\nDistance: %f"), distance);
			AcGePoint3d currentPoint = start;

			double rotation = atan2(direction.y, direction.x);
			double panelLength;

			if (isOuter) {
				
				rotation += M_PI;
			}

			for (const auto& panel : panelSizes) {
				currentHeight = 0;
				//AcGePoint3d backupCurrentPoint = currentPoint;
				//double backupDistance = distance;

				for (int panelNum = 0; panelNum < 3; panelNum++) {
					AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());

					if (assetId != AcDbObjectId::kNull) {
						int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

						//acutPrintf(_T("\nnumPanelsHeight = %d"), numPanelsHeight);
						//for (int x = 0; x < numPanelsHeight; x++) {
						if (numPanelsHeight > 0) {
							//currentPoint = backupCurrentPoint;
							//distance = backupDistance;

							int numPanels = static_cast<int>(distance / panel.length);
							//acutPrintf(_T("\nnumPanels = %d"), numPanels);
							for (int i = 0; i < numPanels; i++) {
								AcGePoint3d currentPointWithHeight = currentPoint;
								currentPointWithHeight.z += currentHeight;
								if (isOuter) {
									currentPointWithHeight += direction * panel.length;
								}
								rotation = normalizeAngle(rotation);
								rotation = snapToExactAngle(rotation, TOLERANCE);

								panelLength = panel.length;
								wallPanels.push_back({ currentPointWithHeight, assetId, rotation, panel.length, panelHeights[panelNum], loopIndex, isOuter });

								totalPanelsPlaced++;
								currentPoint += direction * panelLength;
								distance -= panelLength;
							}

							//acutPrintf(_T("\n%d wall segments placed successfully."), numOfWallSegmentsPlaced);
							currentHeight = wallHeight;

						}
					}
				}
			}
		segments.push_back(std::make_pair(start, end)); // Save segment for later compensator placement
		loopIndex = loopIndexLastPanel;
	}

	// Forth Pass: Adjust positions for specific asset IDs(compensators)
	std::vector<AcDbObjectId> centerAssets = {
		loadAsset(L"128285X"),
		loadAsset(L"129842X"),
		loadAsset(L"129879X"),
		loadAsset(L"129884X"),
		loadAsset(L"128287X"),
		loadAsset(L"128292X")
	};

	int prevStartCornerIndex = -1;
	int movedCompensators = 0;

	//for (int panelNum = 0; panelNum < totalPanelsPlaced; ++panelNum) {
	//	WallPanel& panel = wallPanels[panelNum];
	//	if (std::find(centerAssets.begin(), centerAssets.end(), panel.assetId) != centerAssets.end()) {

	//		// Find the two corner points between which the panel is placed
	//		int panelPosition = panelNum;  // This should be the index of the panel
	//		//acutPrintf(_T("\nFound compensator at %d."), panelNum);
	//		WallPanel detectedPanel = wallPanels[panelPosition];
	//		AcGePoint3d detectedPanelPosition = detectedPanel.position;
	//		AcDbObjectId detectedPanelId = detectedPanel.assetId;

	//		double panelLength = wallPanels[panelPosition].length;


	//		//acutPrintf(_T(" panelLength = %f."), panelLength);

	//		int startCornerIndex = -1;
	//		int endCornerIndex = -1;

	//		for (int j = 0; j < cornerLocations.size(); ++j) {
	//			if (cornerLocations[j] < panelNum) {
	//				startCornerIndex = cornerLocations[j];  // Last corner before the panel
	//			}
	//			if (cornerLocations[j] > panelNum) {
	//				endCornerIndex = cornerLocations[j];  // First corner after the panel
	//				break;
	//			}
	//		}
	//		//acutPrintf(_T(" Between %d."), startCornerIndex);
	//		if (endCornerIndex == -1) {
	//			endCornerIndex = panelNum + 1;
	//		}
	//		//acutPrintf(_T(" and %d."), endCornerIndex);


	//		if (prevStartCornerIndex != startCornerIndex) {
	//			movedCompensators = 0;
	//			prevStartCornerIndex = startCornerIndex;
	//		}

	//		// Validate the corner indices
	//		if (startCornerIndex == -1 || endCornerIndex == -1) {
	//			// No valid corners found; handle error
	//		}

	//		// Calculate the center index in wallPanels
	//		int centerIndex = (startCornerIndex + endCornerIndex) / 2;

	//		// Get positions of centerIndex and detectedPanel(compensator)
	//		AcGePoint3d centerPanelPosition = wallPanels[centerIndex + movedCompensators].position;

	//		AcGeVector3d direction = (wallPanels[panelNum].position - wallPanels[centerIndex].position).normal();

	//		// Adjust the position of the detected compensator panel
	//		wallPanels[panelNum].position = centerPanelPosition;
	//		if (wallPanels[panelNum].isOuterLoop && loopIsClockwise[wallPanels[panelNum].loopIndex]) {
	//			wallPanels[panelNum].position -= direction * wallPanels[centerIndex + movedCompensators].length;
	//			wallPanels[panelNum].position += direction * panelLength;
	//		}
	//		if (wallPanels[panelNum].isOuterLoop && !loopIsClockwise[wallPanels[panelNum].loopIndex]) {
	//			wallPanels[panelNum].position -= direction * wallPanels[centerIndex + movedCompensators].length;
	//			wallPanels[panelNum].position += direction * panelLength;
	//		}


	//		//acutPrintf(_T("\t | Moved to centerIndex = %d."), centerIndex + movedCompensators);
	//		for (int centerToCornerPanelNum = centerIndex + movedCompensators; centerToCornerPanelNum < panelNum - movedCompensators; centerToCornerPanelNum++) {
	//			wallPanels[centerToCornerPanelNum].position = wallPanels[centerToCornerPanelNum].position + direction * panelLength;
	//		}
	//		if (prevStartCornerIndex == startCornerIndex) {
	//			movedCompensators++;
	//		}
	//	}
	//}

	//acutPrintf(_T("\ncornerLocations (size: %d): "), cornerLocations.size());
	//acutPrintf(_T("\ncornerLocations: "));
	for (size_t i = 0; i < cornerLocations.size(); ++i) {
		//acutPrintf(_T("%d "), cornerLocations[i]);
	}
	//acutPrintf(_T("\n"));

	wallHeight = globalVarHeight;
	currentHeight = globalVarHeight;

	// Fifth Pass: Place all wall panels
	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
	if (!pDb) {
		acutPrintf(_T("\nNo working database found."));
		return;
	}

	AcDbBlockTable* pBlockTable;
	if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
		acutPrintf(_T("\nFailed to get block table."));
		return;
	}

	AcDbBlockTableRecord* pModelSpace;
	if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
		acutPrintf(_T("\nFailed to get model space."));
		pBlockTable->close();
		return;
	}
	int timberHeight;
	for (const auto& panel : wallPanels) {

		AcDbBlockReference* pBlockRef = new AcDbBlockReference();
		pBlockRef->setPosition(panel.position);
		pBlockRef->setBlockTableRecord(panel.assetId);
		pBlockRef->setRotation(panel.rotation);
		pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

		if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
			acutPrintf(_T("\nFailed to place wall segment."));
		}
		pBlockRef->close();
		currentHeight = panel.height;
		timberHeight = panel.height;
		for (const auto& panel2 : panelSizes) {
			if (panel2.length == panel.length) {
				for (int panelNum = 0; panelNum < 3; panelNum++) {
					AcDbObjectId assetId = loadAsset(panel2.id[panelNum].c_str());

					if (assetId != AcDbObjectId::kNull) {
						int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

						for (int x = 0; x < numPanelsHeight; x++) {

							AcGePoint3d currentPointWithHeight = panel.position;
							currentPointWithHeight.z += currentHeight;

							AcDbBlockReference* pBlockRef = new AcDbBlockReference();
							pBlockRef->setPosition(currentPointWithHeight);
							pBlockRef->setBlockTableRecord(assetId);
							pBlockRef->setRotation(panel.rotation);
							pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));

							if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
								acutPrintf(_T("\nFailed to place wall segment."));
							}
							pBlockRef->close();
							currentHeight += panelHeights[panelNum];
						}
					}
				}
			}
		}
	}

	pModelSpace->close();
	pBlockTable->close();

	acutPrintf(_T("\nCompleted placing walls."));
}
