#include "StdAfx.h"
#include "extractCol.h"
#include <dbapserv.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <AcDb.h>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// Function to extract column data from the model space
std::vector<ColumnData> ColumnExtractor::extractColumnData() {
    std::vector<ColumnData> columns;
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("No working database found."));
        return columns;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("Failed to get block table."));
        return columns;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("Failed to get model space."));
        pBlockTable->close();
        return columns;
    }

    AcDbBlockTableRecordIterator* pIter;
    pModelSpace->newIterator(pIter);
    for (; !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) != Acad::eOk) {
            continue;
        }

        if (pEnt->isKindOf(AcDbBlockReference::desc())) {
            AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEnt);
            ColumnData colData;
            colData.blockName = getBlockName(pBlockRef->blockTableRecord());
            colData.position = pBlockRef->position();
            colData.rotation = pBlockRef->rotation();
            columns.push_back(colData);
        }

        pEnt->close();
    }

    delete pIter; // Correctly delete the iterator
    pModelSpace->close();
    pBlockTable->close();
    return columns;
}

// Function to extract individual block data and save to a JSON file
void ColumnExtractor::extractAndSaveToJson(const std::string& jsonFilePath) {
    std::vector<ColumnData> columns = extractColumnData();

    // Prepare JSON object to store block data
    json blockData;

    // Save the extracted block references and their details
    for (const auto& col : columns) {
        json colJson;
        colJson["blockName"] = col.blockName;
        colJson["position"]["x"] = col.position.x;
        colJson["position"]["y"] = col.position.y;
        colJson["position"]["z"] = col.position.z;
        colJson["rotation"] = col.rotation;

        blockData["columns"].push_back(colJson);
    }

    // Write JSON to file
    std::ofstream file(jsonFilePath);
    if (!file.is_open()) {
        acutPrintf(_T("Failed to open file for writing JSON data."));
        return;
    }
    file << blockData.dump(4); // Pretty-print JSON with an indentation of 4 spaces
    file.close();

    acutPrintf(_T("Data saved to JSON."));
}