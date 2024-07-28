#include "StdAfx.h"
#include "PlaceColumns.h"
#include "SharedDefinations.h"  // For M_PI constants
#include "DefineScale.h"       // For globalVarScale
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>            // For std::transform
#include "dbapserv.h"           // For acdbHostApplicationServices() and related services
#include "dbents.h"             // For AcDbBlockReference
#include "dbsymtb.h"            // For block table record definitions
#include "AcDb.h"               // General database definitions

AcDbObjectId ColumnPlacer::loadAsset(const wchar_t* blockName) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table: %s"), acadErrorStatusText(es));
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blockId;
    es = pBlockTable->getAt(blockName, blockId);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nBlock not found: %s, Error: %s"), blockName, acadErrorStatusText(es));
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    return blockId;
}

AcDbObjectId ColumnPlacer::createCompositeBlock(const wchar_t* newBlockName) {
    std::vector<const wchar_t*> baseBlockNames = {
        L"128281X", L"128295X", L"128265X", L"030110X"
    };

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return AcDbObjectId::kNull;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table for reading: %s"), acadErrorStatusText(es));
        return AcDbObjectId::kNull;
    }

    std::vector<AcDbObjectId> baseBlockIds;
    for (const wchar_t* baseBlockName : baseBlockNames) {
        AcDbObjectId baseBlockId;
        es = pBlockTable->getAt(baseBlockName, baseBlockId);
        if (es != Acad::eOk) {
            acutPrintf(_T("\nBlock not found: %s, Error: %s"), baseBlockName, acadErrorStatusText(es));
            pBlockTable->close();
            return AcDbObjectId::kNull;
        }
        baseBlockIds.push_back(baseBlockId);
    }
    pBlockTable->close();

    // Now open the block table for writing to add the new block
    es = pDb->getBlockTable(pBlockTable, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table for writing: %s"), acadErrorStatusText(es));
        return AcDbObjectId::kNull;
    }

    // Check if the block name already exists
    AcDbObjectId existingBlockId;
    es = pBlockTable->getAt(newBlockName, existingBlockId);
    if (es == Acad::eOk) {
        acutPrintf(_T("\nBlock with the name %s already exists. Using the existing block."), newBlockName);
        pBlockTable->close();
        return existingBlockId;
    }
    else if (es != Acad::eKeyNotFound) {
        acutPrintf(_T("\nFailed to check for existing block: %s"), acadErrorStatusText(es));
        pBlockTable->close();
        return AcDbObjectId::kNull;
    }

    AcDbBlockTableRecord* pNewBlockDef = new AcDbBlockTableRecord();
    pNewBlockDef->setName(newBlockName);

    AcDbObjectId newBlockId;
    es = pBlockTable->add(pNewBlockDef);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to add new block definition: %s"), acadErrorStatusText(es));
        pBlockTable->close();
        delete pNewBlockDef;
        return AcDbObjectId::kNull;
    }

    // Define positions and rotations for the blocks
    struct BlockPlacement {
        AcGePoint3d position;
        double rotation;
    };

    std::vector<BlockPlacement> placements = {
        { AcGePoint3d(0, 0, 0), 0.0 },
        { AcGePoint3d(200, 0, 0), M_PI_2 },
        { AcGePoint3d(200, 200, 0), M_PI },
        { AcGePoint3d(0, 200, 0), 3 * M_PI_2 }
    };

    for (size_t i = 0; i < baseBlockNames.size(); ++i) {
        AcDbObjectId baseBlockId = baseBlockIds[i];
        const wchar_t* baseBlockName = baseBlockNames[i];
        for (const auto& placement : placements) {
            AcDbBlockReference* pBlockRef = new AcDbBlockReference();
            pBlockRef->setPosition(placement.position);
            pBlockRef->setBlockTableRecord(baseBlockId);
            pBlockRef->setRotation(placement.rotation);

            es = pNewBlockDef->appendAcDbEntity(pBlockRef);
            if (es != Acad::eOk) {
                acutPrintf(_T("\nFailed to append block reference %s at position (%f, %f, %f) with rotation %f. Error: %s"),
                    baseBlockName, placement.position.x, placement.position.y, placement.position.z, placement.rotation, acadErrorStatusText(es));
                pBlockRef->close();
                pNewBlockDef->close();
                pBlockTable->close();
                return AcDbObjectId::kNull;
            }
            pBlockRef->close();
        }
    }

    pNewBlockDef->close();
    pBlockTable->close();

    return newBlockId;
}

void ColumnPlacer::placeColumns() {
    // Create the composite block
    AcDbObjectId compositeBlockId = createCompositeBlock(L"Column200*200");
    if (compositeBlockId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to create composite block."));
        return;
    }

    // Get the insertion point from the user
    ads_point insertionPoint;
    int result = acedGetPoint(nullptr, _T("\nSelect insertion point: "), insertionPoint);
    if (result != RTNORM) {
        acutPrintf(_T("\nPoint selection canceled or failed."));
        return;
    }

    AcGePoint3d position(insertionPoint[X], insertionPoint[Y], insertionPoint[Z]);

    // Insert the composite block at the selected position
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return;
    }

    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table: %s"), acadErrorStatusText(es));
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space: %s"), acadErrorStatusText(es));
        pBlockTable->close();
        return;
    }

    AcDbBlockReference* pBlockRef = new AcDbBlockReference();
    pBlockRef->setPosition(position);
    pBlockRef->setBlockTableRecord(compositeBlockId);
    pBlockRef->setScaleFactors(AcGeScale3d(1.0));  // Adjust scale as needed

    es = pModelSpace->appendAcDbEntity(pBlockRef);
    if (es == Acad::eOk) {
        acutPrintf(_T("\nComposite block placed successfully."));
    }
    else {
        acutPrintf(_T("\nFailed to place composite block: %s"), acadErrorStatusText(es));
    }

    pBlockRef->close();
    pModelSpace->close();
    pBlockTable->close();
}
