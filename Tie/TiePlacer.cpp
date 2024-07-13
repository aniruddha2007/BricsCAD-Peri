// Created by: Ani (2024-07-13)
// Modified by:
// TODO: Write the sub-function to select which tie to place, can refer to WallAssetPlacer.cpp for reference
// WallPanelConnector.cpp
/////////////////////////////////////////////////////////////////////////

#include "stdAfx.h"
#include "TiePlacer.h"
#include "SharedDefinations.h"  //For the shared definations
#include "DefineScale.h"        //For globalVarScale
#include <vector>               //For the vector
#include <algorithm>            //For the algorithm
#include <tuple>                //For the tuple
#include "dbapserv.h"           // For acdbHostApplicationServices() and related services
#include "dbents.h"             // For AcDbBlockReference
#include "dbsymtb.h"            // For block table record definitions
#include "AcDb.h"               //General database definitions
#include "AssetPlacer/GeometryUtils.h" //For the geometry utilities

const double TOLERANCE = 0.1; //Define a small tolerance for angle comparisons

// GET WALL PANEL POSITIONS
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

    int entityCount = 0;
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        entityCount++;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            acutPrintf(_T("\nEntity %d type: %s"), entityCount, pEnt->isA()->name());
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
                        acutPrintf(_T("\nDetected block name: %s"), blockNameStr.c_str());

                        // Compare with assets list
                        if (blockNameStr == ASSET_128280 || blockNameStr == ASSET_128285 ||
                            blockNameStr == ASSET_128286 || blockNameStr == ASSET_128281 ||
                            blockNameStr == ASSET_128283 || blockNameStr == ASSET_128284 ||
                            blockNameStr == ASSET_129837 || blockNameStr == ASSET_129838 ||
                            blockNameStr == ASSET_129839 || blockNameStr == ASSET_129840 ||
                            blockNameStr == ASSET_129841 || blockNameStr == ASSET_129842 ||
                            blockNameStr == ASSET_129864 || blockNameStr == ASSET_128282) {
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

    acutPrintf(_T("\nTotal entities checked: %d"), entityCount);
    return positions;
}

//Calculate position of the tie
std::vector<std::tuple<AcGePoint3d, double>> calculateTiePositions(const std::vector<std::tuple<AcGePoint3d, std::wstring, double>>& panelPositions) {
	std::vector<std::tuple<AcGePoint3d, double>> tiePositions;
    
    //Define offsets here
    double xOffset = 0.0; //Define the x offset
    double yOffset = 0.0; //Define the y offset
    double zOffset = 0.0; //Define the z offset

    for (const auto& panelPositions : panelPositions) {
        AcGePoint3d pos = std::get<0>(panelPositions);
        std::wstring panelName = std::get<1>(panelPositions);
        double rotation = std::get<2>(panelPositions);

        int tieCount = 0;

        for (int i = 0; i < tieCount; ++i) {
            AcGePoint3d tiePos = pos;

            //Adjust positions based on the rotation and apply any offset if required
            switch (static_cast<int>(round(rotation / M_PI_2))) {
            case 0: //0 degrees
                tiePos.x += xOffset;
				tiePos.y += yOffset;
				tiePos.z += zOffset;
				break;
            case 1: //90 degrees
				tiePos.x += yOffset;
                tiePos.y -= xOffset;
                tiePos.z += zOffset;
                break;
            case 2: //180 degrees
                tiePos.x -= xOffset;
				tiePos.y -= yOffset;
				tiePos.z += zOffset;
				break;
            case 3: //270 degrees
            case -1: //Normalize -90 degrees to 270 degrees
				tiePos.x -= yOffset;
				tiePos.y += xOffset;
				tiePos.z += zOffset;
				break;
            default:
				acutPrintf(_T("\nInvalid rotation angle: %f"), rotation);
				break;
            }
            //Print Debug Information
            acutPrintf(_T("\nTie position: (%f, %f, %f)"), tiePos.x, tiePos.y, tiePos.z);
            acutPrintf(_T("\nTie rotation: %f"), rotation);

            tiePositions.emplace_back(std::make_tuple(tiePos, rotation));

        }
    }

	return tiePositions;
}

//Load Tie Asset
AcDbObjectId TiePlacer::LoadTieAsset(const wchar_t* blockName){
	acutPrintf(L"\nLoading Tie Asset: %s", blockName);
	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
	if (!pDb){
		acutPrintf(L"\nFailed to get the working database");
		return AcDbObjectId::kNull;
	}

	AcDbBlockTable* pBlockTable;
	if (Acad::eOk != pDb->getBlockTable(pBlockTable, AcDb::kForRead)){
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

//Place Tie at Position
void TiePlacer::placeTieAtPosition(const AcGePoint3d& position, double rotation, AcDbObjectId assetId){
	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
	if (!pDb){
		acutPrintf(L"\nFailed to get the working database");
		return;
	}

	AcDbBlockTable* pBlockTable;
	if (Acad::eOk != pDb->getBlockTable(pBlockTable, AcDb::kForRead)){
		acutPrintf(L"\nFailed to get the block table");
		return;
	}

	AcDbBlockTableRecord* pModelSpace;
	if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk){
		acutPrintf(L"\nFailed to get the model space");
		pBlockTable->close();
		return;
	}

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(assetId);
    pBlockRef->setRotation(rotation);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  //Set the scale factor

    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
		acutPrintf(_T("\nFailed to append block reference."));
	}
    else {
        acutPrintf(_T("\nFailed to place tie."));
    }

	pBlockRef->close();
	pModelSpace->close();
	pBlockTable->close();
}

//Place Ties
void TiePlacer::placeTies(){
	acutPrintf(L"\nPlacing Ties");
	std::vector<std::tuple<AcGePoint3d, std::wstring, double>> panelPositions = getWallPanelPositions();
	if (panelPositions.empty()){
		acutPrintf(L"\nNo wall panels found");
		return;
	}

    std::vector<std::tuple<AcGePoint3d, double>> tiePositions = calculateTiePositions(panelPositions);
    AcDbObjectId assetId = LoadTieAsset(ASSET_030005.c_str());  //Replace ASSET_TIE with the actual asset name
    
    if (assetId == AcDbObjectId::kNull){
		acutPrintf(L"\nFailed to load the tie asset");
		return;
	}

    for (const auto& tiePos : tiePositions){
		placeTieAtPosition(std::get<0>(tiePos), std::get<1>(tiePos), assetId);
	}

    acutPrintf(L"\nTies placed successfully");
}