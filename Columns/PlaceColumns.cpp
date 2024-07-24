// Created: by Ani (2024-07-24)
// PlaceColumns.cpp

#include "StdAfx.h"
#include "PlaceColumns.h"
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
#include <chrono>

std::map<AcGePoint3d, std::vector<AcGePoint3d>, ColumnPlacer::Point3dComparator> ColumnPlacer::columnMap;

const int BATCH_SIZE = 30;
const double TOLERANCE = 0.1;

// Structure to hold panel information
struct Panel {
    int length;
    std::wstring id[3];
};

AcDbObjectId ColumnPlacer::loadAsset(const wchar_t* blockName) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) return AcDbObjectId::kNull;

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) return AcDbObjectId::kNull;

    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    return blockId;
}

void ColumnPlacer::placeColumn(const AcGePoint3d& position, const int width, const int length) {
    // List of available panels
    std::vector<Panel> panelSizes = {
        /* {90, {L"128280X", L"Null", L"129837X"}},*/ // ONLY ENABLE FOR 90 PANELS
        /*{75, {L"128281X", L"Null", L"129838X"}}, */ // ONLY ENABLE FOR 75 PANELS
        {60, {L"128282X", L"136096X", L"129839X"}},
        {45, {L"128283X", L"Null", L"129840X"}},
        {30, {L"128284X", L"Null", L"129841X"}},
        {15, {L"128285X", L"Null", L"129842X"}},
        {10, {L"128292X", L"Null", L"129884X"}}, // *10 Compensator move to middle TODO:
        {5, {L"128287X", L"Null", L"129879X"}} // *5 Compensator add a break
    };
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

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        delete pText;
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
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
