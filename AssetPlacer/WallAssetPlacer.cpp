







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
const int BATCH_SIZE = 1000; 

const double TOLERANCE = 0.1; 
double proximityTolerance = 1.0; 
std::vector<AcGePoint3d> processedCorners; 


struct Panel {
	int length;
	std::wstring id[3];
};


struct PolylineCorners {
	AcDbObjectId polylineId;          
	std::vector<AcGePoint3d> corners; 

	PolylineCorners(AcDbObjectId id) : polylineId(id) {}
};


bool isInteger(double value, double tolerance = 1e-9) {
	return std::abs(value - std::round(value)) < tolerance;
}


bool isCloseToProcessedCorners(const AcGePoint3d& point, const std::vector<AcGePoint3d>& processedCorners, double tolerance) {
	for (const auto& processedCorner : processedCorners) {
		if (point.distanceTo(processedCorner) < tolerance) {
			return true; 
		}
	}
	return false;
}


bool isCornerConcave(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
	
	AcGeVector3d v1 = current - prev;
	AcGeVector3d v2 = next - current;
	double cross = v1.x * v2.y - v1.y * v2.x;

	double tolerance = 1e-6;

	bool isConcave = cross < -tolerance;

	
	
	
	

	return isConcave;
}


bool isCornerConvex(const AcGePoint3d& prev, const AcGePoint3d& current, const AcGePoint3d& next) {
	AcGeVector3d v1 = current - prev;
	AcGeVector3d v2 = next - current;
	double cross = v1.x * v2.y - v1.y * v2.x;

	
	double tolerance = 1e-6;

	bool isConvex = cross > tolerance;

	
	
	

	return isConvex;
}


bool isClockwise(const AcGePoint3d& p0, const AcGePoint3d& p1, const AcGePoint3d& p2) {
	
	AcGeVector3d v1 = p1 - p0;  
	AcGeVector3d v2 = p2 - p1;  

	
	AcGeVector3d crossProduct = v1.crossProduct(v2);

	
	
	
	return crossProduct.z < 0;
}


void detectClosedPolylinesAndCorners(std::vector<PolylineCorners>& polylineCornerGroups) {
	polylineCornerGroups.clear();

	
	AcDbBlockTable* pBlockTable = nullptr;
	acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead);

	
	AcDbObjectId blockId;
	pBlockTable->getAt(ACDB_MODEL_SPACE, blockId);
	pBlockTable->close();
	
	AcDbBlockTableRecord* pBlockTableRecord = nullptr;
	acdbOpenObject(pBlockTableRecord, blockId, AcDb::kForRead);

	
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
		

		pEntity->close();

	}

	delete pIterator;

	
	
	
	
	
	
	
	
}


double calculatePolygonArea(const std::vector<AcGePoint3d>& polygon) {
	double area = 0.0;
	for (size_t i = 0; i < polygon.size(); ++i) {
		const AcGePoint3d& p1 = polygon[i];
		const AcGePoint3d& p2 = polygon[(i + 1) % polygon.size()];
		area += (p1.x * p2.y - p2.x * p1.y);  
	}
	return fabs(area) / 2.0;
}


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
	return (crossings % 2) == 1;  
}


double crossProduct(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
	return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}


double getDistanceFromUser() {
	ads_point firstPoint, secondPoint;
	double distance = 0.0;

	
	if (acedGetPoint(NULL, _T("Select the first point: "), firstPoint) != RTNORM) {
		acutPrintf(_T("\nError: Failed to get the first point.\n"));
		return -1;  
	}

	
	if (acedGetPoint(NULL, _T("Select the second point: "), secondPoint) != RTNORM) {
		acutPrintf(_T("\nError: Failed to get the second point.\n"));
		return -1;  
	}

	
	double deltaX = firstPoint[X] - secondPoint[X];
	double deltaY = secondPoint[Y] - firstPoint[Y];

	
	const double tolerance = 1.0;

	
	if (firstPoint[X] != 0.0 && firstPoint[Y] != 0.0 && secondPoint[X] != 0.0 && secondPoint[Y] != 0.0) {
		
		if (std::fabs(deltaX - deltaY) <= tolerance) {
			
			distance = snapToPredefinedValues(deltaX);
		}
		else {
			
			distance = snapToPredefinedValues(std::sqrt(deltaX * deltaX + deltaY * deltaY));
		}
	}
	else {
		acutPrintf(_T("\nPoints were not selected properly. Skipping distance calculation.\n"));
		distance = 200;  
	}

	
	acutPrintf(_T("\nDistance between the points: %.2f\n"), distance);
	return distance;
}


bool directionOfDrawing(std::vector<AcGePoint3d>& points) {
	
	if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
		points.push_back(points.front());
	}

	double totalTurns = 0.0;

	
	for (size_t i = 1; i < points.size() - 1; ++i) {
		totalTurns += crossProduct(points[i - 1], points[i], points[i + 1]);
	}

	
	if (totalTurns < 0) {
		return true;  
	}
	
	else if (totalTurns > 0) {
		return false; 
	}
	
	else {
		acutPrintf(_T("Warning: The shape does not have a defined direction. Defaulting to clockwise.\n"));
		return true;  
	}
}


