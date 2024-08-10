#include "StdAfx.h"
#include "extractCol.h"
#include <dbapserv.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <AcDb.h>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

//std::string getBlockName(AcDbObjectId blockId) {
//    AcDbBlockTableRecord* pBlockRec = nullptr;
//    if (acdbOpenObject(pBlockRec, blockId, AcDb::kForRead) != Acad::eOk) {
//        return "";
//    }
//
//    ACHAR* name = nullptr;
//    if (pBlockRec->getName(name) != Acad::eOk) {
//        pBlockRec->close();
//        return "";
//    }
//
//    // Convert ACHAR* (which is wchar_t*) to std::string
//    std::wstring wstr(name);
//    std::string blockName(wstr.begin(), wstr.end());
//
//    pBlockRec->close();
//    acutDelString(name);
//    return blockName;
//}

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

void ColumnExtractor::extractAndCreateBlock(const std::string& blockName, const std::string& jsonFilePath) {
    std::vector<ColumnData> columns = extractColumnData();

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("No working database found."));
        return;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("Failed to get block table."));
        return;
    }

    // Create a new block definition
    AcDbBlockTableRecord* pBlockDef = new AcDbBlockTableRecord();
    std::wstring wBlockName(blockName.begin(), blockName.end()); // Convert to wide string
    pBlockDef->setName(wBlockName.c_str());

    for (const auto& col : columns) {
        AcDbBlockTableRecord* pBlockRec = nullptr;
        std::wstring wBlockName(col.blockName.begin(), col.blockName.end()); // Convert block name to wide string
        if (pBlockTable->getAt(wBlockName.c_str(), pBlockRec, AcDb::kForRead) != Acad::eOk) {
            acutPrintf(_T("Failed to get block: %s\n"), col.blockName.c_str());
            continue;
        }

        AcDbBlockReference* pBlockRef = new AcDbBlockReference();
        pBlockRef->setBlockTableRecord(pBlockRec->objectId());
        pBlockRef->setPosition(col.position);
        pBlockRef->setRotation(col.rotation);
        pBlockRef->setScaleFactors(AcGeScale3d(1.0, 1.0, 1.0));  // Set scale factors as needed

        if (pBlockDef->appendAcDbEntity(pBlockRef) != Acad::eOk) {
            acutPrintf(_T("Failed to add block reference to block definition."));
            delete pBlockRef;
            continue;
        }
        pBlockRec->close(); // Close the block table record
        pBlockRef->close();
    }

    if (pBlockTable->add(pBlockDef) != Acad::eOk) {
        acutPrintf(_T("Failed to add block definition to block table."));
        delete pBlockDef;
        pBlockTable->close();
        return;
    }

    pBlockDef->close();
    pBlockTable->close();

    // Save the block creation information to a JSON file
    json blockData;
    blockData["blockName"] = blockName;

    // Saving just one entity's position as insertion point (this can be any point you choose)
    if (!columns.empty()) {
        blockData["insertion"]["x"] = columns[0].position.x;
        blockData["insertion"]["y"] = columns[0].position.y;
        blockData["insertion"]["z"] = columns[0].position.z;
        blockData["rotation"] = columns[0].rotation;
    }

    std::ofstream file(jsonFilePath);
    file << blockData.dump(4);
    file.close();

    acutPrintf(_T("Block created and data saved to JSON."));
}
