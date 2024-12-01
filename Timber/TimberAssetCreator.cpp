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
    acutPrintf(_T("\nCreating timber asset with length: %f and height: %f"), length, height); 

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
        return blockId; 
    }

    pBlockTable->close(); 

    
    es = pDb->getBlockTable(pBlockTable, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to open block table for write. Error: %d"), es);
        return AcDbObjectId::kNull;
    }

    
    AcDbBlockTableRecord* pBlockTableRecord = new AcDbBlockTableRecord();
    pBlockTableRecord->setName(blockName.c_str());

    es = pBlockTable->add(pBlockTableRecord);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to add block table record. Error: %d"), es);
        pBlockTableRecord->close();
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close(); 

    
    AcDb3dSolid* pSolid = new AcDb3dSolid();
    es = pSolid->createBox(length, 10.0, height); 
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to create 3D solid box. Error: %d"), es);
        delete pSolid;
        pBlockTableRecord->close();
        return AcDbObjectId::kNull;
    }

    
    AcGeMatrix3d moveMatrix;
    moveMatrix.setTranslation(AcGeVector3d(-length / 2.0, -5.0, -height / 2.0));
    pSolid->transformBy(moveMatrix);

    
    es = pBlockTableRecord->appendAcDbEntity(pSolid);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to append 3D solid to block table record. Error: %d"), es);
        pSolid->close();
        pBlockTableRecord->close();
        return AcDbObjectId::kNull;
    }

    
    pSolid->close();

    
    blockId = pBlockTableRecord->objectId();
    pBlockTableRecord->close();

    acutPrintf(_T("\nTimber asset created successfully."));
    return blockId;
}
