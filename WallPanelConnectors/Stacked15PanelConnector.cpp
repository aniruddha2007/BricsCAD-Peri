// Created by: Ani (2024-07-16)
// Modified by: 
// TODO: 
// Stacked15PanelConnector.cpp
// 
/////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "Stacked15PanelConnector.h"
#include "SharedDefinations.h"
#include "DefineScale.h"
#include "AssetPlacer/GeometryUtils.h"
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include <map>
#include <string>

const double TOLERANCE = 0.1; // Define a small tolerance for angle comparisons

const std::vector<std::wstring> panelNames = {
	ASSET_129842, ASSET_128285
};

// Define the function to get the width of the panel based on its name
double get15Panel(const std::wstring& panelName) {
    static const std::map<std::wstring, double> panelWidthMap = {
        {ASSET_129842, 15.0},
        {ASSET_128285, 15.0}
    };
    auto it = panelWidthMap.find(panelName);
    if (it != panelWidthMap.end()) {
        return it->second;
    }
    // Handle case where panelName is not found
    return 0.0;
}

// GET WALL PANEL POSITIONS
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> Stacked15PanelConnector::getWallPanelPositions() {
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
AcDbObjectId Stacked15PanelConnector::loadConnectorAsset(const wchar_t* blockName) {
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
std::vector<std::tuple<AcGePoint3d, double, double, double, double>> Stacked15PanelConnector::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions;

    double xOffset = 7.5; // Offset in X direction
    double yOffset = 5.0; // Offset in Y direction
    double connectorRotation = M_PI_2; // Rotation of the connector

    for (const auto& panelPosition : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPosition);
        std::wstring panelName = std::get<1>(panelPosition);
        double panelRotation = std::get<2>(panelPosition);

        double panelWidth = get15Panel(panelName);

        //Skipping placing connectors at z-axis 0
        if (pos.z == 0.0) {
			continue;
		}

        //Adjusting the position of the connector based on the panel width
        AcGePoint3d connectorPos = pos;
        AcGePoint3d nutPos = pos;

        //Special Rotation Function
        double rotationXConnector = 0.0;
        double rotationYConnector = 0.0;
        double rotationZConnector = 0.0;
        double rotationXNut = 0.0;
        double rotationYNut = 0.0;
        double rotationZNut = 0.0;

        switch (static_cast<int>(round(panelRotation / M_PI_2))) {
        case 0: // 0 degrees
        case 4: // Normalize 360 degrees to 0 degrees
            connectorPos.x += xOffset;
            connectorPos.y -= yOffset;
            nutPos.x += xOffset;
            nutPos.y -= yOffset;
            rotationXConnector = M_PI;
            rotationXNut = M_PI;
            break;
        case 1: // 90 degrees
            connectorPos.x += yOffset;
            connectorPos.y += xOffset;
            nutPos.x += yOffset;
            nutPos.y += xOffset;
            rotationXConnector = M_PI;
            rotationXNut = M_PI;
            break;
        case 2: // 180 degrees
            connectorPos.x -= xOffset;
            connectorPos.y += yOffset;
            nutPos.x -= xOffset;
            nutPos.y += yOffset;
            rotationXConnector = M_PI;
            rotationXNut = M_PI;
            break;
        case 3: // 270 degrees
        case -1:
            connectorPos.x -= yOffset;
            connectorPos.y -= xOffset;
            nutPos.x -= yOffset;
            nutPos.y -= xOffset;
            rotationXConnector = M_PI;
            rotationXNut = M_PI;
            break;
        default:
            acutPrintf(_T("\nInvalid rotation angle detected: %f "), panelRotation);
            continue;
        }

        connectorPositions.emplace_back(std::make_tuple(connectorPos, rotationXConnector, rotationYConnector, rotationZConnector, panelRotation));
        connectorPositions.emplace_back(std::make_tuple(nutPos, rotationXNut, rotationYNut, rotationZNut, panelRotation));
    }

    return connectorPositions;
}

// PLACE CONNECTOR AT POSITION
void Stacked15PanelConnector::placeConnectorAtPosition(const AcGePoint3d& position, double rotationX, double rotationY, double rotationZ, double panelRotation, AcDbObjectId assetId) {
    AcDbDatabase*pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
		acutPrintf(_T("\nNo working database found."));
		return;
	}

    AcDbBlockTable* pBlockTable;
    if(pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
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
    pBlockRef->setBlockTableRecord(assetId);
    pBlockRef->setPosition(position);

    //Apply Special Rotation
    rotateAroundXAxis(pBlockRef, rotationX);
    rotateAroundYAxis(pBlockRef, rotationY);
    rotateAroundZAxis(pBlockRef, rotationZ);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale)); // Set the scale factor

    if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
        //acutPrintf(_T("\nConnector placed at (%f, %f, %f)"), position.x, position.y, position.z); // Debug information
    }
    else {
		acutPrintf(_T("\nFailed to place connector."));
	}
    pBlockRef->close();
	pModelSpace->close();
	pBlockTable->close();
}

// PLACE CONNECTORS
void Stacked15PanelConnector::place15panelConnectors() {
    //acutPrintf(_T("\nPlacing connectors...")); // Debug information
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(_T("\nNo wall panels detected."));
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId connectorAssetId = loadConnectorAsset(ASSET_128254.c_str()); // Connector block name
    AcDbObjectId nutAssetId = loadConnectorAsset(ASSET_128256.c_str()); // Nut block name

    if (connectorAssetId == AcDbObjectId::kNull || nutAssetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (size_t i = 0; i < connectorPositions.size(); i += 2) {
        placeConnectorAtPosition(std::get<0>(connectorPositions[i]), std::get<1>(connectorPositions[i]), std::get<2>(connectorPositions[i]), std::get<3>(connectorPositions[i]), std::get<4>(connectorPositions[i]), connectorAssetId);
        placeConnectorAtPosition(std::get<0>(connectorPositions[i + 1]), std::get<1>(connectorPositions[i + 1]), std::get<2>(connectorPositions[i + 1]), std::get<3>(connectorPositions[i + 1]), std::get<4>(connectorPositions[i + 1]), nutAssetId);
    }

    //acutPrintf(_T("\nCompleted placing connectors.")); // Debug information
}