// Created by:Ani  (2024-05-31)
// Modified by:Ani (2024-07-04)
//
/////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "TimberAssetCreator.h"
#include "AcDb/AcDb3dSolid.h"
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "aced.h"
#include "geassign.h"

AcDbObjectId TimberAssetCreator::createTimberAsset(double length, double height) {
    acutPrintf(_T("\nCreating timber asset with length: %f and height: %f"), length, height); // Debug output

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table for read. Error: %d"), es);
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blockId;

    if (pBlockTable->has(L"Timber")) {
        if (pBlockTable->getAt(L"Timber", blockId) != Acad::eOk) {
            acutPrintf(_T("\nFailed to get existing timber block."));
            pBlockTable->close();
            return AcDbObjectId::kNull;
        }
        pBlockTable->close();
        acutPrintf(_T("\nUsing existing timber block."));
        return blockId; // Return existing block ID
    }

    pBlockTable->close(); // Close the block table opened for read

    // Ensure the block table is closed before opening it for write
    es = pDb->getBlockTable(pBlockTable, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to open block table for write. Error: %d"), es);
        return AcDbObjectId::kNull;
    }

    // Create a new block table record for the timber block
    AcDbBlockTableRecord* pBlockTableRecord = new AcDbBlockTableRecord();
    pBlockTableRecord->setName(L"Timber");

    es = pBlockTable->add(pBlockTableRecord);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to add block table record. Error: %d"), es);
        pBlockTableRecord->close();
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }


    pBlockTable->close(); // Close the block table opened for read

    // Define the points for the rectangular timber
    AcGePoint3d p1(0, 0, 0);
    AcGePoint3d p2(length, 0, 0);
    AcGePoint3d p3(length, height, 0);
    AcGePoint3d p4(0, height, 0);

    // Create the lines
    AcDbLine* pLine1 = new AcDbLine(p1, p2);
    AcDbLine* pLine2 = new AcDbLine(p2, p3);
    AcDbLine* pLine3 = new AcDbLine(p3, p4);
    AcDbLine* pLine4 = new AcDbLine(p4, p1);

    // Append the lines to the block table record
    es = pBlockTableRecord->appendAcDbEntity(pLine1);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to append line 1 to block table record. Error: %d"), es);
        pLine1->close();
        pBlockTableRecord->close();
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    es = pBlockTableRecord->appendAcDbEntity(pLine2);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to append line 2 to block table record. Error: %d"), es);
        pLine2->close();
        pBlockTableRecord->close();
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    es = pBlockTableRecord->appendAcDbEntity(pLine3);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to append line 3 to block table record. Error: %d"), es);
        pLine3->close();
        pBlockTableRecord->close();
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    es = pBlockTableRecord->appendAcDbEntity(pLine4);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to append line 4 to block table record. Error: %d"), es);
        pLine4->close();
        pBlockTableRecord->close();
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    // Close the lines
    pLine1->close();
    pLine2->close();
    pLine3->close();
    pLine4->close();

    // Close the block table record
    blockId = pBlockTableRecord->objectId();
    pBlockTableRecord->close();
    pBlockTable->close();

    acutPrintf(_T("\nTimber asset created successfully."));
    return blockId;
}