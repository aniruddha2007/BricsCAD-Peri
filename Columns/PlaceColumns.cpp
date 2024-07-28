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
#include "Windows.h"            // For Sleep function

// load Assets
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

// create composite block without Ties
AcDbObjectId ColumnPlacer::createCompositeBlock(const wchar_t* newBlockName) {
    acutPrintf(_T("\nStarting creation of composite block: %s"), newBlockName);

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
        acutPrintf(_T("\nLoaded base block: %s"), baseBlockName);
    }
    pBlockTable->close();

    acutPrintf(_T("\nFinished loading base blocks."));

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

    acutPrintf(_T("\nAdded new block definition: %s"), newBlockName);

    // Define positions and rotations for each type of block
    struct BlockPlacement {
        AcGePoint3d position;
        double rotation;
    };

    std::vector<std::vector<BlockPlacement>> allPlacements = {
        { // Placements for Block 128281X
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(200, 0, 0), M_PI_2 },
            { AcGePoint3d(200, 200, 0), M_PI },
            { AcGePoint3d(0, 200, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128295X
            { AcGePoint3d(0, -100, 1050), 0.0 },
            { AcGePoint3d(300, 0, 1050), M_PI_2 },
            { AcGePoint3d(200, 300, 1050), M_PI },
            { AcGePoint3d(-100, 200, 1050), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X
            { AcGePoint3d(100, 325, 1050), 0.0 },
            { AcGePoint3d(-125, 100, 1050), M_PI_2 },
            { AcGePoint3d(100, -125, 1050), M_PI },
            { AcGePoint3d(325, 100, 1050), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X
            { AcGePoint3d(325, -100, 1050), 0.0 },
            { AcGePoint3d(300, 325, 1050), M_PI_2 },
            { AcGePoint3d(-125, 300, 1050), M_PI },
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 }
        }
    };

    for (size_t i = 0; i < baseBlockNames.size(); ++i) {
        AcDbObjectId baseBlockId = baseBlockIds[i];
        const wchar_t* baseBlockName = baseBlockNames[i];
        const auto& placements = allPlacements[i];
        acutPrintf(_T("\nAdding placements for base block: %s"), baseBlockName);
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
            acutPrintf(_T("\nAppended block reference for base block: %s"), baseBlockName);
        }
    }

    pNewBlockDef->close();
    pBlockTable->close();

    acutPrintf(_T("\nFinished creating composite block: %s"), newBlockName);
    return newBlockId;
}

// create composite block with Ties
AcDbObjectId ColumnPlacer::createCompositeBlockWithTies(const wchar_t* newBlockName) {
    acutPrintf(_T("\nStarting creation of composite block: %s"), newBlockName);

    //030490X tie is used for columns750-1050
    std::vector<const wchar_t*> baseBlockNames = {
        L"128281X", L"128295X", L"128265X", L"030110X", L"030490X"
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
        acutPrintf(_T("\nLoaded base block: %s"), baseBlockName);
    }
    pBlockTable->close();

    acutPrintf(_T("\nFinished loading base blocks."));

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

    acutPrintf(_T("\nAdded new block definition: %s"), newBlockName);

    // Define positions and rotations for each type of block
    struct BlockPlacement {
        AcGePoint3d position;
        double rotation;
    };

    std::vector<std::vector<BlockPlacement>> allPlacements = {
        { // Placements for Block 128281X
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(200, 0, 0), M_PI_2 },
            { AcGePoint3d(200, 200, 0), M_PI },
            { AcGePoint3d(0, 200, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128295X
            { AcGePoint3d(0, -100, 1050), 0.0 },
            { AcGePoint3d(300, 0, 1050), M_PI_2 },
            { AcGePoint3d(200, 300, 1050), M_PI },
            { AcGePoint3d(-100, 200, 1050), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X
            { AcGePoint3d(100, 325, 1050), 0.0 },
            { AcGePoint3d(-125, 100, 1050), M_PI_2 },
            { AcGePoint3d(100, -125, 1050), M_PI },
            { AcGePoint3d(325, 100, 1050), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X
            { AcGePoint3d(325, -100, 1050), 0.0 },
            { AcGePoint3d(300, 325, 1050), M_PI_2 },
            { AcGePoint3d(-125, 300, 1050), M_PI },
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 }
        },
		{ // Placements for Block 030490X
			{ AcGePoint3d(0, 0, 0), 0.0 },
			{ AcGePoint3d(200, 0, 0), M_PI_2 },
			{ AcGePoint3d(200, 200, 0), M_PI },
			{ AcGePoint3d(0, 200, 0), 3 * M_PI_2 }
		}
    };

    for (size_t i = 0; i < baseBlockNames.size(); ++i) {
        AcDbObjectId baseBlockId = baseBlockIds[i];
        const wchar_t* baseBlockName = baseBlockNames[i];
        const auto& placements = allPlacements[i];
        acutPrintf(_T("\nAdding placements for base block: %s"), baseBlockName);
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
            acutPrintf(_T("\nAppended block reference for base block: %s"), baseBlockName);
        }
    }

    pNewBlockDef->close();
    pBlockTable->close();

    acutPrintf(_T("\nFinished creating composite block: %s"), newBlockName);
    return newBlockId;
}

// place columns
void ColumnPlacer::placeColumns() {
    acutPrintf(_T("\nStarting to place columns."));

    // Prompt user for dimensions
    int userDimension = 0;
    acedInitGet(RSG_NONULL, NULL);
    if (acedGetInt(_T("\nEnter the dimension (in units): "), &userDimension) != RTNORM) {
        acutPrintf(_T("\nDimension input canceled or invalid."));
        return;
    }

    acutPrintf(_T("\nUser entered dimension: %d"), userDimension);

    // Determine which block creation function to use based on the input dimension
    AcDbObjectId compositeBlockId;
    if (userDimension >= 200 && userDimension <= 550) {
        compositeBlockId = createCompositeBlock(L"Column200x200WithoutTies");
    }
    else {
        compositeBlockId = createCompositeBlockWithTies(L"Column200x200WithTies");
    }

    if (compositeBlockId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to create composite block."));
        return;
    }

    acutPrintf(_T("\nComposite block created: %s"), (userDimension >= 200 && userDimension <= 550) ? L"Column200x200WithoutTies" : L"Column200x200WithTies");

    // Introduce a short delay to ensure block creation is finalized
    Sleep(100);

    // Get the insertion point from the user
    ads_point insertionPoint;
    int result = acedGetPoint(nullptr, _T("\nSelect insertion point: "), insertionPoint);
    if (result != RTNORM) {
        acutPrintf(_T("\nPoint selection canceled or failed."));
        return;
    }

    AcGePoint3d position(insertionPoint[X], insertionPoint[Y], insertionPoint[Z]);

    acutPrintf(_T("\nInsertion point selected: (%f, %f, %f)"), position.x, position.y, position.z);

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
    pBlockRef->setScaleFactors(AcGeScale3d(globalVarScale));  // Adjust scale as needed

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
