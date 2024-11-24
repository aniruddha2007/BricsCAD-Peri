#include "stdafx.h"
#include "ExtractColumn.h"
#include <acdb.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbapserv.h>
#include <aced.h>
#include <nlohmann/json.hpp> // Include nlohmann JSON header
#include <fstream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

using json = nlohmann::json;

void ExtractColumn()
{
    // Ask the user for the column height
    int columnHeight = 1350;

    // Ask the user for the column name
    ACHAR columnName[256];
    if (acedGetString(Adesk::kFalse, _T("\nEnter the column name: "), columnName) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

    // Convert ACHAR* (columnName) to std::string
    std::string columnNameStr;
#ifdef UNICODE
    columnNameStr = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(columnName);
#else
    columnNameStr = columnName;
#endif


    // Prompt the user to select entities
    ads_name selectionSet;
    int result = acedSSGet(NULL, NULL, NULL, NULL, selectionSet);
    if (result != RTNORM) {
        acutPrintf(_T("\nNo entities selected or selection canceled."));
        return;
    }

    // Get the number of selected entities
    long length = 0;
    acedSSLength(selectionSet, &length);
    if (length == 0) {
        acutPrintf(_T("\nNo entities selected."));
        acedSSFree(selectionSet);
        return;
    }

    json columnJson; // JSON object to store the column data
    columnJson["blockname"] = columnNameStr;
    columnJson["height"] = columnHeight;  // Store the height information in JSON
    columnJson["blocks"] = json::array();

    // Iterate through the selected entities
    for (long i = 0; i < length; i++) {
        ads_name entityName;
        acedSSName(selectionSet, i, entityName);

        AcDbObjectId entityId;
        if (acdbGetObjectId(entityId, entityName) != Acad::eOk) {
            continue;
        }

        AcDbEntity* pEntity;
        if (acdbOpenObject(pEntity, entityId, AcDb::kForRead) != Acad::eOk) {
            continue;
        }

        if (pEntity->isKindOf(AcDbBlockReference::desc())) {
            AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEntity);

            // Get block name using AcString
            AcDbObjectId blockId = pBlockRef->blockTableRecord();
            AcDbBlockTableRecord* pBlockDef;
            Acad::ErrorStatus es = acdbOpenObject(pBlockDef, blockId, AcDb::kForRead);
            if (es != Acad::eOk) {
                acutPrintf(_T("\nFailed to open block definition: %s"), (es));
                pEntity->close();
                continue;
            }

            AcString blockName;
            pBlockDef->getName(blockName);

            // Convert AcString to std::string
            std::string blockNameStr;
#ifdef UNICODE
            blockNameStr = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(blockName.kwszPtr());
#else
            blockNameStr = blockName.kACharPtr(); // Direct conversion for ANSI builds
#endif

            // Get position, rotation, and scale
            AcGePoint3d position = pBlockRef->position(); // Position of the block
            double rotation = pBlockRef->rotation();      // Rotation of the block
            AcGeScale3d scale = pBlockRef->scaleFactors(); // Scale of the block

            // Create JSON object for the block
            json blockData;
            blockData["name"] = blockNameStr;
            blockData["position"] = { {"x", position.x}, {"y", position.y}, {"z", position.z} };
            blockData["rotation"] = rotation;
            blockData["scale"] = { {"x", scale.sx}, {"y", scale.sy}, {"z", scale.sz} };

            columnJson["blocks"].push_back(blockData);

            pBlockDef->close();
        }

        pEntity->close();
    }

    acedSSFree(selectionSet); // Free the selection set

    // Load the existing JSON file or create a new one
    std::ifstream inFile("C:\\Users\\aniru\\OneDrive\\Desktop\\work\\AP-Columns_12-11-24.json");
    json blocksJson;
    if (inFile.is_open()) {
        acutPrintf(_T("\nJSON file opened successfully."));
        inFile >> blocksJson;
        inFile.close();
    }
    else {
        acutPrintf(_T("\nJSON file not found, creating a new one."));
    }

    // Append the new column data to the existing JSON structure
    blocksJson["columns"].push_back(columnJson);

    // Save the updated JSON structure back to the file
    std::ofstream outFile("C:\\Users\\aniru\\OneDrive\\Desktop\\work\\AP-Columns_12-11-24.json");
    if (outFile.is_open()) {
        outFile << blocksJson.dump(4); // Write with 4-space indentation
        outFile.close();
        acutPrintf(_T("\nColumn data saved successfully."));
    }
    else {
        acutPrintf(_T("\nFailed to open the JSON file for writing."));
    }
}
