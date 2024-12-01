





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

const double TOLERANCE = 0.1; 

const std::vector<std::wstring> panelNames = {
	ASSET_129842, ASSET_128285
};


double get15Panel(const std::wstring& panelName) {
    static const std::map<std::wstring, double> panelWidthMap = {
        {ASSET_129842, 150.0},
        {ASSET_128285, 150.0}
    };
    auto it = panelWidthMap.find(panelName);
    if (it != panelWidthMap.end()) {
        return it->second;
    }
    
    return 0.0;
}


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


AcDbObjectId Stacked15PanelConnector::loadConnectorAsset(const wchar_t* blockName) {
    
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
    
	return blockId;
}


std::vector<std::tuple<AcGePoint3d, double, double, double, double>> Stacked15PanelConnector::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions;

    double xOffset = 75.0; 
    double yOffset = 50.0; 
    double connectorRotation = M_PI_2; 

    for (const auto& panelPosition : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPosition);
        std::wstring panelName = std::get<1>(panelPosition);
        double panelRotation = std::get<2>(panelPosition);

        double panelWidth = get15Panel(panelName);

        
        if (pos.z == 0.0) {
			continue;
		}

        
        AcGePoint3d connectorPos = pos;
        AcGePoint3d nutPos = pos;

        
        double rotationXConnector = 0.0;
        double rotationYConnector = 0.0;
        double rotationZConnector = 0.0;
        double rotationXNut = 0.0;
        double rotationYNut = 0.0;
        double rotationZNut = 0.0;

        switch (static_cast<int>(round(panelRotation / M_PI_2))) {
        case 0: 
        case 4: 
            connectorPos.x += xOffset;
            connectorPos.y -= yOffset;
            nutPos.x += xOffset;
            nutPos.y -= yOffset;
            rotationXConnector = M_PI;
            rotationXNut = M_PI;
            break;
        case 1: 
            connectorPos.x += yOffset;
            connectorPos.y += xOffset;
            nutPos.x += yOffset;
            nutPos.y += xOffset;
            rotationXConnector = M_PI;
            rotationXNut = M_PI;
            break;
        case 2: 
            connectorPos.x -= xOffset;
            connectorPos.y += yOffset;
            nutPos.x -= xOffset;
            nutPos.y += yOffset;
            rotationXConnector = M_PI;
            rotationXNut = M_PI;
            break;
        case 3: 
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

    
    rotateAroundXAxis(pBlockRef, rotationX);
    rotateAroundYAxis(pBlockRef, rotationY);
    rotateAroundZAxis(pBlockRef, rotationZ);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale)); 

    if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
        
    }
    else {
		acutPrintf(_T("\nFailed to place connector."));
	}
    pBlockRef->close();
	pModelSpace->close();
	pBlockTable->close();
}


void Stacked15PanelConnector::place15panelConnectors() {
    
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(_T("\nNo wall panels detected."));
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId connectorAssetId = loadConnectorAsset(ASSET_128254.c_str()); 
    AcDbObjectId nutAssetId = loadConnectorAsset(ASSET_128256.c_str()); 

    if (connectorAssetId == AcDbObjectId::kNull || nutAssetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (size_t i = 0; i < connectorPositions.size(); i += 2) {
        placeConnectorAtPosition(std::get<0>(connectorPositions[i]), std::get<1>(connectorPositions[i]), std::get<2>(connectorPositions[i]), std::get<3>(connectorPositions[i]), std::get<4>(connectorPositions[i]), connectorAssetId);
        placeConnectorAtPosition(std::get<0>(connectorPositions[i + 1]), std::get<1>(connectorPositions[i + 1]), std::get<2>(connectorPositions[i + 1]), std::get<3>(connectorPositions[i + 1]), std::get<4>(connectorPositions[i + 1]), nutAssetId);
    }

    
}