#include "StdAfx.h"
#include "TestCol.h"
#include <dbapserv.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <AcDb.h>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

std::string getBlockName(AcDbObjectId blockId) {
    AcDbBlockTableRecord* pBlockRec = nullptr;
    if (acdbOpenObject(pBlockRec, blockId, AcDb::kForRead) != Acad::eOk) {
        return "";
    }

    ACHAR* name = nullptr;
    if (pBlockRec->getName(name) != Acad::eOk) {
        pBlockRec->close();
        return "";
    }

    std::wstring wstr(name);
    std::string blockName(wstr.begin(), wstr.end());

    pBlockRec->close();
    acutDelString(name);
    return blockName;
}

std::vector<ColumnDataload> ColumnPlacermain::extractColumnData() {
    std::vector<ColumnDataload> columns;
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
            ColumnDataload colData;
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

void ColumnPlacermain::saveColumnDataToJson(const std::vector<ColumnDataload>& columns, const std::string& filePath) {
    json root;
    for (const auto& col : columns) {
        json colData;
        colData["blockName"] = col.blockName;
        colData["position"]["x"] = col.position.x;
        colData["position"]["y"] = col.position.y;
        colData["position"]["z"] = col.position.z;
        colData["rotation"] = col.rotation;
        root["columns"].push_back(colData);
    }

    std::ofstream file(filePath);
    file << root.dump(4); // Pretty-print JSON with an indentation of 4 spaces
    file.close();
}

std::vector<ColumnDataload> ColumnPlacermain::loadColumnDataFromJson(const std::string& filePath) {
    std::vector<ColumnDataload> columns;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        acutPrintf(_T("Failed to open file: %s\n"), filePath.c_str());
        return columns;
    }

    json root;
    file >> root;
    file.close();

    for (const auto& colData : root["columns"]) {
        ColumnDataload col;
        col.blockName = colData["blockName"];
        col.position = AcGePoint3d(colData["position"]["x"], colData["position"]["y"], colData["position"]["z"]);
        col.rotation = colData["rotation"];
        columns.push_back(col);
    }

    return columns;
}

void ColumnPlacermain::placeColumnsFromJson(const std::string& filePath) {
    std::vector<ColumnDataload> columns = loadColumnDataFromJson(filePath);
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("No working database found."));
        return;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("Failed to get block table."));
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("Failed to get model space."));
        pBlockTable->close();
        return;
    }

    for (const auto& col : columns) {
        // Convert std::string to ACHAR*
        std::wstring wstr(col.blockName.begin(), col.blockName.end());
        const ACHAR* blockName = wstr.c_str();

        AcDbBlockTableRecord* pBlockRec = nullptr;
        if (pBlockTable->getAt(blockName, pBlockRec, AcDb::kForRead) != Acad::eOk) {
            acutPrintf(_T("Failed to get block: %s\n"), col.blockName.c_str());
            continue;
        }

        AcDbObjectId blockId = pBlockRec->objectId();
        pBlockRec->close();

        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
        pBlockRef->setBlockTableRecord(blockId);
        pBlockRef->setPosition(col.position);
        pBlockRef->setRotation(col.rotation);
        pBlockRef->setScaleFactors(AcGeScale3d(1.0, 1.0, 1.0));  // Set scale factors as needed

        // Debugging print for positions and rotations
        acutPrintf(_T("Placing block %s at X: %f, Y: %f, Z: %f, Rotation: %f\n"),
            blockName, col.position.x, col.position.y, col.position.z, col.rotation);

        if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
            acutPrintf(_T("Failed to place block: %s\n"), col.blockName.c_str());
            delete pBlockRef;
            continue;
        }

        pBlockRef->close();
    }

    pModelSpace->close();
    pBlockTable->close();
}
