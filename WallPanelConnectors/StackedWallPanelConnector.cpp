// Created by: Ani (2024-07-16)
// Modified by: 
// TODO: 
// StackedWallPanelConnector.cpp
// Place the connectors for the stacked wall panels
// Calculate the position of the connectors for the stacked wall panels
// two connectors are required for a pair of stacked wall panels
// they are placed on the same z-axis as the stacked wall panels,the x-axis is offset by 5 units and the y-axis is offset by 7.5 units for first connector and second connector is offset depending on the width of the wall panel,
// the second offset is width of the wall panel - 7.5 units
// the connectors are rotated by 90 degrees each depending on the side of the wall panel
// the connectors are vertical by default they need to be made horizontal to match with the wall panel's hole.
// the flow of the code is as follows:
// Get Wall Panel Positions
// Calculate the position of the connectors for the stacked wall panels
// Load the connectors into the drawing
// Place the connectors in the drawing
/////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "StackedWallPanelConnector.h"
#include "SharedDefinations.h"  // For M_PI constants
#include "DefineScale.h"       // For globalVarScale
#include "AssetPlacer/GeometryUtils.h"
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>            // For std::transform
#include "dbapserv.h"        // For acdbHostApplicationServices() and related services
#include "dbents.h"          // For AcDbBlockReference
#include "dbsymtb.h"         // For block table record definitions
#include "AcDb.h"            // General database definitions

const double TOLERANCE = 0.1; // Tolerance for comparing double values

// Define the assets for the stacked wall panels
const std::vector<std::wstring> panelNames = {
    ASSET_128280,
    ASSET_129840,
    ASSET_129838,
    ASSET_128283,
    ASSET_128281,
    ASSET_129841,
    ASSET_129839,
    ASSET_129837,
    ASSET_128284,
    ASSET_128282,
    ASSET_136096,
};

// Define the function to get the width of the panel based on its name
double getPanelWidth(const std::wstring& panelName) {
    static std::map<std::wstring, double> panelWidthMap = {
        {ASSET_128280, 900.0},
        {ASSET_129840, 450.0},
        {ASSET_129838, 750.0},
        {ASSET_128283, 450.0},
        {ASSET_128281, 750.0},
        {ASSET_129841, 300.0},
        {ASSET_129839, 600.0},
        {ASSET_129837, 900.0},
        {ASSET_128284, 300.0},
        {ASSET_128282, 600.0},
        {ASSET_136096, 600.0},
    };

    auto it = panelWidthMap.find(panelName);
    if (it != panelWidthMap.end()) {
        return it->second;
    }

    // Default width if the panel name is not found
    return 0.0;
}

// GET WALL PANEL POSITIONS
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> StackedWallPanelConnectors::getWallPanelPositions() {
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

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbBlockReference::desc())) {
                AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
                if (pBlockRef) {
                    AcDbObjectId blockId = pBlockRef->blockTableRecord();
                    AcDbBlockTableRecord* pBlockDef;
                    if (acdbOpenObject(pBlockDef, blockId, AcDb::kForRead) == Acad::eOk) {
                        const wchar_t* blockName;
                        pBlockDef->getName(blockName);
                        std::wstring blockNameStr(blockName);
                        blockNameStr = toUpperCase(blockNameStr);

                        // Compare with assets list
                        if (std::find(panelNames.begin(), panelNames.end(), blockNameStr) != panelNames.end()) {
                            positions.emplace_back(pBlockRef->position(), blockNameStr, pBlockRef->rotation());
                        }
                        pBlockDef->close();
                    }
                }
            }
            pEnt->close();
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    return positions;
}

// LOAD CONNECTOR ASSET
AcDbObjectId StackedWallPanelConnectors::loadConnectorAsset(const wchar_t* blockName) {
    //acutPrintf(_T("\nLoading asset: %s"), blockName); // Debug information
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        acutPrintf(_T("\nBlock not found: %s"), blockName);
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    //acutPrintf(_T("\nLoaded block: %s"), blockName); // Debug information
    return blockId;
}