void classifyLoopsMultiCheck(const std::vector<std::vector<AcGePoint3d>>& allPolylines,
	std::vector<std::vector<AcGePoint3d>>& outerLoops,
	std::vector<std::vector<AcGePoint3d>>& innerLoops, std::vector<bool>& loopIsClockwise) {
	std::vector<bool> isProcessed(allPolylines.size(), false);

	
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

	
	for (size_t i = 0; i < allPolylines.size(); ++i) {
		if (isProcessed[i]) continue;
		bool isClockwise = directionOfDrawing(const_cast<std::vector<AcGePoint3d>&>(allPolylines[i]));
		loopIsClockwise.push_back(isClockwise); 

		bool isOuter = false;
		for (size_t j = 0; j < outerLoops.size(); ++j) {
			if (isPointInsidePolygon(allPolylines[i][0], outerLoops[j])) {
				
				isOuter = true;

				
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

		
		if (!isOuter) {
			outerLoops.push_back(allPolylines[i]);
			isProcessed[i] = true;
		}
	}
}


struct TJoint {
	AcGePoint3d position;      
	AcGePoint3d segmentStart;  
	AcGePoint3d segmentEnd;    

	TJoint() = default;

	TJoint(const AcGePoint3d& pos, const AcGePoint3d& start, const AcGePoint3d& end)
		: position(pos), segmentStart(start), segmentEnd(end) {}
};


bool isPointNearSegment(const AcGePoint3d& point,
	const AcGePoint3d& start,
	const AcGePoint3d& end,
	double threshold) {
	
	AcGeVector3d segmentVector = end - start;
	AcGeVector3d startToPoint = point - start;

	double lengthSq = segmentVector.length() * segmentVector.length();

	
	double projection = startToPoint.dotProduct(segmentVector) / lengthSq;


	
	projection = std::max(0.0, std::min(1.0, projection));

	
	AcGePoint3d closestPoint = start + segmentVector * projection;

	
	double distance = (point - closestPoint).length();

	return distance <= threshold;
}


void detectTJoints(const std::vector<std::vector<AcGePoint3d>>& allLoops,
	std::vector<TJoint>& detectedTJoints,
	double threshold = 150.0) {
	
	detectedTJoints.reserve(allLoops.size() * 10); 

	
	for (size_t i = 0; i < allLoops.size(); ++i) {
		const auto& currentLoop = allLoops[i];

		
		for (const AcGePoint3d& corner : currentLoop) {
			
			for (size_t j = 0; j < allLoops.size(); ++j) {
				if (i == j) continue; 

				const auto& comparisonLoop = allLoops[j];
				size_t segmentCount = comparisonLoop.size();

				
				for (size_t segIdx = 0; segIdx < segmentCount; ++segIdx) {
					const AcGePoint3d& start = comparisonLoop[segIdx];
					const AcGePoint3d& end = comparisonLoop[(segIdx + 1) % segmentCount];

					
					if (isPointNearSegment(corner, start, end, threshold)) {
						
						detectedTJoints.emplace_back(corner, start, end);
					}
				}
			}
		}
	}
}


AcDbObjectId WallPlacer::loadAsset(const wchar_t* blockName) {
	
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
	
	return blockId;
}


void convertToClockwise(std::vector<AcGePoint3d>& polylinePoints) {
	
	if (directionOfDrawing(polylinePoints) == false) {  
		std::reverse(polylinePoints.begin(), polylinePoints.end());
	}
}


void WallPlacer::placeWalls() {
	
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
	struct LoopInfo {
		std::vector<AcGePoint3d> points;  
		bool isOuter;                    
		bool isClockwise;                
		double area;                     
	};
	std::vector<LoopInfo> loopData;
	bool ranLoopData = false;

	std::vector<WallPanel> wallPanels;
	std::vector<std::pair<AcGePoint3d, AcGePoint3d>> Rawsegments;
	std::vector<std::pair<AcGePoint3d, AcGePoint3d>> segments;

	
	
	detectClosedPolylinesAndCorners(polylineCornerGroups);

	
	if (polylineCornerGroups.empty()) {
		acutPrintf(L"\nNo closed polylines detected.\n");
		return;
	}

	
	std::vector<AcGePoint3d> corners;
	for (const auto& polylineGroup : polylineCornerGroups) {
		corners.insert(corners.end(), polylineGroup.corners.begin(), polylineGroup.corners.end());
	}

	for (const auto& polylineGroup : polylineCornerGroups) {
		allPolylines.push_back(polylineGroup.corners);
	}

	classifyLoopsMultiCheck(allPolylines, outerLoops, innerLoops, loopIsClockwise);
	
	double maxArea = -1.0;
	size_t outerIndex = -1;
	
	for (size_t j = 0; j < allPolylines.size(); ++j) {
		double area = calculatePolygonArea(allPolylines[j]);
		if (area > maxArea) {
			maxArea = area;
			outerIndex = j;
		}
	}

	
	for (size_t i = 0; i < allPolylines.size(); ++i) {
		LoopInfo loop;
		
		loop.points = allPolylines[i]; 
		loop.isOuter = (i == outerIndex); 
		loop.isClockwise = loopIsClockwise[i]; 
		loop.area = calculatePolygonArea(allPolylines[i]); 
		
		loopData.push_back(loop); 
	}

	
	
	
	
	
	
	
	
	

	
	
	
	
	
	
	
	

	
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
	struct PanelPlacement {
		int numPanels;   
		int panelLength; 
	};

	int closeLoopCounter = 0;
	int outerLoopIndexValue = 0;
	int loopIndex = 0;
	int loopIndexLastPanel = 0;
	 closeLoopCounter = -1;
	double distanceBetweenPolylines = getDistanceFromUser();

	double totalPanelsPlaced = 0;
	std::vector<int> cornerLocations;
	AcGePoint3d first_start;

	
	for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {

		closeLoopCounter++;
		cornerLocations.push_back(static_cast<int>(totalPanelsPlaced));
		AcGePoint3d start = corners[cornerNum];
		AcGePoint3d end = corners[cornerNum + 1];
		if (cornerNum == 0) {
			first_start = start;
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
			
			next = corners[(cornerNum + 1) % corners.size()];
		}
		else {
			
			next = corners[cornerNum + 1 - closeLoopCounter];
		}
		if (cornerNum + 2 < corners.size()) {
			nextNext = corners[(cornerNum + 2) % corners.size()]; 
		}
		else {
			nextNext = corners[cornerNum + 2 - (corners.size() / 2)];
		}

		
		if (isCloseToProcessedCorners(current, processedCorners, proximityTolerance)) {
			continue; 
		}

		bool isConcave = isCornerConcave(prev, current, next);
		bool isConvex = !isConcave && isCornerConvex(prev, current, next);

		bool isAdjacentConcave = isCornerConcave(current, next, nextNext);
		bool isAdjacentConvex = !isAdjacentConcave && isCornerConvex(current, next, nextNext);

		if (isConvex) {
			
			
			size_t prevIndex = (cornerNum + corners.size() - 1) % corners.size();
			size_t nextIndex = (cornerNum + 1) % corners.size();

			
			
			
		}
		else if (isConcave) {
			
			
			
		}

		bool prevClockwise = isClockwise(prev, start, end);
		bool nextClockwise = isClockwise(start, end, next);

		bool isInner = loopIndex != outerLoopIndexValue;
		bool isOuter = !isInner;  
		if (!loopIsClockwise[loopIndex]) {
			isInner = !isInner;
			isOuter = !isOuter;
		}

		direction = (end - start).normal();
		
		reverseDirection = (start - end).normal();
		double rotation = atan2(direction.y, direction.x);
		int adjustment = 0;
		LoopInfo loop = loopData[loopIndex];
		if (loop.isOuter) {

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
				adjustment = 150; 
			}
			adjustment -= 100; 
			int forStartadjustment = 600;


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

		processedCorners.push_back(current); 

		double distance = start.distanceTo(end);
		
		AcGePoint3d currentPoint = start;

		rotation += M_PI;
		bool flagInitialPanelLength = false;
		int panelIndex = 1;
		std::vector<PanelPlacement> panelPlan; 
		double remainingDistance = distance; 


		for (const auto& panel : panelSizes) {
			currentHeight = 0;
			
			

			for (int panelNum = 0; panelNum < 3; panelNum++) {
				AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());

				if (assetId != AcDbObjectId::kNull) {
					int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

					
					
					if (numPanelsHeight > 0) {

						int numPanels = static_cast<int>(distance / panel.length);
						double remainingDistance = distance - (numPanels * panel.length);
						
						for (int i = 0; i < numPanels; i++) {

							currentPoint += direction * panel.length;
							
							AcGePoint3d currentPointWithHeight = currentPoint;
							currentPointWithHeight.z += currentHeight;

							
							
							

							
							
							rotation = snapToExactAngle(rotation, TOLERANCE);

							
							wallPanels.push_back({
								currentPointWithHeight,  
								assetId,                 
								rotation,                
								panel.length,            
								panelHeights[panelNum],  
								loopIndex,               
								isOuter                  
								});

							
							totalPanelsPlaced++;
							panelIndex++;

							

						}
						distance = remainingDistance; 
					}

					
					currentHeight += panelHeights[panelNum];
					

					
					if (currentHeight >= wallHeight) {
						
						break;
					}
				}
			}
		}
		segments.push_back(std::make_pair(start, end)); 
		loopIndex = loopIndexLastPanel;


		
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

		
		
		

		
		
		
		
		
		

		


		

		
		

		
		
		
		
		
		
		
		
		
		
		
		
		
		


		
		
		
		

		
		
		
		

		
		

		
		

		

		
		
		
		
		
		
		
		
		
		


		
		
		
		
		
		
		
		
		

		
		
		for (size_t i = 0; i < cornerLocations.size(); ++i) {
			
		}
		
		wallHeight = globalVarHeight;
		currentHeight = globalVarHeight;
	}
	
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
