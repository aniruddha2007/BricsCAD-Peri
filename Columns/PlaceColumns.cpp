// PlaceColumns.cpp

#include "StdAfx.h"
#include "PlaceColumns.h"
#include "SharedDefinations.h"
#include "AssetPlacer/GeometryUtils.h"
#include <vector>
#include <limits>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include "aced.h"
#include <cmath>
#include "acutads.h"
#include "acdocman.h"
#include "rxregsvc.h"
#include "geassign.h"
#include <string>
#include <thread>
#include <map>
#include <chrono>

const struct Panel {
    int size;
    std::vector<std::wstring> blockNames;
};

std::map<AcGePoint3d, std::vector<AcGePoint3d>, ColumnPlacer::Point3dComparator> ColumnPlacer::columnMap;

const int BATCH_SIZE = 30;
const double TOLERANCE = 0.1;

AcDbObjectId ColumnPlacer::loadAsset(const wchar_t* blockName) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable = nullptr;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table. Error: %d"), es);
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blockId;
    es = pBlockTable->getAt(blockName, blockId);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block ID for %s. Error: %d"), blockName, es);
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    return blockId;
}

void ColumnPlacer::placeColumn(const AcGePoint3d& position, const int width, const int length) {
    // List of available panels
    std::vector<Panel> panelSizes = {
        {60, {L"128282X", L"136096X", L"129839X"}},
        {45, {L"128283X", L"Null", L"129840X"}},
        {30, {L"128284X", L"Null", L"129841X"}},
        {15, {L"128285X", L"Null", L"129842X"}},
        {10, {L"128292X", L"Null", L"129884X"}},
        {5, {L"128287X", L"Null", L"129879X"}}
    };

    // Define the composite block name
    const wchar_t* blockName = L"CompositePanelBlock";

    // Define panel block names, positions, and rotations
    std::vector<std::wstring> panelNames = { L"128282X", L"128282X", L"128282X", L"128282X" };
    std::vector<AcGePoint3d> positions = { position, AcGePoint3d(position.x + 200, position.y, position.z), AcGePoint3d(position.x + 200, position.y + 200, position.z), AcGePoint3d(position.x, position.y + 200, position.z) };
    std::vector<double> rotations = { 0, M_PI_2, M_PI, 3 * M_PI_2 };

    // Create the composite block
    acutPrintf(_T("\nCreating composite block..."));
    ColumnPlacer::createCompositeBlock(acdbHostApplicationServices()->workingDatabase(), blockName, panelNames, positions, rotations);
    acutPrintf(_T("Composite block created."));

    // Insert the composite block
    acutPrintf(_T("\nInserting composite block..."));
    ColumnPlacer::insertCompositeBlock(acdbHostApplicationServices()->workingDatabase(), blockName, position);
    acutPrintf(_T("Composite block inserted."));
}

void ColumnPlacer::placeColumns() {
    // Implement the logic to place columns here.
    // This is a placeholder for the actual logic to place columns at desired positions.
    acutPrintf(_T("\nHey there, columns."));
    AcGePoint3d position = AcGePoint3d(0, 0, 0);
    int width = 30;
    int length = 30;
    placeColumn(position, width, length);
}

void ColumnPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
    AcDbText* pText = new AcDbText();
    pText->setPosition(position);
    pText->setHeight(5.0);
    pText->setTextString(text);

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        delete pText;
        return;
    }

    AcDbBlockTable* pBlockTable = nullptr;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        delete pText;
        return;
    }

    AcDbBlockTableRecord* pModelSpace = nullptr;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        delete pText;
        return;
    }

    if (pModelSpace->appendAcDbEntity(pText) != Acad::eOk) {
        acutPrintf(_T("\nFailed to append text."));
        pModelSpace->close();
        pBlockTable->close();
        delete pText;
        return;
    }

    pText->close();
    pModelSpace->close();
    pBlockTable->close();
}

