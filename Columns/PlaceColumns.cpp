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
AcDbObjectId ColumnPlacer::createCompositeBlock(int newBlockDimension) {
    // Buffer to hold the final block name
    wchar_t newBlockName[50];

    // Create the block name using swprintf
    swprintf(newBlockName, 50, L"Column%dX%dWithoutTies", newBlockDimension, newBlockDimension);

    acutPrintf(_T("\nStarting creation of composite block: %s"), newBlockName);

    std::vector<const wchar_t*> baseBlockNames = { // 75 ,connector, nut , wingnut
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

    std::vector<std::vector<BlockPlacement>> dynamicPlacements = {
        { // Placements for Block 128281X
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },//dim x 0
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },//dim x dim
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }// 0 x dim
        },
        { // Placements for Block 128295X
            { AcGePoint3d(0, -100, 1050), 0.0 },// always same
            { AcGePoint3d(newBlockDimension+100, 0, 1050), M_PI_2 },// 100+dim x 0
            { AcGePoint3d(newBlockDimension, newBlockDimension+100, 1050), M_PI },// dim x 100+dim
            { AcGePoint3d(-100, newBlockDimension, 1050), 3 * M_PI_2 },// -100 x dim
            { AcGePoint3d(0, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 0, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X
            { AcGePoint3d(newBlockDimension-100, newBlockDimension+125, 1050), 0.0 },// dim-100 x dim+125
            { AcGePoint3d(-125, newBlockDimension-100, 1050), M_PI_2 },//same x dim-100
            { AcGePoint3d(100, -125, 1050), M_PI },//same
            { AcGePoint3d(newBlockDimension+125, 100, 1050), 3 * M_PI_2 },//dim+125 x 100
            { AcGePoint3d(newBlockDimension - 100, newBlockDimension + 125, 300), 0.0 },
            { AcGePoint3d(-125, newBlockDimension - 100, 300), M_PI_2 },
            { AcGePoint3d(100, -125, 300), M_PI },
            { AcGePoint3d(newBlockDimension + 125, 100, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X
            { AcGePoint3d(newBlockDimension+125, -100, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension+100, newBlockDimension+125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension+100, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 }
        }
    };

    std::vector<std::vector<BlockPlacement>> d200Placements = {
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
            { AcGePoint3d(-100, 200, 1050), 3 * M_PI_2 },
            { AcGePoint3d(0, -100, 300), 0.0 },
            { AcGePoint3d(300, 0, 300), M_PI_2 },
            { AcGePoint3d(200, 300, 300), M_PI },
            { AcGePoint3d(-100, 200, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X
            { AcGePoint3d(100, 325, 1050), 0.0 },
            { AcGePoint3d(-125, 100, 1050), M_PI_2 },
            { AcGePoint3d(100, -125, 1050), M_PI },
            { AcGePoint3d(325, 100, 1050), 3 * M_PI_2 },
            { AcGePoint3d(100, 325, 300), 0.0 },
            { AcGePoint3d(-125, 100, 300), M_PI_2 },
            { AcGePoint3d(100, -125, 300), M_PI },
            { AcGePoint3d(325, 100, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X
            { AcGePoint3d(325, -100, 1050), 0.0 },
            { AcGePoint3d(300, 325, 1050), M_PI_2 },
            { AcGePoint3d(-125, 300, 1050), M_PI },
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },
            { AcGePoint3d(325, -100, 300), 0.0 },
            { AcGePoint3d(300, 325, 300), M_PI_2 },
            { AcGePoint3d(-125, 300, 300), M_PI },
            { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 }
        }
    };

    std::vector<std::vector<BlockPlacement>> selectedPlacements;
    selectedPlacements = dynamicPlacements;

    for (size_t i = 0; i < baseBlockNames.size(); ++i) {
        AcDbObjectId baseBlockId = baseBlockIds[i];
        const wchar_t* baseBlockName = baseBlockNames[i];
        const auto& placements = selectedPlacements[i];
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
AcDbObjectId ColumnPlacer::createCompositeBlockWithTies(int newBlockDimension) {
    // Buffer to hold the final block name
    wchar_t newBlockName[50];

    // Create the block name using swprintf
    swprintf(newBlockName, 50, L"Column%dX%dWithoutTies", newBlockDimension, newBlockDimension);

    //030490X tie is used for columns600-750
    std::vector<const wchar_t*> baseBlockNames = { // 45, 75, connector, nut+tie, wingnut, tie120
        L"128283X", L"128281X", L"128295X", L"128265X", L"030110X", L"030490X", L"128255X"
    };

    std::vector<const wchar_t*> TieNames = { // wingnut, tie 120, tie 150, tie 170, tie 200, tie 250, tie 300
        L"128283X", L"128281X", L"128295X", L"128265X", L"030110X"
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

    std::vector<std::vector<BlockPlacement>> placements600 = { // 600
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)

        },{ // Placements for Block 128295X (connector)
            { AcGePoint3d(0, -100, 1050), 0.0 },// always same
            { AcGePoint3d(newBlockDimension + 100, 0, 1050), M_PI_2 },// 100+dim x 0
            { AcGePoint3d(newBlockDimension, newBlockDimension + 100, 1050), M_PI },// dim x 100+dim
            { AcGePoint3d(-100, newBlockDimension, 1050), 3 * M_PI_2 },// -100 x dim
            { AcGePoint3d(0, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 0, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X (nut+tie)
            { AcGePoint3d(newBlockDimension - 100, newBlockDimension + 125, 1050), 0.0 },// dim-100 x dim+125
            { AcGePoint3d(-125, newBlockDimension - 100, 1050), M_PI_2 },//same x dim-100
            { AcGePoint3d(100, -125, 1050), M_PI },//same
            { AcGePoint3d(newBlockDimension + 125, 100, 1050), 3 * M_PI_2 },//dim+125 x 100
            { AcGePoint3d(newBlockDimension - 100, newBlockDimension + 125, 300), 0.0 },
            { AcGePoint3d(-125, newBlockDimension - 100, 300), M_PI_2 },
            { AcGePoint3d(100, -125, 300), M_PI },
            { AcGePoint3d(newBlockDimension + 125, 100, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X (wingnut)
            { AcGePoint3d(newBlockDimension + 125, -145, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension + 145, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-145, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -145, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 145, 300), M_PI },
            { AcGePoint3d(-145, -125, 300), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2) + 25, newBlockDimension + 145, 1050), M_PI },
            { AcGePoint3d((newBlockDimension / 2) + 25, -145, 1050), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, (newBlockDimension / 2) + 25, 1050), M_PI_2 },
            { AcGePoint3d(-145, (newBlockDimension / 2) + 25, 1050), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2) + 25, newBlockDimension + 145, 300), M_PI },
            { AcGePoint3d((newBlockDimension / 2) + 25, -145, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, (newBlockDimension / 2) + 25, 300), M_PI_2 },
            { AcGePoint3d(-145, (newBlockDimension / 2) + 25, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030490X (120cm tie)
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 1050), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) + 25, newBlockDimension / 2, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 300), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) + 25, newBlockDimension / 2, 300), M_PI_2 },
        },
        { // Placements for Block 128255X (waler)
            { AcGePoint3d(525, -100, 1050), 0.0 },
            { AcGePoint3d(525, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 525, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 525, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 525, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 525, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 525, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 525, 300), 3 * M_PI_2 }
        }
    };

    std::vector<std::vector<BlockPlacement>> placements650 = { // 650
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)

        },{ // Placements for Block 128295X (connector)
            { AcGePoint3d(0, -100, 1050), 0.0 },// always same
            { AcGePoint3d(newBlockDimension + 100, 0, 1050), M_PI_2 },// 100+dim x 0
            { AcGePoint3d(newBlockDimension, newBlockDimension + 100, 1050), M_PI },// dim x 100+dim
            { AcGePoint3d(-100, newBlockDimension, 1050), 3 * M_PI_2 },// -100 x dim
            { AcGePoint3d(0, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 0, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X (nut+tie)
            { AcGePoint3d(newBlockDimension - 100, newBlockDimension + 125, 1050), 0.0 },// dim-100 x dim+125
            { AcGePoint3d(-125, newBlockDimension - 100, 1050), M_PI_2 },//same x dim-100
            { AcGePoint3d(100, -125, 1050), M_PI },//same
            { AcGePoint3d(newBlockDimension + 125, 100, 1050), 3 * M_PI_2 },//dim+125 x 100
            { AcGePoint3d(newBlockDimension - 100, newBlockDimension + 125, 300), 0.0 },
            { AcGePoint3d(-125, newBlockDimension - 100, 300), M_PI_2 },
            { AcGePoint3d(100, -125, 300), M_PI },
            { AcGePoint3d(newBlockDimension + 125, 100, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X (wingnut)
            { AcGePoint3d(newBlockDimension + 125, -145, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension + 145, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-145, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -145, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 145, 300), M_PI },
            { AcGePoint3d(-145, -125, 300), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension + 145, 1050), M_PI },
            { AcGePoint3d((newBlockDimension / 2), -145, 1050), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, (newBlockDimension / 2), 1050), M_PI_2 },
            { AcGePoint3d(-145, (newBlockDimension / 2), 1050), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension + 145, 300), M_PI },
            { AcGePoint3d((newBlockDimension / 2), -145, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, (newBlockDimension / 2), 300), M_PI_2 },
            { AcGePoint3d(-145, (newBlockDimension / 2), 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030490X (120cm tie)
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2), 1050), 0.0 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension / 2, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2), 300), 0.0 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension / 2, 300), M_PI_2 },
        },
        { // Placements for Block 128255X (waler)
            { AcGePoint3d(525, -100, 1050), 0.0 },
            { AcGePoint3d(525, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 525, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 525, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 525, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 525, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 525, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 525, 300), 3 * M_PI_2 }
        }
    };

    std::vector<std::vector<BlockPlacement>> placements700 = { // 700
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)

        },{ // Placements for Block 128295X (connector)
            { AcGePoint3d(0, -100, 1050), 0.0 },// always same
            { AcGePoint3d(newBlockDimension + 100, 0, 1050), M_PI_2 },// 100+dim x 0
            { AcGePoint3d(newBlockDimension, newBlockDimension + 100, 1050), M_PI },// dim x 100+dim
            { AcGePoint3d(-100, newBlockDimension, 1050), 3 * M_PI_2 },// -100 x dim
            { AcGePoint3d(0, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 0, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X (nut+tie)
            { AcGePoint3d(newBlockDimension - 100, newBlockDimension + 125, 1050), 0.0 },// dim-100 x dim+125
            { AcGePoint3d(-125, newBlockDimension - 100, 1050), M_PI_2 },//same x dim-100
            { AcGePoint3d(100, -125, 1050), M_PI },//same
            { AcGePoint3d(newBlockDimension + 125, 100, 1050), 3 * M_PI_2 },//dim+125 x 100
            { AcGePoint3d(newBlockDimension - 100, newBlockDimension + 125, 300), 0.0 },
            { AcGePoint3d(-125, newBlockDimension - 100, 300), M_PI_2 },
            { AcGePoint3d(100, -125, 300), M_PI },
            { AcGePoint3d(newBlockDimension + 125, 100, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X (wingnut)
            { AcGePoint3d(newBlockDimension + 125, -145, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension + 145, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-145, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -145, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 145, 300), M_PI },
            { AcGePoint3d(-145, -125, 300), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension + 145, 1050), M_PI },
            { AcGePoint3d((newBlockDimension / 2) - 25, -145, 1050), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, (newBlockDimension / 2) + 25, 1050), M_PI_2 },
            { AcGePoint3d(-145, (newBlockDimension / 2) + 25, 1050), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension + 145, 300), M_PI },
            { AcGePoint3d((newBlockDimension / 2) - 25, -145, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 145, (newBlockDimension / 2) + 25, 300), M_PI_2 },
            { AcGePoint3d(-145, (newBlockDimension / 2) + 25, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030490X (120cm tie)
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 1050), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 300), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 300), M_PI_2 },
        },
        { // Placements for Block 128255X (waler)
            { AcGePoint3d(525, -100, 1050), 0.0 },
            { AcGePoint3d(525, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 525, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 525, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 525, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 525, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 525, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 525, 300), 3 * M_PI_2 }
        }
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
            { AcGePoint3d(-100, 200, 1050), 3 * M_PI_2 },
            { AcGePoint3d(0, -100, 300), 0.0 },
            { AcGePoint3d(300, 0, 300), M_PI_2 },
            { AcGePoint3d(200, 300, 300), M_PI },
            { AcGePoint3d(-100, 200, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 128265X
            { AcGePoint3d(100, 325, 1050), 0.0 },
            { AcGePoint3d(-125, 100, 1050), M_PI_2 },
            { AcGePoint3d(100, -125, 1050), M_PI },
            { AcGePoint3d(325, 100, 1050), 3 * M_PI_2 },
            { AcGePoint3d(100, 325, 300), 0.0 },
            { AcGePoint3d(-125, 100, 300), M_PI_2 },
            { AcGePoint3d(100, -125, 300), M_PI },
            { AcGePoint3d(325, 100, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030110X
            { AcGePoint3d(325, -100, 1050), 0.0 },
            { AcGePoint3d(300, 325, 1050), M_PI_2 },
            { AcGePoint3d(-125, 300, 1050), M_PI },
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },
            { AcGePoint3d(325, -100, 300), 0.0 },
            { AcGePoint3d(300, 325, 300), M_PI_2 },
            { AcGePoint3d(-125, 300, 300), M_PI },
            { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030490X
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(200, 0, 0), M_PI_2 },
            { AcGePoint3d(200, 200, 0), M_PI },
            { AcGePoint3d(0, 200, 0), 3 * M_PI_2 }
        }
    };

    std::vector<std::vector<BlockPlacement>> selectedPlacements;
    switch (newBlockDimension) {
        case 600:
            selectedPlacements = placements600;
            break;
        case 650:
            selectedPlacements = placements650;
            break;
        case 700:
            selectedPlacements = placements700;
            break;
    }
    

    for (size_t i = 0; i < baseBlockNames.size(); ++i) {
        AcDbObjectId baseBlockId = baseBlockIds[i];
        const wchar_t* baseBlockName = baseBlockNames[i];
        const auto& placements = selectedPlacements[i];
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
        compositeBlockId = createCompositeBlock(userDimension);
    }
    else {
        compositeBlockId = createCompositeBlockWithTies(userDimension);
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