// CALCULATE CONNECTOR POSITIONS
std::vector<std::tuple<AcGePoint3d, double, double, double, double>> StackedWallPanelConnectors::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions;

    double xOffset = 50.0; // X offset for the connectors
    double yOffset = 75.0; // Y offset for the connectors
    double connectorRotation = M_PI_2; // Rotation for the connectors


    for (const auto& panelPosition : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPosition);
        std::wstring panelName = std::get<1>(panelPosition);
        double panelRotation = std::get<2>(panelPosition);

        double panelWidth = getPanelWidth(panelName); // Fetch the panel width

        // Skip placing connectors at z-axis 0
        if (pos.z == 0) {
            //acutPrintf(_T("\nSkipping connector at z-axis 0\n")); // Debug information
            continue;
        }

        // Adjust positions based on the panel rotation
        AcGePoint3d connectorPos1 = pos;
        AcGePoint3d connectorPos2 = pos;

        //Special Rotations function if required copy to switch case and change the values
        double rotationXConnector1 = 0.0;
        double rotationXConnector2 = 0.0;
        double rotationYConnector1 = 0.0;
        double rotationYConnector2 = 0.0;
        double rotationZConnector1 = 0.0;
        double rotationZConnector2 = 0.0;

        switch (static_cast<int>(round(panelRotation / M_PI_2))) {
        case 0: // 0 degrees
        case 4: // Normalize 360 degrees to 0 degrees
            connectorPos1.x += yOffset;
            connectorPos1.y -= xOffset;
            connectorPos2.y -= xOffset;
            connectorPos2.x += panelWidth - yOffset;
            rotationYConnector1 += M_3PI_2;
            rotationYConnector2 += M_PI_2;
            break;
        case 1: // 90 degrees
            connectorPos1.x += xOffset;
            connectorPos1.y += yOffset;
            connectorPos2.x += xOffset;
            connectorPos2.y += panelWidth - yOffset;
            rotationXConnector1 += M_PI_2;
            rotationYConnector1 += M_3PI_2;
            rotationYConnector2 += M_PI_2;
            rotationZConnector2 += M_PI;
            break;
        case 2: // 180 degrees
            connectorPos1.x -= yOffset;
            connectorPos1.y += xOffset;
            connectorPos2.y += xOffset;
            connectorPos2.x -= panelWidth - yOffset;
            rotationXConnector1 += M_PI;
            rotationYConnector1 += M_3PI_2;
            rotationYConnector2 += M_PI_2;
            break;
        case 3: // 270 degrees
        case -1:
            connectorPos1.x -= xOffset;
            connectorPos1.y -= yOffset;
            connectorPos2.x -= xOffset;
            connectorPos2.y -= panelWidth - yOffset;
            rotationXConnector1 += M_3PI_2;
            rotationYConnector1 += M_3PI_2;
            rotationYConnector2 += M_PI_2;
            rotationZConnector2 += M_PI;

            break;
        default:
            acutPrintf(_T("\nInvalid rotation angle detected: %f "), panelRotation);
            continue;
        }

        // Print debug information
        //acutPrintf(_T("\nConnector 1 calculated:\n"));
        //acutPrintf(_T("Position: (%f, %f, %f)\n"), connectorPos1.x, connectorPos1.y, connectorPos1.z);
        //acutPrintf(_T("Panel: %s\n"), panelName.c_str());
        //acutPrintf(_T("Rotation: %f radians\n"), panelRotation);

        //acutPrintf(_T("\nConnector 2 calculated:\n"));
        //acutPrintf(_T("Position: (%f, %f, %f)\n"), connectorPos2.x, connectorPos2.y, connectorPos2.z);
        //acutPrintf(_T("Panel: %s\n"), panelName.c_str());
        //acutPrintf(_T("Rotation: %f radians\n"), panelRotation);

        connectorPositions.emplace_back(std::make_tuple(connectorPos1, rotationXConnector1, rotationYConnector1, rotationZConnector1, panelRotation));
        connectorPositions.emplace_back(std::make_tuple(connectorPos2, rotationXConnector1, rotationYConnector2, rotationZConnector2, panelRotation));
    }

    return connectorPositions;
}

// PLACE CONNECTOR AT POSITION
void StackedWallPanelConnectors::placeConnectorAtPosition(const AcGePoint3d& position, double rotationX, double rotationY, double rotationZ, double panelRotation, AcDbObjectId assetId) {
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

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(assetId);

    //apply special rotation
    rotateAroundXAxis(pBlockRef, rotationX);
    rotateAroundYAxis(pBlockRef, rotationY);
    rotateAroundZAxis(pBlockRef, rotationZ);
       // Apply rotations using AcGeMatrix3d
    //AcGeMatrix3d rotMatrix;
    //rotMatrix.setToRotation(rotationX, AcGeVector3d::kXAxis, position);
    //rotMatrix.postMultBy(AcGeMatrix3d::rotation(rotationY, AcGeVector3d::kYAxis, position));
    //rotMatrix.postMultBy(AcGeMatrix3d::rotation(rotationZ, AcGeVector3d::kZAxis, position));
    //rotMatrix.postMultBy(AcGeMatrix3d::rotation(panelRotation, AcGeVector3d::kZAxis, position));

    //pBlockRef->setBlockTransform(rotMatrix);


    //pBlockRef->setRotation(rotation);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Ensure scaling

    if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
        //acutPrintf(_T("\nConnector placed successfully.")); // Debug information
    }
    else {
        acutPrintf(_T("\nFailed to place connector."));
    }

    pBlockRef->close();  // Decrement reference count
    pModelSpace->close();  // Decrement reference count
    pBlockTable->close();  // Decrement reference count
}

// PLACE STACKED WALL CONNECTORS
void StackedWallPanelConnectors::placeStackedWallConnectors() {
    //acutPrintf(_T("\nPlacing stacked wall connectors...")); // Debug information
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(_T("\nNo wall panels detected."));
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId assetId = loadConnectorAsset(ASSET_128247.c_str());  // Replace with the actual block name

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (const auto& connector : connectorPositions) {
        placeConnectorAtPosition(
            std::get<0>(connector),
            std::get<1>(connector),
            std::get<2>(connector),
            std::get<3>(connector),
            std::get<4>(connector),
            assetId
        );
    }

    //acutPrintf(_T("\nCompleted placing stacked wall connectors.")); // Debug information
}