// Purpose: Implementation of the Props functions.
#include "StdAfx.h"
#include <vector>
#include <set>
#include <cmath>
#include <limits>
#include <chrono>
#include <thread>
#include <dbapserv.h>
#include <acutads.h>
#include <fstream> // For file handling
#include <iostream> // For console output
#include <string> // For string operations
#include <iterator>
#include <map>
#include "Props.h"
#include "SharedDefinations.h"
#include "AssetPlacer/GeometryUtils.h"
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include <AcDb/AcDbBlockTable.h>
#include <AcDb/AcDbBlockTableRecord.h>
#include <AcDb/AcDbAttribute.h>
#include <AcDb/AcDbPolyline.h>
#include <AcDb/AcDbDynBlockReference.h>
#include "aced.h"
#include "gepnt3d.h"
#include "DefineHeight.h"
#include "DefineScale.h" 
#include <nlohmann/json.hpp> // Include nlohmann JSON header
#include "AcDb/AcDbSmartObjectPointer.h"  
#include <Windows.h>
//#define PROPS_FILE_NAME "props.json"

using json = nlohmann::json;

std::map<AcGePoint3d, std::vector<AcGePoint3d>, PlaceProps::Point3dComparator> PlaceProps::wallMap;

const int BATCH_SIZE = 1000; // Batch size for processing entities

const double TOLERANCE = 0.1; // Tolerance for comparing angles

static bool isIntegerProps(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

//Detect polylines
std::vector<AcGePoint3d> PlaceProps::detectPolylines() {
    //acutPrintf(_T("\nDetecting polylines..."));
    std::vector<AcGePoint3d> corners;
    wallMap.clear();  // Clear previous data

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return corners;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table. Error status: %d\n"), es);
        return corners;
    }

    AcDbBlockTableRecord* pModelSpace;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space. Error status: %d\n"), es);
        pBlockTable->close();
        return corners;
    }

    AcDbBlockTableRecordIterator* pIter;
    es = pModelSpace->newIterator(pIter);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator. Error status: %d\n"), es);
        pModelSpace->close();
        pBlockTable->close();
        return corners;
    }

    int entityCount = 0;
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        es = pIter->getEntity(pEnt, AcDb::kForRead);
        if (es == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPolyline = AcDbPolyline::cast(pEnt);
                if (pPolyline) {
                    processPolyline(pPolyline, corners, 90.0, TOLERANCE);  // Assuming 90.0 degrees as the threshold for corners
                }
            }
            pEnt->close();
            entityCount++;

            if (entityCount % BATCH_SIZE == 0) {
                acutPrintf(_T("\nProcessed %d entities. Pausing to avoid resource exhaustion.\n"), entityCount);
                std::this_thread::sleep_for(std::chrono::seconds(1));  // Pause for a moment
            }
        }
        else {
            acutPrintf(_T("\nFailed to get entity. Error status: %d\n"), es);
        }
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    //acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;

}

double crossProductProps(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool directionOfDrawingProps(std::vector<AcGePoint3d>& points) {
    // Ensure the shape is closed
    if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
        points.push_back(points.front());
    }

    double totalTurns = 0.0;

    for (size_t i = 1; i < points.size() - 1; ++i) {
        totalTurns += crossProductProps(points[i - 1], points[i], points[i + 1]);
    }

    // If totalTurns is negative, the shape is drawn clockwise
    if (totalTurns < 0) {
        return true;  // Clockwise
    }
    // If totalTurns is positive, the shape is drawn counterclockwise
    else if (totalTurns > 0) {
        return false; // Counterclockwise
    }
    // Handle the case where totalTurns is zero (indicating an undefined direction)
    else {
        acutPrintf(_T("Warning: The shape does not have a defined direction. Defaulting to clockwise.\n"));
        return true;  // Default to clockwise if direction cannot be determined
    }
}

struct BlockInfoProps {
    AcGePoint3d position;
    std::wstring blockName;
    double rotation;

	// Default constructor
	BlockInfoProps() : position(AcGePoint3d()), blockName(L""), rotation(0.0) {}

    // Parameterized constructor
    BlockInfoProps(const AcGePoint3d& pos, const std::wstring& name, double rot)
        : position(pos), blockName(name), rotation(rot) {}
};

