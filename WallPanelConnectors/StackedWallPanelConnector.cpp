

















#include "StdAfx.h"
#include "StackedWallPanelConnector.h"
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

const double TOLERANCE = 0.1; 


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

    
    return 0.0;
}


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


AcDbObjectId StackedWallPanelConnectors::loadConnectorAsset(const wchar_t* blockName) {
    
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


std::vector<std::tuple<AcGePoint3d, double, double, double, double>> StackedWallPanelConnectors::calculateConnectorPositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions;

    double xOffset = 50.0; 
    double yOffset = 75.0; 
    double connectorRotation = M_PI_2; 


    for (const auto& panelPosition : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPosition);
        std::wstring panelName = std::get<1>(panelPosition);
        double panelRotation = std::get<2>(panelPosition);

        double panelWidth = getPanelWidth(panelName); 

        
        if (pos.z == 0) {
            
            continue;
        }

        
        AcGePoint3d connectorPos1 = pos;
        AcGePoint3d connectorPos2 = pos;

        
        double rotationXConnector1 = 0.0;
        double rotationXConnector2 = 0.0;
        double rotationYConnector1 = 0.0;
        double rotationYConnector2 = 0.0;
        double rotationZConnector1 = 0.0;
        double rotationZConnector2 = 0.0;

        switch (static_cast<int>(round(panelRotation / M_PI_2))) {
        case 0: 
        case 4: 
            connectorPos1.x += yOffset;
            connectorPos1.y -= xOffset;
            connectorPos2.y -= xOffset;
            connectorPos2.x += panelWidth - yOffset;
            rotationYConnector1 += M_3PI_2;
            rotationYConnector2 += M_PI_2;
            break;
        case 1: 
            connectorPos1.x += xOffset;
            connectorPos1.y += yOffset;
            connectorPos2.x += xOffset;
            connectorPos2.y += panelWidth - yOffset;
            rotationXConnector1 += M_PI_2;
            rotationYConnector1 += M_3PI_2;
            rotationYConnector2 += M_PI_2;
            rotationZConnector2 += M_PI;
            break;
        case 2: 
            connectorPos1.x -= yOffset;
            connectorPos1.y += xOffset;
            connectorPos2.y += xOffset;
            connectorPos2.x -= panelWidth - yOffset;
            rotationXConnector1 += M_PI;
            rotationYConnector1 += M_3PI_2;
            rotationYConnector2 += M_PI_2;
            break;
        case 3: 
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

        
        
        
        
        

        
        
        
        

        connectorPositions.emplace_back(std::make_tuple(connectorPos1, rotationXConnector1, rotationYConnector1, rotationZConnector1, panelRotation));
        connectorPositions.emplace_back(std::make_tuple(connectorPos2, rotationXConnector1, rotationYConnector2, rotationZConnector2, panelRotation));
    }

    return connectorPositions;
}


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


void StackedWallPanelConnectors::placeStackedWallConnectors() {
    
    std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
    if (panelPositions.empty()) {
        acutPrintf(_T("\nNo wall panels detected."));
        return;
    }

    std::vector<std::tuple<AcGePoint3d, double, double, double, double>> connectorPositions = calculateConnectorPositions(panelPositions);
    AcDbObjectId assetId = loadConnectorAsset(ASSET_128247.c_str());  

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

    
}