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
    int columnHeight;
    if (acedGetInt(_T("\nEnter the column height (e.g., 1350 or 600): "), &columnHeight) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

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

    // Start a transaction to access the model space
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    AcDbBlockTable* pBlockTable;
    if (pDb->getSymbolTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to open the block table."));
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to open the model space."));
        pBlockTable->close();
        return;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create a model space iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return;
    }

    json columnJson; // JSON object to store the column data
    columnJson["blockname"] = columnNameStr;
    columnJson["height"] = columnHeight;  // Store the height information in JSON
    columnJson["blocks"] = json::array();

    // Iterate through model space and extract block data
    for (pIter->start(); !pIter->done(); pIter->step())
    {
        AcDbEntity* pEntity;
        pIter->getEntity(pEntity, AcDb::kForRead);

        if (pEntity->isKindOf(AcDbBlockReference::desc())) {
            AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEntity);

            // Get block name using AcString
            AcDbObjectId blockId = pBlockRef->blockTableRecord();
            AcDbBlockTableRecord* pBlockDef;
            if (acdbOpenObject(pBlockDef, blockId, AcDb::kForRead) != Acad::eOk) {
                acutPrintf(_T("\nFailed to open block definition."));
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
            AcGePoint3d position = pBlockRef->position();
            double rotation = pBlockRef->rotation();
            AcGeScale3d scale = pBlockRef->scaleFactors();

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

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    // Load the existing JSON file or create a new one
    std::ifstream inFile("C:\\Users\\aniru\\OneDrive\\Desktop\\work\\AP-Columns_05-10-24.json");
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
    std::ofstream outFile("C:\\Users\\aniru\\OneDrive\\Desktop\\work\\AP-Columns_05-10-24.json");
    if (outFile.is_open()) {
        outFile << blocksJson.dump(4); // Write with 4-space indentation
        outFile.close();
        acutPrintf(_T("\nColumn data saved successfully."));
    }
    else {
        acutPrintf(_T("\nFailed to open the JSON file for writing."));
    }
}

