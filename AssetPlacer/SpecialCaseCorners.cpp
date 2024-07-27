// Created by:Ani  (2024-07-25)
//
// TODO:
// SpecialCaseCorners.cpp
/////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SpecialCaseCorners.h"
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
#include "Timber/TimberAssetCreator.h"

const double TOLERANCE = 0.1;  // Define a small tolerance for angle comparisons
const int BATCH_SIZE = 10;    // Define the batch size for processing entities

// Definitions
struct Panel {
    int length;
    std::wstring id;
};

// Utility function to load an asset block from the block table
AcDbObjectId loadAsset(const wchar_t* blockName) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table. Error status: %d\n"), es);
        return AcDbObjectId::kNull;
    }

    AcDbObjectId assetId = AcDbObjectId::kNull;
    AcDbBlockTableRecord* pBlockTableRecord;
    es = pBlockTable->getAt(blockName, pBlockTableRecord, AcDb::kForRead);
    if (es == Acad::eOk) {
        assetId = pBlockTableRecord->objectId();
        pBlockTableRecord->close();
    }
    else {
        acutPrintf(_T("\nFailed to get block table record for block '%s'. Error status: %d\n"), blockName, es);
    }

    pBlockTable->close();
    return assetId;
}

// Utility function to place a panel
void placePanel(const AcGePoint3d& position, const std::wstring& blockName, double rotation) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    AcDbBlockTable* pBlockTable;
    AcDbBlockTableRecord* pModelSpace;

    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return;
    }
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return;
    }

    AcDbObjectId assetId = loadAsset(blockName.c_str());
    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load panel asset."));
        pModelSpace->close();
        pBlockTable->close();
        return;
    }

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(assetId);
    pBlockRef->setRotation(rotation);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // No scaling applied

    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
        acutPrintf(_T("\nFailed to append block reference."));
    }
    pBlockRef->close();

    pModelSpace->close();
    pBlockTable->close();
}

// Function to detect polylines and filter corners based on the distance constraint and direction consistency
std::vector<AcGePoint3d> SpecialCaseCorners::detectPolylines() {
    acutPrintf(_T("\nDetecting polylines..."));
    std::vector<AcGePoint3d> corners;
    std::vector<AcGePoint3d> filteredCorners;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return filteredCorners;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return filteredCorners;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return filteredCorners;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return filteredCorners;
    }

    int entityCount = 0;
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPolyline = AcDbPolyline::cast(pEnt);
                if (pPolyline && pPolyline->numVerts() > 1) {
                    processPolyline(pPolyline, corners, 90.0, TOLERANCE);
                }
            }
            pEnt->close();
            entityCount++;
            if (entityCount % BATCH_SIZE == 0) {
                std::this_thread::sleep_for(std::chrono::seconds(1));  // Avoid resource exhaustion
            }
        }
    }
    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    // Calculate direction vectors and filter based on direction consistency and neighbor context
    for (size_t i = 0; i < corners.size() - 1; ++i) {
        AcGeVector3d direction = (corners[i + 1] - corners[i]).normal();
        double angle = atan2(direction.y, direction.x) * 180.0 / M_PI;

        // Check if angle is close to multiples of 90 degrees
        if (std::fmod(std::abs(angle), 90.0) < TOLERANCE || std::fmod(std::abs(angle), 90.0) > (90.0 - TOLERANCE)) {
            double distance = corners[i].distanceTo(corners[i + 1]);
            if (distance < 100) {
                // Additional context check: Verify previous and next vertices form right angles
                if (i > 0 && i < corners.size() - 2) {
                    AcGeVector3d prevDirection = (corners[i] - corners[i - 1]).normal();
                    AcGeVector3d nextDirection = (corners[i + 2] - corners[i + 1]).normal();

                    double prevAngle = atan2(prevDirection.y, prevDirection.x) * 180.0 / M_PI;
                    double nextAngle = atan2(nextDirection.y, nextDirection.x) * 180.0 / M_PI;

                    if ((std::fmod(std::abs(prevAngle - angle), 90.0) < TOLERANCE || std::fmod(std::abs(prevAngle - angle), 90.0) > (90.0 - TOLERANCE)) &&
                        (std::fmod(std::abs(nextAngle - angle), 90.0) < TOLERANCE || std::fmod(std::abs(nextAngle - angle), 90.0) > (90.0 - TOLERANCE))) {
                        filteredCorners.push_back(corners[i]);
                        filteredCorners.push_back(corners[i + 1]);
                        acutPrintf(_T("Filtered corner pair: (%.6f, %.6f, %.6f) and (%.6f, %.6f, %.6f) with distance %.6f and angle %.6f\n"),
                            corners[i].x, corners[i].y, corners[i].z,
                            corners[i + 1].x, corners[i + 1].y, corners[i + 1].z,
                            distance, angle);
                    }
                }
            }
        }
    }

    acutPrintf(_T("\nDetected %d corners from polylines with required distance and direction consistency."), filteredCorners.size());
    return filteredCorners;
}

