// Created by: Ani (2024-07-13)
// Modified by:
// TODO: Write the sub-function to select which tie to place, can refer to WallAssetPlacer.cpp for reference
// TiePlacer.cpp
/////////////////////////////////////////////////////////////////////////

#include "stdAfx.h"
#include "TiePlacer.h"
#include "SharedDefinations.h"  // For the shared definitions
#include "DefineScale.h"        // For globalVarScale
#include <vector>               // For the vector
#include <algorithm>            // For the algorithm
#include <tuple>                // For the tuple
#include "dbapserv.h"           // For acdbHostApplicationServices() and related services
#include "dbents.h"             // For AcDbBlockReference
#include "dbsymtb.h"            // For block table record definitions
#include "AssetPlacer/GeometryUtils.h" // For the geometry utilities
#include <array>
#include <cmath>

const double TOLERANCE = 0.1; // Define a small tolerance for angle comparisons

// Function to get the vertices of an AcDbPolyline
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

// Function to calculate the distance between corresponding vertices of two polylines
double getPolylineDistance(AcDbPolyline* pPolyline1, AcDbPolyline* pPolyline2) {
    std::vector<AcGePoint3d> vertices1 = getPolylineVertices(pPolyline1);
    std::vector<AcGePoint3d> vertices2 = getPolylineVertices(pPolyline2);

    if (vertices1.size() != vertices2.size()) {
        acutPrintf(_T("\nThe polylines have different numbers of vertices."));
        return -1.0;
    }

    for (size_t i = 0; i < vertices1.size(); ++i) {
        double deltaX = vertices2[i].x - vertices1[i].x;
        double deltaY = vertices2[i].y - vertices1[i].y;

        if (std::abs(deltaX) == std::abs(deltaY)) {
            return std::abs(deltaX); // Return the positive distance value
        }
    }

    return -1.0; // Return -1 if no matching deltas are found
}

// Function to get the wall panel positions and find the distance between polylines
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> TiePlacer::getWallPanelPositions() {
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> positions;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return positions;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return positions;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return positions;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return positions;
    }

    AcDbPolyline* pFirstPolyline = nullptr;
    AcDbPolyline* pSecondPolyline = nullptr;

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                if (pFirstPolyline == nullptr) {
                    pFirstPolyline = AcDbPolyline::cast(pEnt);
                }
                else {
                    pSecondPolyline = AcDbPolyline::cast(pEnt);
                    break; // Found both polylines, no need to continue
                }
            }
            pEnt->close();
        }
    }

    if (pFirstPolyline && pSecondPolyline) {
        double distance = getPolylineDistance(pFirstPolyline, pSecondPolyline);
        if (distance > 0) {
            acutPrintf(_T("\nDistance between polylines: %f"), distance);
        }
        else {
            acutPrintf(_T("\nNo matching deltas found between polylines."));
        }
    }
    else {
        acutPrintf(_T("\nDid not find two polylines."));
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    return positions;
}

// Calculate position of the tie
std::vector<std::tuple<AcGePoint3d, double>> calculateTiePositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double>> tiePositions;
    acutPrintf(L"\n Debug: calculateTiePositions");
    // Define offsets here
    double xOffset = 20.0; // Define the x offset
    double yOffset = 2.5; // Define the y offset
    std::array<double, 2> zOffset = { 30.0, 105.0 }; // Define the z offset array

    for (const auto& panelPositions : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPositions);
        std::wstring panelName = std::get<1>(panelPositions);
        double rotation = std::get<2>(panelPositions);

        // Adjust rotation by adding 90 degrees (pi/2 radians)
        rotation += M_PI_2;

        int tieCount = 2;

        for (int i = 0; i < tieCount; ++i) {
            AcGePoint3d tiePos = pos;

            // Adjust positions based on the rotation and apply any offset if required
            switch (static_cast<int>(round(rotation / M_PI_2))) {
            case 1: // 90 degrees (top)
                tiePos.x += yOffset;
                tiePos.y += xOffset;
                tiePos.z += zOffset[i];
                break;
            case 2: // 180 degrees(left)
                tiePos.x -= xOffset;
                tiePos.y -= yOffset;
                tiePos.z += zOffset[i];
                break;
            case 3: // 270 degrees (bottom)
                tiePos.x -= yOffset;
                tiePos.y -= xOffset;
                tiePos.z += zOffset[i];
                break;
            case 4: // 360 degrees(right)
                tiePos.x += xOffset;
                tiePos.y -= yOffset;
                tiePos.z += zOffset[i];
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle: %f"), rotation);
                break;
            }
            // Print Debug Information
            // acutPrintf(_T("\nTie position: (%f, %f, %f)"), tiePos.x, tiePos.y, tiePos.z);
            // acutPrintf(_T("\nTie rotation: %f"), rotation);

            tiePositions.emplace_back(std::make_tuple(tiePos, rotation));
        }
    }

    return tiePositions;
}

// Load Tie Asset
AcDbObjectId TiePlacer::LoadTieAsset(const wchar_t* blockName) {
    acutPrintf(L"\nLoading Tie Asset: %s", blockName);
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(L"\nFailed to get the working database");
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    if (Acad::eOk != pDb->getBlockTable(pBlockTable, AcDb::kForRead)) {
        acutPrintf(L"\nFailed to get the block table");
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        acutPrintf(_T("\nBlock not found %s"), blockName);
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    acutPrintf(L"\nLoaded block: %s", blockName);
    return blockId;
}

// Place Tie at Position
void TiePlacer::placeTieAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(L"\nFailed to get the working database");
        return;
    }

    AcDbBlockTable* pBlockTable;
    if (Acad::eOk != pDb->getBlockTable(pBlockTable, AcDb::kForRead)) {
        acutPrintf(L"\nFailed to get the block table");
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(L"\nFailed to get the model space");
        pBlockTable->close();
        return;
    }

    // Print the values of position, assetId, and rotation
    // acutPrintf(L"\nPosition: (%f, %f, %f)", position.x, position.y, position.z);
    // acutPrintf(L"\nRotation: %f", rotation);
    // acutPrintf(L"\nAsset ID: %llu", static_cast<unsigned long long>(assetId.asOldId()));

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(assetId);
    pBlockRef->setRotation(rotation);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Set the scale factor

    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
        acutPrintf(_T("\nFailed to append block reference."));
    }
    else {
        // acutPrintf(_T("\nFailed to place tie."));
    }

    pBlockRef->close();
    pModelSpace->close();
    pBlockTable->close();
}

// Place Ties
void TiePlacer::placeTies() {
    acutPrintf(L"\nPlacing Ties");
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(L"\nNo wall panels found");
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double>> tiePositions = calculateTiePositions(panelPositions);
    AcDbObjectId assetId = LoadTieAsset(ASSET_030005.c_str());  // Replace ASSET_TIE with the actual asset name

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(L"\nFailed to load the tie asset");
        return;
    }

    for (const auto& tiePos : tiePositions) {
        placeTieAtPosition(std::get<0>(tiePos), std::get<1>(tiePos), assetId);
    }

    acutPrintf(L"\nTies placed successfully");
}
