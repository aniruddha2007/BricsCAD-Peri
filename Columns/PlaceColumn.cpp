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
    // Ask the user for the column name
    ACHAR blockNameInput[256];
    if (acedGetString(Adesk::kFalse, _T("\nEnter the column name (e.g., 200x200): "), blockNameInput) != RTNORM) {
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

    // Use the global variable for height
    int totalHeightRequired = globalVarHeight;
    int currentHeight = 0;

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

    // Find the column with the specified name
    bool columnFound = false;
    std::vector<json> selectedBlockData;
    for (const auto& columnData : blocksJson["columns"]) {
        if (columnData["blockname"] == blockNameStr) {
            selectedBlockData.push_back(columnData);
            columnFound = true;
        }
    }

    if (!columnFound) {
        acutPrintf(_T("\nColumn '%s' not found in the JSON file."), blockNameInput);
        return;
    }

    // Sort the selected blocks by height in descending order
    std::sort(selectedBlockData.begin(), selectedBlockData.end(), [](const json& a, const json& b) {
        return a["height"].get<int>() > b["height"].get<int>();
        });

    // Prompt the user to pick a base point on the screen
    AcGePoint3d basePoint;
    ads_point adsBasePoint;
    if (acedGetPoint(nullptr, _T("\nSpecify the base point for column insertion: "), adsBasePoint) != RTNORM) {
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

    // Loop through available heights and place blocks accordingly
    for (const auto& blockData : selectedBlockData) {
        int blockHeight = blockData["height"].get<int>();

        // Calculate how many blocks of this height we can stack
        int numBlocksToPlace = (totalHeightRequired - currentHeight) / blockHeight;

        for (int i = 0; i < numBlocksToPlace; i++) {
            // Place all blocks associated with this height
            for (const auto& block : blockData["blocks"]) {
                std::string blockNameStr = block["name"];
                double blockRotation = block["rotation"];
                AcGeScale3d blockScale(
                    block["scale"]["x"].get<double>(),
                    block["scale"]["y"].get<double>(),
                    block["scale"]["z"].get<double>()
                );

                // Retrieve the block definition from the block table
                AcDbBlockTableRecord* pBlockDef;
#ifdef UNICODE
                std::wstring wBlockName = std::wstring(blockNameStr.begin(), blockNameStr.end());
                if (pBlockTable->getAt(wBlockName.c_str(), pBlockDef, AcDb::kForRead) != Acad::eOk) {
                    acutPrintf(_T("\nBlock definition not found: %ls"), wBlockName.c_str());
                    continue;
                }
#else
                if (pBlockTable->getAt(blockNameStr.c_str(), pBlockDef, AcDb::kForRead) != Acad::eOk) {
                    acutPrintf(_T("\nBlock definition not found: %s"), blockNameStr.c_str());
                    continue;
                }
#endif

                // Get the block's base point (insertion point within the block definition)
                AcGePoint3d blockBasePoint;
                // Correct version:
                AcGePoint3d originPoint;
                if (!blockBasePoint.isEqualTo(originPoint)) {
                    blockBasePoint = originPoint; // Update the base point
                }


                // Create a new block reference
                AcDbBlockReference* pBlockRef = new AcDbBlockReference();
                pBlockRef->setBlockTableRecord(pBlockDef->objectId());

                // Adjust the insertion point relative to the block's base point
                AcGePoint3d adjustedInsertionPoint = basePoint - blockBasePoint.asVector();
                adjustedInsertionPoint.z += currentHeight; // Adjust Z for stacking

                pBlockRef->setPosition(adjustedInsertionPoint);
                pBlockRef->setRotation(blockRotation);
                pBlockRef->setScaleFactors(blockScale);

                // Add the block reference to model space
                if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
                    acutPrintf(_T("\nBlock '%s' inserted successfully at (%.2f, %.2f, %.2f)."),
                        blockNameStr.c_str(), adjustedInsertionPoint.x, adjustedInsertionPoint.y, adjustedInsertionPoint.z);
                }
                else {
                    acutPrintf(_T("\nFailed to insert block '%s'."), blockNameStr.c_str());
                }

                // Clean up
                pBlockRef->close();
                pBlockDef->close();
            }

            // Update the current height after placing the blocks
            currentHeight += blockHeight;
        }
    }

    // Clean up
    pModelSpace->close();
    pBlockTable->close();
}