// Function to compute if the corner is turning clockwise or counterclockwise
bool isClockwise1(const AcGePoint3d& p0, const AcGePoint3d& p1, const AcGePoint3d& p2) {
    AcGeVector3d v1 = p1 - p0;  // Vector from p0 to p1
    AcGeVector3d v2 = p2 - p1;  // Vector from p1 to p2
    AcGeVector3d crossProduct = v1.crossProduct(v2);  // Compute the cross product

    // Determine the direction of the turn
    // If cross product z-component is positive, the turn is clockwise
    // If cross product z-component is negative, the turn is counterclockwise
    return crossProduct.z < 0;
}

// Determine setup based on distance and place panels accordingly
void SpecialCaseCorners::determineAndPlacePanels(const std::vector<AcGePoint3d>& positions) {
    if (positions.size() < 2) {
        acutPrintf(_T("\nNot enough points to determine panel placement."));
        return;
    }

    for (size_t i = 0; i < positions.size() - 1; ++i) {
        AcGePoint3d start = positions[i];
        AcGePoint3d end = positions[i + 1];
        AcGeVector3d direction = end - start;
        double distance = direction.length();
        direction.normalize();

        // Compute rotation
        double rotation = atan2(direction.y, direction.x);

        // Determine if the current segment is an inside or outside corner
        bool isInsideCorner = isClockwise1(positions[(i + positions.size() - 1) % positions.size()], start, end);

        if (isInsideCorner) {
            rotation += M_PI;
        }

        std::vector<std::pair<Panel, int>> setup;

        // Determine setup based on the calculated distance
        if (distance >= 0 && distance <= 44) {
            setup.push_back({ Panel{static_cast<int>(distance), L"Timber"}, 1 });
        }
        else if (distance == 45) {
            setup.push_back({ Panel{45, L"128283X"}, 1 });
        }
        else if (distance >= 46 && distance <= 59) {
            setup.push_back({ Panel{15, L"128285X"}, 2 });
            setup.push_back({ Panel{static_cast<int>(distance - 30), L"Timber"}, 1 });
        }
        else if (distance == 60) {
            setup.push_back({ Panel{60, L"128282X"}, 1 });
        }
        else if (distance >= 61 && distance <= 64) {
            setup.push_back({ Panel{30, L"128284X"}, 2 });
            setup.push_back({ Panel{static_cast<int>(distance - 60), L"Timber"}, 1 });
        }
        else if (distance == 65) {
            setup.push_back({ Panel{30, L"128284X"}, 2 });
            setup.push_back({ Panel{5, L"128287X"}, 1 });
        }
        else if (distance >= 66 && distance <= 69) {
            setup.push_back({ Panel{30, L"128284X"}, 2 });
            setup.push_back({ Panel{static_cast<int>(distance - 60), L"Timber"}, 1 });
        }
        else if (distance == 70) {
            setup.push_back({ Panel{30, L"128284X"}, 2 });
            setup.push_back({ Panel{10, L"128287X"}, 1 });
        }
        else if (distance >= 71 && distance <= 74) {
            setup.push_back({ Panel{30, L"128284X"}, 2 });
            setup.push_back({ Panel{static_cast<int>(distance - 60), L"Timber"}, 1 });
        }
        else if (distance == 75) {
            setup.push_back({ Panel{75, L"128281X"}, 1 });
        }
        else if (distance >= 76 && distance <= 79) {
            setup.push_back({ Panel{45, L"128283X"}, 1 });
            setup.push_back({ Panel{static_cast<int>(distance - 75), L"Timber"}, 1 });
            setup.push_back({ Panel{30, L"128284X"}, 1 });
        }
        else if (distance == 80) {
            setup.push_back({ Panel{45, L"128283X"}, 1 });
            setup.push_back({ Panel{5, L"128287X"}, 1 });
            setup.push_back({ Panel{30, L"128284X"}, 1 });
        }
        else if (distance >= 81 && distance <= 84) {
            setup.push_back({ Panel{45, L"128283X"}, 1 });
            setup.push_back({ Panel{static_cast<int>(distance - 75), L"Timber"}, 1 });
            setup.push_back({ Panel{30, L"128284X"}, 1 });
        }
        else if (distance == 85) {
            setup.push_back({ Panel{45, L"128283X"}, 1 });
            setup.push_back({ Panel{10, L"128287X"}, 1 });
            setup.push_back({ Panel{30, L"128284X"}, 1 });
        }
        else if (distance >= 86 && distance <= 89) {
            setup.push_back({ Panel{45, L"128283X"}, 1 });
            setup.push_back({ Panel{static_cast<int>(distance - 75), L"Timber"}, 1 });
            setup.push_back({ Panel{30, L"128284X"}, 1 });
        }
        else if (distance == 90) {
            setup.push_back({ Panel{45, L"128283X"}, 2 });
        }
        else if (distance >= 91 && distance <= 94) {
            setup.push_back({ Panel{45, L"128283X"}, 2 });
            setup.push_back({ Panel{static_cast<int>(distance - 90), L"Timber"}, 1 });
        }
        else if (distance == 95) {
            setup.push_back({ Panel{45, L"128283X"}, 1 });
            setup.push_back({ Panel{5, L"128287X"}, 1 });
            setup.push_back({ Panel{45, L"128283X"}, 1 });
        }
        else if (distance >= 96 && distance <= 99) {
            setup.push_back({ Panel{45, L"128283X"}, 2 });
            setup.push_back({ Panel{static_cast<int>(distance - 90), L"Timber"}, 1 });
        }
        else if (distance == 100) {
            setup.push_back({ Panel{45, L"128283X"}, 2 });
            setup.push_back({ Panel{10, L"128287X"}, 1 });
        }
        else {
            acutPrintf(_T("Error, case not recognized"));
            continue;
        }

        // Place the determined panels
        AcGePoint3d currentPosition = start;
        for (const auto& item : setup) {
            for (int j = 0; j < item.second; ++j) {
                if (item.first.id == L"Timber") {
                    AcDbObjectId timberAssetId = TimberAssetCreator::createTimberAsset(item.first.length, 1.0);  // Adjust height as needed
                    if (timberAssetId != AcDbObjectId::kNull) {
                        placePanel(currentPosition, L"Timber", rotation);
                    }
                }
                else {
                    placePanel(currentPosition, item.first.id, rotation);
                }
                currentPosition += direction * item.first.length;  // Move to the next position
            }
        }
    }
}

// Main function to handle the detection and placement process
void SpecialCaseCorners::handleSpecialCases() {
    std::vector<AcGePoint3d> corners = detectPolylines();
    if (!corners.empty()) {
        determineAndPlacePanels(corners);
    }
}