// Function to get the positions of wall panels need to modify the assets to look for and what data to extract currently only has the positions
std::vector<std::tuple<AcGePoint3d, std::wstring, double>> PlaceProps::getWallPanelPositions() {
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
                        const wchar_t* blockName;
                        pBlockDef->getName(blockName);
                        std::wstring blockNameStr(blockName);
                        blockNameStr = toUpperCase(blockNameStr);

                        // Compare with assets list
                        if (blockNameStr == ASSET_128292 || blockNameStr == ASSET_129884) {
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

std::wstring getBlockNameProps(AcDbObjectId blockId) {
    AcDbBlockTableRecord* pBlockRec = nullptr;
    if (acdbOpenObject(pBlockRec, blockId, AcDb::kForRead) != Acad::eOk) {
        return L"";
    }

    ACHAR* name = nullptr;
    if (pBlockRec->getName(name) != Acad::eOk) {
        pBlockRec->close();
        return L"";
    }

    std::wstring blockName(name);

    pBlockRec->close();
    acutDelString(name);
    return blockName;
}

// Function to get the information of selected blocks by User
std::vector<BlockInfoProps> getSelectedBlocksInfo() {
    std::vector<BlockInfoProps> BlockInfoProps;

    // Prompt the user to select block references
    ads_name ss;
    if (acedSSGet(NULL, NULL, NULL, NULL, ss) != RTNORM) {
        acutPrintf(_T("\nNo selection made or invalid selection."));
        return BlockInfoProps;
    }

    // Get the number of selected entities
    long length = 0;
    if (acedSSLength(ss, &length) != RTNORM || length == 0) {
        acutPrintf(_T("\nFailed to get the number of selected entities."));
        acedSSFree(ss);
        return BlockInfoProps;
    }

    // Iterate through the selection set
    for (long i = 0; i < length; ++i) {
        ads_name ent;
        if (acedSSName(ss, i, ent) != RTNORM) {
            acutPrintf(_T("\nFailed to get entity from selection set."));
            continue;
        }

        // Open the selected entity
        AcDbObjectId objId;
        acdbGetObjectId(objId, ent);
        AcDbEntity* pEnt = nullptr;
        if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk) {
            acutPrintf(_T("\nFailed to open entity."));
            continue;
        }

        // Check if the entity is a block reference
        if (pEnt->isKindOf(AcDbBlockReference::desc())) {
            AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
            BlockInfoProp info;
            info.position = pBlockRef->position();
            info.rotation = pBlockRef->rotation();

            // Get the block name
            AcDbObjectId blockTableRecordId = pBlockRef->blockTableRecord();
            AcDbBlockTableRecord* pBlockTableRecord = nullptr;
            if (acdbOpenObject(pBlockTableRecord, blockTableRecordId, AcDb::kForRead) == Acad::eOk) {
                info.blockName = getBlockNameProps(pBlockRef->blockTableRecord());
                pBlockTableRecord->close();
            }
            else {
                acutPrintf(_T("\nFailed to get block table record."));
            }

            //blocksInfo.push_back(info);
            BlockInfoProps.emplace_back(info.position, info.blockName, info.rotation);
        }
        else {
            acutPrintf(_T("\nSelected entity is not a block reference."));
        }

        pEnt->close();
    }

    acedSSFree(ss);
    //print the block info
    //for (const auto& block : BlockInfoProps) {
    //	acutPrintf(_T("\nBlock name: %s"), block.blockName.c_str());
    //	acutPrintf(_T("\nPosition: (%f, %f, %f)"), block.position.x, block.position.y, block.position.z);
    //	acutPrintf(_T("\nRotation: %f"), block.rotation);
    //}
    return BlockInfoProps;
}

// Function to load an asset block from the block table
AcDbObjectId PlaceProps::loadAsset(const wchar_t* blockName) {
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

    AcDbObjectId assetId;
    if (pBlockTable->getAt(blockName, assetId) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table record for block '%s'."), blockName);
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    return assetId;
}

const std::string  PROPS_FILE_NAME = "OneDrive - PERI Group\\Documents\\AP-PeriCAD-Automation-Tools\\[03]Plugin\\props.json";

void PlaceProps::placeProps() {
    //step: 1 detect Polylines
    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo corners detected. Exiting."));
        return;
    }

	//step: 2 detect the direction of drawing
    int closeLoopCounter = -1;
    int loopIndex = 0;
    double outerPointCounter = corners[0].x;
	int outerLoopIndexValue = 0;
    int firstLoopEnd;

	// Pass 1: Determine inner and outer loops
    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[(cornerNum + 1) % corners.size()];  // Wrap around to the first point
        AcGeVector3d direction = (end - start).normal();

        if (start.x > outerPointCounter) {
            outerPointCounter = start.x;
            outerLoopIndexValue = loopIndex;
        }

        if (!isIntegerProps(direction.x) || !isIntegerProps(direction.y)) {
            if (cornerNum < corners.size() - 1) {
                closeLoopCounter = -1;
                loopIndex = 1;
                firstLoopEnd = static_cast<int>(cornerNum);
            }
        }
    }

	if (outerLoopIndexValue == 0) {
		std::vector<AcGePoint3d> firstLoop(corners.begin(), corners.begin() + firstLoopEnd + 1);
        bool firstLoopIsClockwise = directionOfDrawingProps(firstLoop);
	}
    else if (outerLoopIndexValue == 1) {
        std::vector<AcGePoint3d> firstLoop(corners.begin() + firstLoopEnd + 1, corners.end());
		bool firstLoopIsClockwise = directionOfDrawingProps(firstLoop);
    }
    std::vector<BlockInfoProps> BlockInfoProps = getSelectedBlocksInfo();
    if (BlockInfoProps.empty()) {
        acutPrintf(_T("\nNo block references selected."));
        return;
    }

    int i = 0;
    AcGePoint3d start;
    AcGePoint3d end;
    double rotation;
    std::wstring id;

    for (const auto& panel : BlockInfoProps) {
        if (i == 0) {
            start = panel.position;
            rotation = panel.rotation;
            i = 1;
        }
        else {
            end = panel.position;
            id = panel.blockName;
        }
    }

    AcGeVector3d direction = (end - start).normal();

    struct WallPanel {
        AcGePoint3d position;
        AcDbObjectId assetId;
        double rotation;
        double length;
    };

    std::vector<WallPanel> wallPanels;

    int wallHeight = globalVarHeight;
    int panelHeights[] = { 1350, 1200, 600 };

    int numHeights = 3;

    int maxHeight = 0;

    // Iterate through all combinations of panel heights
    for (int i = 0; i < (1 << numHeights); ++i) {
		int currentHeight = 0;
        for (int j = 0; j < numHeights; ++j) {
            if (i & (1 << j)) {
                currentHeight += panelHeights[j];
            }
        }
        if (currentHeight <= wallHeight && currentHeight > maxHeight) {
            maxHeight = currentHeight;
        }
    }

    // Structure to hold panel information
    struct Panel {
        int length;
        std::wstring id[3];
    };

    std::vector<Panel> panelSizes = {
{600, {L"128282X", L"129839X"}},
{450, {L"128283X", L"129840X"}},
{300, {L"128284X", L"129841X"}},
{150, {L"128285X", L"129842X"}},
{100, {L"128292X", L"129884X"}},
{50, {L"128287X", L"129879X"}}
    };

    int lastPanelLength = 0;

    for (const auto& panel : panelSizes) {
        for (const auto& panelId : panel.id) {
            if (panelId == id) {
                lastPanelLength = panel.length;
            }
        }
    }

    double distance = start.distanceTo(end) + lastPanelLength;
    //acutPrintf(_T("\n distance %f"), distance);
    AcGePoint3d currentPoint = start;
    double panelLength;

	//Pass 2: Save all positions, asset IDs, and rotations
    for (const auto& panel : panelSizes) {

        for (int panelNum = 0; panelNum < 2; panelNum++) {
            AcDbObjectId assetId = loadAsset(panel.id[panelNum].c_str());
			//acutPrintf(_T("\n assetId %s"), panel.id[panelNum].c_str());

            if (assetId != AcDbObjectId::kNull) {
                    int numPanels = static_cast<int>(distance / panel.length);
                    if (numPanels != 0) {
                        for (int i = 0; i < numPanels; i++) {

                            panelLength = panel.length;
                            wallPanels.push_back({ currentPoint, assetId, rotation, panelLength });

                            currentPoint += direction * panelLength;
                            distance -= panelLength;
                        }
                    }
            }
        }
    }

    //Pass 3: Remove specific assets
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

    for (int panelNum = 0; panelNum < wallPanels.size(); ++panelNum) {
        WallPanel& panel = wallPanels[panelNum];
        if (std::find(centerAssets.begin(), centerAssets.end(), panel.assetId) != centerAssets.end()) {

            // Find the two corner points between which the panel is placed
            int panelPosition = panelNum;
            WallPanel detectedPanel = wallPanels[panelPosition];
            AcGePoint3d detectedPanelPosition = detectedPanel.position;
            AcDbObjectId detectedPanelId = detectedPanel.assetId;

            double panelLength = wallPanels[panelPosition].length;

            // Calculate the center index in wallPanels
            int centerIndex = static_cast<int>(wallPanels.size() / 2);

            // Get positions of centerIndex and detectedPanel
            AcGePoint3d centerPanelPosition = wallPanels[centerIndex + movedCompensators].position;

            AcGeVector3d direction = (wallPanels[panelNum].position - wallPanels[centerIndex].position).normal();

            // Adjust the position of the detected panel
            wallPanels[panelNum].position = centerPanelPosition;

            for (int centerToCornerPanelNum = centerIndex + movedCompensators; centerToCornerPanelNum < panelNum - movedCompensators; centerToCornerPanelNum++) {
                wallPanels[centerToCornerPanelNum].position = wallPanels[centerToCornerPanelNum].position + direction * panelLength;
            }

            movedCompensators++;
        }
    }

    wallPanels.erase(
        std::remove_if(
            wallPanels.begin(),
            wallPanels.end(),
            [&centerAssets](const WallPanel& panel) {
                return std::find(centerAssets.begin(), centerAssets.end(), panel.assetId) != centerAssets.end();
            }
        ),
        wallPanels.end()
    );

	//Pass 4: Place all assets
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

    //Step 3: Define all the Assets
    AcDbObjectId PushPullProp;
    AcDbObjectId PushPullKicker;
    AcDbObjectId BraceConnector = loadAsset(L"128294X");
	AcDbObjectId BasePlate = loadAsset(L"126666X");
	AcDbObjectId Anchor = loadAsset(L"124777X");

    //Props and kicker cases heights are: 600, 900, 1200, 1350,1800, 1950, 2400,2550, 2700,3000, 31500, 3300, 3600, 3750, 3900, 4050, 4200, 4350, 4500, 4650, 4800, 4950, 5100, 5250, 5400
    if (globalVarHeight == 600)
    {
        PushPullKicker = loadAsset(L"117466X600Prop");
    }
    else if  ( globalVarHeight  == 900)
	{
        PushPullKicker = loadAsset(L"117466X900Prop");
	}
    else if (globalVarHeight == 1200)
    {
        PushPullKicker = loadAsset(L"117466X1200Prop");
    }
    else if (globalVarHeight == 1350)
    {
        PushPullProp = loadAsset(L"117466X1350Prop");
		PushPullKicker = loadAsset(L"117466X1350Kicker");
    }
	else if (globalVarHeight == 1800)
	{
		PushPullProp = loadAsset(L"117467X1800Prop");
		PushPullKicker = loadAsset(L"117466X1800Kicker");
	}
    else if (globalVarHeight == 1950)
    {
        PushPullProp = loadAsset(L"117466X1950-2400Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 2400)
    {
        PushPullProp = loadAsset(L"117466X1950-2400Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 2550)
    {
        PushPullProp = loadAsset(L"117466X2550-2700Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 2700)
    {
        PushPullProp = loadAsset(L"117466X2550-2700Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 3000)
    {
        PushPullProp = loadAsset(L"117466X3000Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 3150)
    {
        PushPullProp = loadAsset(L"117466X3150Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 3300)
    {
        PushPullProp = loadAsset(L"117466X3300Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 3600)
    {
        PushPullProp = loadAsset(L"117466X3600-3750Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 3750)
    {
        PushPullProp = loadAsset(L"117466X3600-3750Prop");
        PushPullKicker = loadAsset(L"117466X1950-3750Kicker");
    }
    else if (globalVarHeight == 3900)
    {
        PushPullProp = loadAsset(L"117466X3900Prop");
        PushPullKicker = loadAsset(L"117466X3900Kicker");
    }
    else if (globalVarHeight == 4050)
    {
        PushPullProp = loadAsset(L"117466X4050Prop");
        PushPullKicker = loadAsset(L"117466X4050Kicker");
    }
    else if (globalVarHeight == 4200)
    {
        PushPullProp = loadAsset(L"117466X4200Prop");
        PushPullKicker = loadAsset(L"117466X4200Kicker");
    }
    else if (globalVarHeight == 4350)
    {
        PushPullProp = loadAsset(L"117466X4350Prop");
        PushPullKicker = loadAsset(L"117466X4350Kicker");
    }
    else if (globalVarHeight == 4500)
    {
        PushPullProp = loadAsset(L"117466X4500Prop");
        PushPullKicker = loadAsset(L"117466X4500Kicker");
    }
    else if (globalVarHeight == 4650)
    {
        PushPullProp = loadAsset(L"117466X4650Prop");
        PushPullKicker = loadAsset(L"117466X4650Kicker");
    }
    else if (globalVarHeight == 4800)
    {
        PushPullProp = loadAsset(L"117466X4800-5100Prop");
        PushPullKicker = loadAsset(L"117466X4800-5100Kicker");
    }
    else if (globalVarHeight == 4950)
    {
        PushPullProp = loadAsset(L"117466X4800-5100Prop");
        PushPullKicker = loadAsset(L"117466X4800-5100Kicker");
    }
    else if (globalVarHeight == 5100)
    {
        PushPullProp = loadAsset(L"117466X4800-5100Prop");
        PushPullKicker = loadAsset(L"117466X4800-5100Kicker");
    }
    else if (globalVarHeight == 5250)
    {
        PushPullProp = loadAsset(L"117466X5250Prop");
        PushPullKicker = loadAsset(L"117466X5250Kicker");
    }
    else if (globalVarHeight == 5400)
    {
        PushPullProp = loadAsset(L"117466X5400Prop");
        PushPullKicker = loadAsset(L"117466X5400Kicker");
    }
    else
    {
        acutPrintf(_T("\nError: Invalid height, Not from Catalogue"));
    }

    struct TableData {
        int HeightProps;  // Height of the props 
        double xOffset;      // X offset for the panel
        double yOffset;      // Y offset for the panel
        double cOffset;      // Some configuration offset
        double wOffset;      // Width offset of the panel
        double resultAngle;  // Angle of the panel after placement
        double resultAngle2;  // Angle of the panel after placement
		double Distance;	 // Distance as Custom Variable
		double Distance1;	 // Distance as Custom Variable
		double Distance2;	 // Distance as Custom Variable
    };

	//import the json file
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
	GetUserNameA(username, &username_len);

    //set a variable as username
	std::string usernameW(username, username + strlen(username));
    
    // Construct the full path for the JSON file
    //construct props path
    std::string jsonFilePath = "C:\\Users\\" + usernameW + "\\" + PROPS_FILE_NAME;

    std::ifstream jsonFile(jsonFilePath);
    std::string jsonData;

    if (jsonFile.is_open()) {
        jsonData.assign((std::istreambuf_iterator<char>(jsonFile)),
            (std::istreambuf_iterator<char>()));
        jsonFile.close();
    }
    else {
		acutPrintf(_T("\nFailed to open JSON file."));
        return;
    }

    // Parse the JSON
    nlohmann::json json = nlohmann::json::parse(jsonData);

    // Vector to hold the TableData structs
    std::vector<TableData> tableDataList;

    for (const auto& item : json["props"]) {
        TableData tableData;

        tableData.HeightProps = item["height"].get<int>();

        const auto& data = item["data"];
        tableData.xOffset = data["x"].get<double>();
        tableData.yOffset = data["y"].get<double>();
        tableData.cOffset = data["c"].get<double>();
        tableData.wOffset = data["w"].get<double>();
        tableData.resultAngle = data["resultAngle"].get<double>();
        tableData.resultAngle2 = data["resultAngle2"].get<double>();
        tableData.Distance = data["Distance"].get<double>();
        tableData.Distance1 = data["Distance1"].get<double>();
        tableData.Distance2 = data["Distance2"].get<double>();

        tableDataList.push_back(tableData);
    }

    // Define offsets
    int basePlateOffset;
    int braceConnectorOffsetBottom;
    int braceConnectorOffsetTop;
    double propAngle;
    double kickerAngle;
    double braceConnectorOffset = 1200;
    double braceConnectorWidthOfset = 100;
	double braceConnectorlengthOfset = 150;
    int propWidth;
    // Define these variables outside the loop so they can be accessed in the second loop
    std::wstring Distance;
    std::wstring Distance1;
    std::wstring Distance2;

    // get xOffset as basePlateOffset using for loop
    for (const auto& tableData : tableDataList) {
        if (tableData.HeightProps == globalVarHeight) {

            basePlateOffset = static_cast<int>(std::round(tableData.xOffset));
            braceConnectorOffsetBottom = static_cast<int>(std::round(tableData.cOffset));
            braceConnectorOffsetTop = static_cast<int>(std::round(tableData.yOffset));
            propAngle = tableData.resultAngle;
            kickerAngle = tableData.resultAngle2;
            propWidth = static_cast<int>(std::round(tableData.wOffset));
            Distance = std::to_wstring(tableData.Distance);
            Distance1 = std::to_wstring(tableData.Distance1);
            Distance2 = std::to_wstring(tableData.Distance2);

            break;
        }
    }

    for (const auto& panel : wallPanels) {
        //Create block references
		AcDbBlockReference* pBasePlate = new AcDbBlockReference();
		AcDbBlockReference* pAnchor = new AcDbBlockReference();
		AcDbBlockReference* pBraceConnectorTop = new AcDbBlockReference();
		AcDbBlockReference* pBraceConnectorBottom = new AcDbBlockReference();
		AcDbBlockReference* pPushPullProp = new AcDbBlockReference();
		AcDbBlockReference* pPushPullKicker = new AcDbBlockReference();

        //Initialize rotation Matrices
        AcGeMatrix3d rotationMatrixXProp;
		AcGeMatrix3d rotationMatrixYProp;
		AcGeMatrix3d rotationMatrixZProp;

		AcGeMatrix3d rotationMatrixXKicker;
		AcGeMatrix3d rotationMatrixYKicker;
		AcGeMatrix3d rotationMatrixZKicker;


        //Set current points based on panel positions
		AcGePoint3d BasePlatecurrentPoint = panel.position;
		AcGePoint3d AnchorcurrentPoint = panel.position;
		AcGePoint3d BraceConnectorTopcurrentPoint = panel.position;
		AcGePoint3d BraceConnectorBottomcurrentPoint = panel.position;
		AcGePoint3d PushPullPropcurrentPoint = panel.position;
		AcGePoint3d PushPullKickercurrentPoint = panel.position;
        double angleZProp;
        double angleZKicker;

        switch (static_cast<int>(round(rotation / M_PI_2))) {
        case 0:
			//Base Plate Offsets
            //acutPrintf(_T("\n Rotation: %d"), rotation);
            BasePlatecurrentPoint.y -= basePlateOffset;
            BasePlatecurrentPoint.x += braceConnectorlengthOfset;
            //Anchor Offsets
            AnchorcurrentPoint.y -= basePlateOffset;
            AnchorcurrentPoint.x += braceConnectorlengthOfset;
            //BraceConnector Offsets
            BraceConnectorTopcurrentPoint.z += braceConnectorOffsetTop;
            BraceConnectorTopcurrentPoint.x += braceConnectorlengthOfset;
            BraceConnectorTopcurrentPoint.y -= braceConnectorWidthOfset;
            BraceConnectorBottomcurrentPoint.z += braceConnectorOffsetBottom;
            BraceConnectorBottomcurrentPoint.x += braceConnectorlengthOfset;
            BraceConnectorBottomcurrentPoint.y -= braceConnectorWidthOfset;
            //Prop Offsets
            PushPullPropcurrentPoint.x += braceConnectorlengthOfset;
            PushPullPropcurrentPoint.y -= basePlateOffset;
            PushPullPropcurrentPoint.y += 55;
            PushPullPropcurrentPoint.z += 75;
			//acutPrintf(_T("\n PushPullPropcurrentPoint: %f, %f, %f"), PushPullPropcurrentPoint.x, PushPullPropcurrentPoint.y, PushPullPropcurrentPoint.z);
			angleZProp = (M_PI_2);
            //Kicker Offsets
            PushPullKickercurrentPoint.x += braceConnectorlengthOfset;
            PushPullKickercurrentPoint.y -= basePlateOffset;
            PushPullKickercurrentPoint.y += 145;
            PushPullKickercurrentPoint.z += 65;
            angleZKicker = (M_PI_2);
            break;

		case 1://90 degree
            //acutPrintf(_T("\n Rotation: %d"), rotation);
            BasePlatecurrentPoint.x += basePlateOffset;
            BasePlatecurrentPoint.y += braceConnectorlengthOfset;
            //Anchor Offsets
            AnchorcurrentPoint.x += basePlateOffset;
            AnchorcurrentPoint.y += braceConnectorlengthOfset;
            //BraceConnector Offsets
            BraceConnectorTopcurrentPoint.z += braceConnectorOffsetTop;
            BraceConnectorTopcurrentPoint.y += braceConnectorlengthOfset;
            BraceConnectorTopcurrentPoint.x += braceConnectorWidthOfset;
            BraceConnectorBottomcurrentPoint.z += braceConnectorOffsetBottom;
            BraceConnectorBottomcurrentPoint.y += braceConnectorlengthOfset;
            BraceConnectorBottomcurrentPoint.x += braceConnectorWidthOfset;
            //Prop Offsets
            PushPullPropcurrentPoint.y += braceConnectorlengthOfset;
            PushPullPropcurrentPoint.x += basePlateOffset;
            PushPullPropcurrentPoint.x -= 55;
            PushPullPropcurrentPoint.z += 75;
            //acutPrintf(_T("\n PushPullPropcurrentPoint: %f, %f, %f"), PushPullPropcurrentPoint.x, PushPullPropcurrentPoint.y, PushPullPropcurrentPoint.z);
            angleZProp = (M_PI_2+M_PI_2);
            //Kicker Offsets
            PushPullKickercurrentPoint.y += braceConnectorlengthOfset;
            PushPullKickercurrentPoint.x += basePlateOffset;
            PushPullKickercurrentPoint.x -= 145;
            PushPullKickercurrentPoint.z += 65;
            angleZKicker = (M_PI_2+M_PI_2);
            break;
		case 2: //180 degree
            BasePlatecurrentPoint.y += basePlateOffset;
            BasePlatecurrentPoint.x -= braceConnectorlengthOfset;
            //Anchor Offsets
            AnchorcurrentPoint.y += basePlateOffset;
            AnchorcurrentPoint.x -= braceConnectorlengthOfset;
            //BraceConnector Offsets
            BraceConnectorTopcurrentPoint.z += braceConnectorOffsetTop;
            BraceConnectorTopcurrentPoint.x -= braceConnectorlengthOfset;
            BraceConnectorTopcurrentPoint.y += braceConnectorWidthOfset;
            BraceConnectorBottomcurrentPoint.z += braceConnectorOffsetBottom;
            BraceConnectorBottomcurrentPoint.x -= braceConnectorlengthOfset;
            BraceConnectorBottomcurrentPoint.y += braceConnectorWidthOfset;
            //Prop Offsets
            PushPullPropcurrentPoint.x -= braceConnectorlengthOfset;
            PushPullPropcurrentPoint.y += basePlateOffset;
            PushPullPropcurrentPoint.y -= 55;
            PushPullPropcurrentPoint.z += 75;
            angleZProp = (M_PI_2 + M_PI_2 + M_PI_2);
            //Kicker Offsets
            PushPullKickercurrentPoint.x -= braceConnectorlengthOfset;
            PushPullKickercurrentPoint.y += basePlateOffset;
            PushPullKickercurrentPoint.y -= 145;
            PushPullKickercurrentPoint.z += 65;
            angleZKicker = (M_PI_2 + M_PI_2+ M_PI_2);
            break;
        case 3: //270 degree
		case -1: // -90 degree
            BasePlatecurrentPoint.x -= basePlateOffset;
            BasePlatecurrentPoint.y -= braceConnectorlengthOfset;
            //Anchor Offsets
            AnchorcurrentPoint.x -= basePlateOffset;
            AnchorcurrentPoint.y -= braceConnectorlengthOfset;
            //BraceConnector Offsets
            BraceConnectorTopcurrentPoint.z += braceConnectorOffsetTop;
            BraceConnectorTopcurrentPoint.y -= braceConnectorlengthOfset;
            BraceConnectorTopcurrentPoint.x -= braceConnectorWidthOfset;
            BraceConnectorBottomcurrentPoint.z += braceConnectorOffsetBottom;
            BraceConnectorBottomcurrentPoint.y -= braceConnectorlengthOfset;
            BraceConnectorBottomcurrentPoint.x -= braceConnectorWidthOfset;
            //Prop Offsets
            PushPullPropcurrentPoint.y -= braceConnectorlengthOfset;
            PushPullPropcurrentPoint.x -= basePlateOffset;
            PushPullPropcurrentPoint.x += 55;
            PushPullPropcurrentPoint.z += 75;
            //acutPrintf(_T("\n PushPullPropcurrentPoint: %f, %f, %f"), PushPullPropcurrentPoint.x, PushPullPropcurrentPoint.y, PushPullPropcurrentPoint.z);
            angleZProp = (M_PI_2 + M_PI_2 + M_PI_2 + M_PI_2);
            //Kicker Offsets
            PushPullKickercurrentPoint.y -= braceConnectorlengthOfset;
            PushPullKickercurrentPoint.x -= basePlateOffset;
            PushPullKickercurrentPoint.x += 145;
            PushPullKickercurrentPoint.z += 65;
            angleZKicker = (M_PI_2 + M_PI_2 + M_PI_2 + M_PI_2);
            break;

        }

        //place basePlate
		pBasePlate->setPosition(BasePlatecurrentPoint);
		pBasePlate->setBlockTableRecord(BasePlate);
		pBasePlate->setRotation(rotation - M_PI_2);
		if (pModelSpace->appendAcDbEntity(pBasePlate) != Acad::eOk) {
			acutPrintf(_T("\nFailed to append BasePlate reference."));
		}
		pBasePlate->close();

        //place anchor
		pAnchor->setPosition(AnchorcurrentPoint);
		pAnchor->setBlockTableRecord(Anchor);
		pAnchor->setRotation(rotation - M_PI_2);
		if (pModelSpace->appendAcDbEntity(pAnchor) != Acad::eOk) {
			acutPrintf(_T("\nFailed to append Anchor reference."));
		}
		pAnchor->close();

		//place braceConnectorTop
		pBraceConnectorTop->setPosition(BraceConnectorTopcurrentPoint);
		pBraceConnectorTop->setBlockTableRecord(BraceConnector);
		pBraceConnectorTop->setRotation(rotation);
		if (pModelSpace->appendAcDbEntity(pBraceConnectorTop) != Acad::eOk) {
			acutPrintf(_T("\nFailed to append BraceConnectorTop reference."));
		}
		pBraceConnectorTop->close();

        if (globalVarHeight == 600 || globalVarHeight == 900 || globalVarHeight == 1200) {
            acutPrintf(_T("\n Second Brace Not required"));
        }
        else {
            // Place braceConnectorBottom logic
            pBraceConnectorBottom->setPosition(BraceConnectorBottomcurrentPoint);
            pBraceConnectorBottom->setBlockTableRecord(BraceConnector);
            pBraceConnectorBottom->setRotation(rotation);

            if (pModelSpace->appendAcDbEntity(pBraceConnectorBottom) != Acad::eOk) {
                acutPrintf(_T("\nFailed to append BraceConnectorBottom reference."));
            }
            pBraceConnectorBottom->close();

            acutPrintf(_T("\n brace Placed"));
        }

        if (globalVarHeight == 600 || globalVarHeight == 900 || globalVarHeight == 1200) {
            acutPrintf(_T("\n Kicker Not required"));
        }
        else {
            //place PushPullProp
            pPushPullProp->setPosition(PushPullPropcurrentPoint);
            pPushPullProp->setBlockTableRecord(PushPullProp);
            rotationMatrixYProp.setToRotation(propAngle, AcGeVector3d::kYAxis, pPushPullProp->position());
            rotationMatrixZProp.setToRotation(angleZProp, AcGeVector3d::kZAxis, pPushPullProp->position());
            AcGeMatrix3d combinedRotationMatrixProp = rotationMatrixZProp * rotationMatrixYProp;

            pPushPullProp->transformBy(combinedRotationMatrixProp);
            if (pModelSpace->appendAcDbEntity(pPushPullProp) != Acad::eOk) {
                acutPrintf(_T("\nFailed to append PushPullProp reference."));
            }
            pPushPullProp->close();

            acutPrintf(_T("\n Prop Placed"));
        }

            //place PushPullKicker
            pPushPullKicker->setPosition(PushPullKickercurrentPoint);
            pPushPullKicker->setBlockTableRecord(PushPullKicker);
            rotationMatrixYKicker.setToRotation(kickerAngle, AcGeVector3d::kYAxis, pPushPullKicker->position());
            rotationMatrixZKicker.setToRotation(angleZKicker, AcGeVector3d::kZAxis, pPushPullKicker->position());
            AcGeMatrix3d combinedRotationMatrixKicker = rotationMatrixZKicker * rotationMatrixYKicker;

            pPushPullKicker->transformBy(combinedRotationMatrixKicker);
            if (pModelSpace->appendAcDbEntity(pPushPullKicker) != Acad::eOk) {
                acutPrintf(_T("\nFailed to append PushPullKicker reference."));
            }
            pPushPullKicker->close();
	}

	pModelSpace->close();
	pBlockTable->close();


}