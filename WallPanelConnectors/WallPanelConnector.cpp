





#include "StdAfx.h"
#include "WallPanelConnector.h"
#include "SharedDefinations.h"  
#include "DefineScale.h"       
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>            
#include "dbapserv.h"        
#include "dbents.h"          
#include "dbsymtb.h"         
#include "AcDb.h"            

const double TOLERANCE = 0.1;  


const std::vector<std::wstring> panelsWithThreeConnectors = {
    ASSET_128285, ASSET_128280, ASSET_128283,
    ASSET_128281, ASSET_128284, ASSET_128282,
    ASSET_128286
};


const std::vector<std::wstring> panelsWithTwoConnectors = {
    ASSET_129840, ASSET_129838, ASSET_129842,
    ASSET_129841, ASSET_129839, ASSET_129837,
    ASSET_129864
};


std::vector<std::tuple<AcGePoint3d, std::wstring, double>> WallPanelConnector::getWallPanelPositions() {
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

    int entityCount = 0;
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        entityCount++;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbBlockReference::desc())) {
                AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
                if (pBlockRef) {
                    AcDbObjectId blockId = pBlockRef->blockTableRecord();
                    AcDbBlockTableRecord* pBlockDef;
                    if (acdbOpenObject(pBlockDef, blockId, AcDb::kForRead) == Acad::eOk) {
                        AcString blockName;
                        pBlockDef->getName(blockName);
                        std::wstring blockNameStr(blockName.kACharPtr());
                        blockNameStr = toUpperCase(blockNameStr);

                        
                        if (std::find(panelsWithThreeConnectors.begin(), panelsWithThreeConnectors.end(), blockNameStr) != panelsWithThreeConnectors.end() ||
                            std::find(panelsWithTwoConnectors.begin(), panelsWithTwoConnectors.end(), blockNameStr) != panelsWithTwoConnectors.end()) {
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


void adjustConnectorPosition(AcGePoint3d& connectorPos, double panelRotation, double xOffset, double yOffset) {
    
    if (fabs(panelRotation - 0.0) < TOLERANCE) {
        connectorPos.x += xOffset;
        
    }
    
    else if (fabs(panelRotation - M_PI_2) < TOLERANCE) {
        
        connectorPos.y += yOffset;
    }
    
    else if (fabs(panelRotation - M_PI) < TOLERANCE) {
        connectorPos.x -= xOffset;
    }
    
    else if (fabs(panelRotation - 3 * M_PI_2) < TOLERANCE) {
        
        connectorPos.y -= yOffset;
    }
}


std::vector<std::tuple<AcGePoint3d, double>> WallPanelConnector::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double>> connectorPositions;

    double zOffsets[] = { 225.0, 525.0, 975.0 }; 
    double yOffset = 50.0;

    for (const auto& panelPosition : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPosition);
        std::wstring panelName = std::get<1>(panelPosition);
        double panelRotation = std::get<2>(panelPosition);

        int connectorCount = (std::find(panelsWithTwoConnectors.begin(), panelsWithTwoConnectors.end(), panelName) != panelsWithTwoConnectors.end()) ? 2 : 3;

        for (int i = 0; i < connectorCount; ++i) {
            AcGePoint3d connectorPos = pos;
            connectorPos.z += zOffsets[i];

            if (panelName == ASSET_128286) {
                
                adjustConnectorPosition(connectorPos, panelRotation, 100.0, 100.0);
            }

            
            switch (static_cast<int>(round(panelRotation / M_PI_2))) {
            case 0: 
            case 4: 
                connectorPos.y -= yOffset;
                break;
            case 1: 
                connectorPos.x += yOffset;
                break;
            case 2: 
                connectorPos.y += yOffset;
                break;
            case 3: 
            case -1: 
                connectorPos.x -= yOffset;
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle detected: %f "), panelRotation);
                continue;
            }

            connectorPositions.emplace_back(std::make_tuple(connectorPos, panelRotation));
        }
    }

    return connectorPositions;
}


AcDbObjectId WallPanelConnector::loadConnectorAsset(const wchar_t* blockName) {
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


void WallPanelConnector::placeConnectors() {
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(_T("\nNo wall panels detected."));
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId assetId = loadConnectorAsset(ASSET_128247.c_str());

    if (assetId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load asset."));
        return;
    }

    for (const auto& connector : connectorPositions) {
        placeConnectorAtPosition(std::get<0>(connector), std::get<1>(connector), assetId);
    }
}


void WallPanelConnector::placeConnectorAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId) {
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
    pBlockRef->setRotation(rotation);
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
