// Created by: Ani (2024-07-13)
// Modified by:
// TODO: Write the sub-function to select which tie to place, can refer to WallAssetPlacer.cpp for reference
// TiePlacer.cpp
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
#include <array>

const double TOLERANCE = 0.1; //Define a small tolerance for angle comparisons

// Threshold for comparing positions (adjust as needed)
const double POSITION_THRESHOLD = 0.0;

// Function to check if both coordinates have changed significantly
bool isSignificantChange(const AcGePoint3d& pos1, const AcGePoint3d& pos2, double threshold) {
    acutPrintf(_T("\n--------------------"));
    acutPrintf(_T(" x pos difference: %f"), abs(pos1.x - pos2.x));
    acutPrintf(_T(" y pos difference: %f"), abs(pos1.y - pos2.y));
    return (std::abs(pos1.x - pos2.x) >= threshold) && (std::abs(pos1.y - pos2.y) >= threshold);
}

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
    bool innerLoopDetected = false;
    AcGePoint3d previousPanelPosition;
    bool firstPanel = true;

    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        entityCount++;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            //acutPrintf(_T("\nEntity %d type: %s"), entityCount, pEnt->isA()->name());
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
                        //acutPrintf(_T("\nDetected block name: %s"), blockNameStr.c_str());

                        // Compare with assets list
                        if (blockNameStr == ASSET_128280 || blockNameStr == ASSET_128285 ||
                            blockNameStr == ASSET_128286 || blockNameStr == ASSET_128281 ||
                            blockNameStr == ASSET_128283 || blockNameStr == ASSET_128284 ||
                            blockNameStr == ASSET_129837 || blockNameStr == ASSET_129838 ||
                            blockNameStr == ASSET_129839 || blockNameStr == ASSET_129840 ||
                            blockNameStr == ASSET_129841 || blockNameStr == ASSET_129842 ||
                            blockNameStr == ASSET_129864 || blockNameStr == ASSET_128282) {
                            //positions.emplace_back(pBlockRef->position(), blockNameStr, pBlockRef->rotation());
                            
                            AcGePoint3d currentPosition = pBlockRef->position();

                            if (firstPanel) {
                                // Save the position of the first panel
                                previousPanelPosition = currentPosition;
                                firstPanel = false;
                            }
                            else {

                                if (innerLoopDetected) {
                                    // Save inner loop positions
                                    positions.emplace_back(currentPosition, blockNameStr, pBlockRef->rotation());
                                }
                                else if (isSignificantChange(currentPosition, previousPanelPosition, POSITION_THRESHOLD)) {
                                    // Significant change detected, start saving inner loop positions
                                    innerLoopDetected = true;
                                    acutPrintf(_T("\n innerLoopDetected."));
                                }

                                // Update previous panel position
                                previousPanelPosition = currentPosition;
                            }
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
    acutPrintf(L"\n Debug: calculateTiePositions");
    //Define offsets here
    double xOffset = 20.0; //Define the x offset
    double yOffset = 2.5; //Define the y offset
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

            //Adjust positions based on the rotation and apply any offset if required
            switch (static_cast<int>(round(rotation / M_PI_2))) {
            case 1: //90 degrees (top)
				tiePos.x += yOffset;
                tiePos.y += xOffset;
                tiePos.z += zOffset[i];
                break;
            case 2: //180 degrees(left)
                tiePos.x -= xOffset;
				tiePos.y -= yOffset;
				tiePos.z += zOffset[i];
				break;
            case 3: //270 degrees (botom)
                tiePos.x -= yOffset;
                tiePos.y -= xOffset;
                tiePos.z += zOffset[i];
                break;
            case 4: //360 degrees(right)
				tiePos.x += xOffset;
				tiePos.y -= yOffset;
				tiePos.z += zOffset[i];
				break;
            default:
                acutPrintf(_T("\nInvalid rotation angle: %f"), rotation);
				break;
            }
            //Print Debug Information
            //acutPrintf(_T("\nTie position: (%f, %f, %f)"), tiePos.x, tiePos.y, tiePos.z);
            //acutPrintf(_T("\nTie rotation: %f"), rotation);

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

    // Print the values of position, assetId, and rotation
    //acutPrintf(L"\nPosition: (%f, %f, %f)", position.x, position.y, position.z);
    //acutPrintf(L"\nRotation: %f", rotation);
    //acutPrintf(L"\nAsset ID: %llu", static_cast<unsigned long long>(assetId.asOldId()));

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(assetId);
    pBlockRef->setRotation(rotation);
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  //Set the scale factor

    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
		acutPrintf(_T("\nFailed to append block reference."));
	}
    else {
        //acutPrintf(_T("\nFailed to place tie."));
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