void ColumnPlacer::createCompositeBlock(AcDbDatabase* pDb, const wchar_t* blockName, const std::vector<std::wstring>& panelNames, const std::vector<AcGePoint3d>& positions, const std::vector<double>& rotations)
{
    AcDbBlockTable* pBlockTable = nullptr;
    if (pDb->getSymbolTable(pBlockTable, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table for write."));
        return;
    }

    // Check if the block already exists
    AcDbBlockTableRecord* pBlockTableRecord = nullptr;
    if (pBlockTable->has(blockName) == Adesk::kTrue) {
        if (pBlockTable->getAt(blockName, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk) {
            acutPrintf(_T("\nFailed to get existing block table record for write."));
            pBlockTable->close();
            return;
        }
    }
    else {
        pBlockTableRecord = new AcDbBlockTableRecord();
        pBlockTableRecord->setName(blockName);
        if (pBlockTable->add(pBlockTableRecord) != Acad::eOk) {
            acutPrintf(_T("\nFailed to add new block table record."));
            pBlockTableRecord->close();
            pBlockTable->close();
            return;
        }
    }

    // Iterate through panel names, positions, and rotations
    for (size_t i = 0; i < panelNames.size(); ++i) {
        AcDbBlockReference* pBlockRef = new AcDbBlockReference(positions[i], AcDbObjectId::kNull);

        // Set the block name
        AcDbObjectId blockId;
        if (pBlockTable->getAt(panelNames[i].c_str(), blockId) != Acad::eOk) {
            acutPrintf(_T("\nFailed to get block ID for panel: %s"), panelNames[i].c_str());
            delete pBlockRef;
            continue;
        }
        pBlockRef->setBlockTableRecord(blockId);

        // Set the rotation angle
        pBlockRef->setRotation(rotations[i]);

        // Add the block reference to the block table record
        if (pBlockTableRecord->appendAcDbEntity(pBlockRef) != Acad::eOk) {
            acutPrintf(_T("\nFailed to append block reference for panel: %s"), panelNames[i].c_str());
            delete pBlockRef;
            continue;
        }
        pBlockRef->close();
    }

    // Close the block table record
    pBlockTableRecord->close();
    pBlockTable->close();
}

void ColumnPlacer::insertCompositeBlock(AcDbDatabase* pDb, const wchar_t* blockName, const AcGePoint3d& insertPoint)
{
    AcDbBlockTable* pBlockTable = nullptr;
    if (pDb->getSymbolTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table for read."));
        return;
    }

    AcDbBlockTableRecord* pModelSpace = nullptr;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space for write."));
        pBlockTable->close();
        return;
    }

    // Create a block reference for the composite block
    AcDbBlockReference* pBlockRef = new AcDbBlockReference(insertPoint, AcDbObjectId::kNull);
    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block ID for composite block: %s"), blockName);
        delete pBlockRef;
        pModelSpace->close();
        pBlockTable->close();
        return;
    }
    pBlockRef->setBlockTableRecord(blockId);

    // Add the block reference to model space
    if (pModelSpace->appendAcDbEntity(pBlockRef) != Acad::eOk) {
        acutPrintf(_T("\nFailed to append composite block reference to model space."));
        delete pBlockRef;
    }
    else {
        pBlockRef->close();
    }

    pModelSpace->close();
    pBlockTable->close();
}

void insertCompositeBlockCmd()
{
    // Define the composite block name
    const wchar_t* blockName = L"CompositePanelBlock";

    // Define panel block names, positions, and rotations
    std::vector<std::wstring> panelNames = { L"128282X", L"128282X", L"128282X", L"128282X" };
    std::vector<AcGePoint3d> positions = { AcGePoint3d(0, 0, 0), AcGePoint3d(200, 0, 0), AcGePoint3d(200, 200, 0), AcGePoint3d(0, 200, 0) };
    std::vector<double> rotations = { 0, M_PI_2, M_PI, 3 * M_PI_2 };

    // Create the composite block
    acutPrintf(_T("\nCreating composite block..."));
    ColumnPlacer::createCompositeBlock(acdbHostApplicationServices()->workingDatabase(), blockName, panelNames, positions, rotations);
    acutPrintf(_T("Composite block created."));

    // Ask user for the insertion point
    ads_point pt;
    if (acedGetPoint(NULL, L"\nSpecify insertion point: ", pt) == RTNORM) {
        AcGePoint3d insertPoint(pt[X], pt[Y], pt[Z]);

        // Insert the composite block
        acutPrintf(_T("\nInserting composite block..."));
        ColumnPlacer::insertCompositeBlock(acdbHostApplicationServices()->workingDatabase(), blockName, insertPoint);
        acutPrintf(_T("Composite block inserted."));
    }
}

void placeColumnsCmd() {
    acutPrintf(_T("\nPlacing columns..."));
    ColumnPlacer::placeColumns();
    acutPrintf(_T("Columns placed."));
}
