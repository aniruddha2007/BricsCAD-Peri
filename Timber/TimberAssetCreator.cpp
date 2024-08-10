#include "StdAfx.h"
#include "TimberAssetCreator.h"
#include "AcDb/AcDb3dSolid.h"
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "aced.h"
#include "geassign.h"
#include <sstream>

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

    // Create a unique name for the timber block based on its dimensions
    std::wstringstream ss;
    ss << L"Timber_" << length << L"x" << height;
    std::wstring blockName = ss.str();

    AcDbObjectId blockId;
    if (pBlockTable->has(blockName.c_str())) {
        if (pBlockTable->getAt(blockName.c_str(), blockId) != Acad::eOk) {
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
    pBlockTableRecord->setName(blockName.c_str());

    es = pBlockTable->add(pBlockTableRecord);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to add block table record. Error: %d"), es);
        pBlockTableRecord->close();
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close(); // Close the block table opened for write

    // Define the 3D solid box for the timber
    AcDb3dSolid* pSolid = new AcDb3dSolid();
    es = pSolid->createBox(length, 10.0, height); // length, width, height (assuming a fixed width of 10.0 units)
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to create 3D solid box. Error: %d"), es);
        delete pSolid;
        pBlockTableRecord->close();
        return AcDbObjectId::kNull;
    }

    // Move the base point to the center
    AcGeMatrix3d moveMatrix;
    moveMatrix.setTranslation(AcGeVector3d(-length / 2.0, -5.0, -height / 2.0));
    pSolid->transformBy(moveMatrix);

    // Append the 3D solid to the block table record
    es = pBlockTableRecord->appendAcDbEntity(pSolid);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to append 3D solid to block table record. Error: %d"), es);
        pSolid->close();
        pBlockTableRecord->close();
        return AcDbObjectId::kNull;
    }

    // Close the 3D solid
    pSolid->close();

    // Close the block table record
    blockId = pBlockTableRecord->objectId();
    pBlockTableRecord->close();

    acutPrintf(_T("\nTimber asset created successfully."));
    return blockId;
}
