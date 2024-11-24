#include "stdafx.h"
#include "PlaceColumn.h"
#include <acdb.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbapserv.h>
#include <aced.h>
#include <nlohmann/json.hpp> // Include nlohmann JSON header
#include <fstream>
#include <string>
#include <sstream>
#include "DefineHeight.h"

using json = nlohmann::json;

void PlaceColumn(const std::string& jsonFilePath)
{
    // Ask the user for the block name
    ACHAR blockNameInput[256];
    if (acedGetString(Adesk::kFalse, _T("\nEnter the block name: "), blockNameInput) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

    // Convert ACHAR* to std::string
#ifdef UNICODE
    std::wstring wBlockNameInput(blockNameInput);
    std::string blockNameStr = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wBlockNameInput);
#else
    std::string blockNameStr(blockNameInput);
#endif

    // Load the JSON file
    std::ifstream inFile(jsonFilePath);
    if (!inFile.is_open()) {
        acutPrintf(_T("\nFailed to open the JSON file."));
        return;
    }

    json blocksJson;
    inFile >> blocksJson;
    inFile.close();

    if (blocksJson.empty()) {
        acutPrintf(_T("\nNo blocks found in the JSON file."));
        return;
    }

    // Find the block with the specified name
    bool blockFound = false;
    json selectedBlockData;
    for (const auto& blockData : blocksJson["columns"]) {
        if (blockData["blockname"] == blockNameStr) {
            selectedBlockData = blockData;
            blockFound = true;
            break;
        }
    }

    if (!blockFound) {
        acutPrintf(_T("\nBlock '%s' not found in the JSON file."), blockNameInput);
        return;
    }

    // Prompt the user to pick a base point on the screen
    AcGePoint3d basePoint;
    ads_point adsBasePoint;
    if (acedGetPoint(nullptr, _T("\nSpecify the base point for block insertion: "), adsBasePoint) != RTNORM) {
        acutPrintf(_T("\nPoint selection was canceled."));
        return;
    }
    basePoint.set(adsBasePoint[X], adsBasePoint[Y], adsBasePoint[Z]);

    // Open the block table
    AcDbBlockTable* pBlockTable;
    acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, AcDb::kForRead);

    // Open model space for writing
    AcDbBlockTableRecord* pModelSpace;
    pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);

    // Calculate the reference point of the first block to use as a relative base point
    AcGePoint3d firstBlockPos(
        selectedBlockData["blocks"][0]["position"]["x"].get<double>(),
        selectedBlockData["blocks"][0]["position"]["y"].get<double>(),
        selectedBlockData["blocks"][0]["position"]["z"].get<double>()
    );

    // Loop over each block in the selected column
    for (const auto& blockData : selectedBlockData["blocks"]) {
        std::string blockNameStr = blockData["name"];
        AcGePoint3d blockPos(
            blockData["position"]["x"].get<double>(),
            blockData["position"]["y"].get<double>(),
            blockData["position"]["z"].get<double>()
        );
        double blockRotation = blockData["rotation"];
        AcGeScale3d blockScale(
            blockData["scale"]["x"].get<double>(),
            blockData["scale"]["y"].get<double>(),
            blockData["scale"]["z"].get<double>()
        );

        // Convert std::string to ACHAR*
#ifdef UNICODE
        std::wstring wBlockName = std::wstring(blockNameStr.begin(), blockNameStr.end());
        const wchar_t* blockName = wBlockName.c_str();
#else
        const char* blockName = blockNameStr.c_str();
#endif

        // Check if the block name exists in the block table
        AcDbBlockTableRecord* pBlockDef;
        if (pBlockTable->getAt(blockName, pBlockDef, AcDb::kForRead) != Acad::eOk) {
            acutPrintf(_T("\nBlock definition not found: %s"), blockName);
            continue;
        }

        // Calculate the offset from the first block's original position
        AcGeVector3d offset = blockPos - firstBlockPos;

        // Calculate the final insertion point relative to the user-specified base point
        AcGePoint3d insertionPoint = basePoint + offset;

        // Create a new block reference
        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
        pBlockRef->setBlockTableRecord(pBlockDef->objectId());
        pBlockRef->setPosition(insertionPoint);
        pBlockRef->setRotation(blockRotation);
        pBlockRef->setScaleFactors(blockScale);

        // Add the block reference to model space
        if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
            acutPrintf(_T("\nBlock '%s' inserted successfully at (%.2f, %.2f, %.2f)."),
                blockName, insertionPoint.x, insertionPoint.y, insertionPoint.z);
        }
        else {
            acutPrintf(_T("\nFailed to insert block '%s'."), blockName);
        }

        // Clean up
        pBlockRef->close();
        pBlockDef->close();
    }

    // Clean up
    pModelSpace->close();
    pBlockTable->close();
}