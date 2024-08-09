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
    std::vector<const wchar_t*> baseBlockNames = { // 45, 75, connector, nut+tie, wingnut, tie120, waler, tie150, tie170, tie200, tie250, tie300
        L"128283X", L"128281X", L"128295X", L"128265X", L"030110X", L"030490X", L"128255X", L"030170X", L"030020X", L"030180X", L"030710X", L"030720X"
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
        },
        { // Placements for Block 030170X (150cm tie)

        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements1234 = { // 50x30
    { // Placements for Block 128283X (45cm panel)
        { AcGePoint3d(0, 0, 0), 0.0 },
        { AcGePoint3d(450, 0, 0), 0.0 },
        { AcGePoint3d(50, 0, 0), M_PI_2 },
        { AcGePoint3d(50, 450, 0), M_PI_2 },
        { AcGePoint3d(50, 30, 0), M_PI },
        { AcGePoint3d(0, 30, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128281X (75cm panel)
        { AcGePoint3d(450, 0, 0), 0.0 },
        { AcGePoint3d(50, 450, 0), M_PI_2 },
        { AcGePoint3d(50, 30, 0), M_PI },
        { AcGePoint3d(0, 30, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128295X (connector)
        { AcGePoint3d(0, -100, 1050), 0.0 },
        { AcGePoint3d(150, 0, 1050), M_PI_2 },
        { AcGePoint3d(50, 130, 1050), M_PI },
        { AcGePoint3d(-100, 30, 1050), 3 * M_PI_2 },
        { AcGePoint3d(0, -100, 300), 0.0 },
        { AcGePoint3d(150, 0, 300), M_PI_2 },
        { AcGePoint3d(50, 130, 300), M_PI },
        { AcGePoint3d(-100, 30, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 128265X (nut+tie)
        { AcGePoint3d(50 - 100, 30 + 125, 1050), 0.0 },
        { AcGePoint3d(-125, 30 - 100, 1050), M_PI_2 },
        { AcGePoint3d(100, -125, 1050), M_PI },
        { AcGePoint3d(50 + 125, 100, 1050), 3 * M_PI_2 },
        { AcGePoint3d(50 - 100, 30 + 125, 300), 0.0 },
        { AcGePoint3d(-125, 30 - 100, 300), M_PI_2 },
        { AcGePoint3d(100, -125, 300), M_PI },
        { AcGePoint3d(50 + 125, 100, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 030110X (wingnut)
        { AcGePoint3d(50 + 125, -145, 1050), 0.0 },
        { AcGePoint3d(50 + 145, 30 + 125, 1050), M_PI_2 },
        { AcGePoint3d(-125, 30 + 145, 1050), M_PI },
        { AcGePoint3d(-145, -125, 1050), 3 * M_PI_2 },
        { AcGePoint3d(50 + 125, -145, 300), 0.0 },
        { AcGePoint3d(50 + 145, 30 + 125, 300), M_PI_2 },
        { AcGePoint3d(-125, 30 + 145, 300), M_PI },
        { AcGePoint3d(-145, -125, 300), 3 * M_PI_2 },
        { AcGePoint3d((50 / 2) + 25, 30 + 145, 1050), M_PI },
        { AcGePoint3d((50 / 2) + 25, -145, 1050), 0.0 },
        { AcGePoint3d(50 + 145, (30 / 2) + 25, 1050), M_PI_2 },
        { AcGePoint3d(-145, (30 / 2) + 25, 1050), 3 * M_PI_2 },
        { AcGePoint3d((50 / 2) + 25, 30 + 145, 300), M_PI },
        { AcGePoint3d((50 / 2) + 25, -145, 300), 0.0 },
        { AcGePoint3d(50 + 145, (30 / 2) + 25, 300), M_PI_2 },
        { AcGePoint3d(-145, (30 / 2) + 25, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 030490X (120cm tie)
        { AcGePoint3d(50 / 2, (30 / 2) + 25, 1050), 0.0 },
        { AcGePoint3d((50 / 2) + 25, 30 / 2, 1050), M_PI_2 },
        { AcGePoint3d(50 / 2, (30 / 2) + 25, 300), 0.0 },
        { AcGePoint3d((50 / 2) + 25, 30 / 2, 300), M_PI_2 }
    },
    { // Placements for Block 128255X (waler)
        { AcGePoint3d(525, -100, 1050), 0.0 },
        { AcGePoint3d(525, -100, 300), 0.0 },
        { AcGePoint3d(50 + 100, 525, 1050), M_PI_2 },
        { AcGePoint3d(50 + 100, 525, 300), M_PI_2 },
        { AcGePoint3d(50 - 525, 30 + 100, 1050), M_PI },
        { AcGePoint3d(50 - 525, 30 + 100, 300), M_PI },
        { AcGePoint3d(-100, 30 - 525, 1050), 3 * M_PI_2 },
        { AcGePoint3d(-100, 30 - 525, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 030170X (150cm tie)

    },
    { // Placements for Block 030020X (170cm tie)

    },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

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
        },
        { // Placements for Block 030170X (150cm tie)

        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

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
            { AcGePoint3d(575, -100, 1050), 0.0 },
            { AcGePoint3d(575, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 575, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 575, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 575, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 575, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 575, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 575, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030170X (150cm tie)

        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements750 = { // 750
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128295X (connector)
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
            { AcGePoint3d(625, -100, 1050), 0.0 },
            { AcGePoint3d(625, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 625, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 625, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 625, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 625, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 625, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 625, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030170X (150cm tie)
            
        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements800 = { // 700
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128295X (connector)
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
            { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },
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
        },
        { // Placements for Block 030170X (150cm tie)
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 1050), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 300), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 300), M_PI_2 },
        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements850 = {
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128295X (connector)
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
            { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d((newBlockDimension / 2), -100, 1050), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 2), 1050), M_PI_2 },
            { AcGePoint3d(-100, (newBlockDimension / 2), 1050), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d((newBlockDimension / 2), -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 2), 300), M_PI_2 },
            { AcGePoint3d(-100, (newBlockDimension / 2), 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030490X (120cm tie)

        },
        { // Placements for Block 128255X (waler)
        },
        { // Placements for Block 030170X (150cm tie)
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2), 1050), 0.0 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension / 2, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2), 300), 0.0 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension / 2, 300), M_PI_2 },
        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements900 = {
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128295X (connector)
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
            { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d((newBlockDimension / 2) - 25, -100, 1050), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 2) + 25, 1050), M_PI_2 },
            { AcGePoint3d(-100, (newBlockDimension / 2) + 25, 1050), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d((newBlockDimension / 2) - 25, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 2) + 25, 300), M_PI_2 },
            { AcGePoint3d(-100, (newBlockDimension / 2) + 25, 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030490X (120cm tie)

        },
        { // Placements for Block 128255X (waler)
        },
        { // Placements for Block 030170X (150cm tie)
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 1050), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 300), 0.0 },
            { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 300), M_PI_2 },
        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements950 = {
        { // Placements for Block 128283X (45cm panel)
            { AcGePoint3d(0, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128281X (75cm panel)
            { AcGePoint3d(450, 0, 0), 0.0 },
            { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
            { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
        },
        { // Placements for Block 128295X (connector)
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
            { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
            { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
            { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
            { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
            { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d((newBlockDimension / 2), -100, 1050), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 2), 1050), M_PI_2 },
            { AcGePoint3d(-100, (newBlockDimension / 2), 1050), 3 * M_PI_2 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d((newBlockDimension / 2), -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 2), 300), M_PI_2 },
            { AcGePoint3d(-100, (newBlockDimension / 2), 300), 3 * M_PI_2 }
        },
        { // Placements for Block 030490X (120cm tie)

        },
        { // Placements for Block 128255X (waler)
        },
        { // Placements for Block 030170X (150cm tie)
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2), 1050), 0.0 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension / 2, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2), 300), 0.0 },
            { AcGePoint3d((newBlockDimension / 2), newBlockDimension / 2, 300), M_PI_2 },
        },
        { // Placements for Block 030020X (170cm tie)

        },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements1000 = {
    { // Placements for Block 128283X (45cm panel)
        { AcGePoint3d(0, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128281X (75cm panel)
        { AcGePoint3d(450, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128295X (connector)
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
        { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
        { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
        { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
        { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
        { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },
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
    },
    { // Placements for Block 030170X (150cm tie)
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 2) + 25, 300), 0.0 },
        { AcGePoint3d((newBlockDimension / 2) - 25, newBlockDimension / 2, 300), M_PI_2 },
    },
    { // Placements for Block 030020X (170cm tie)

    },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements1050 = {
    { // Placements for Block 128283X (45cm panel)

    },
    { // Placements for Block 128281X (75cm panel)
        { AcGePoint3d(0, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
        { AcGePoint3d(750, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128295X (connector)
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
        { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
        { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
        { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
        { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
        { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

        { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension - 500) / 2, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 500) / 2, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension - 500) / 2, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 500) / 2, 300), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension - 500) / 2 + 500, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2 + 500, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 500) / 2 + 500, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension - 500) / 2 + 500, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2 + 500, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 500) / 2 + 500, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 030490X (120cm tie)

    },
    { // Placements for Block 128255X (waler)

    },
    { // Placements for Block 030170X (150cm tie)
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2, 300), 0.0 },
        { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension / 2, 300), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2 + 500, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2 + 500, 300), 0.0 },
        { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension / 2, 300), M_PI_2 },
    },
    { // Placements for Block 030020X (170cm tie)

    },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements1100 = {
    { // Placements for Block 128283X (45cm panel)

    },
    { // Placements for Block 128281X (75cm panel)
        { AcGePoint3d(0, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
        { AcGePoint3d(750, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128295X (connector)
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
        { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
        { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
        { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
        { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
        { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

        { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension - 450) / 2, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 450) / 2, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 450) / 2, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension - 450) / 2, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 450) / 2, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 450) / 2, 300), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension - 450) / 2 + 450, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 450) / 2 + 450, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 450) / 2 + 450, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension - 450) / 2 + 450, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 450) / 2 + 450, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 450) / 2 + 450, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 030490X (120cm tie)

    },
    { // Placements for Block 128255X (waler)

    },
    { // Placements for Block 030170X (150cm tie)
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2, 300), 0.0 },
        { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension / 2, 300), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2 + 450, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2 + 450, 300), 0.0 },
        { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension / 2, 300), M_PI_2 },
    },
    { // Placements for Block 030020X (170cm tie)

    },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements1150 = {
    { // Placements for Block 128283X (45cm panel)

    },
    { // Placements for Block 128281X (75cm panel)
        { AcGePoint3d(0, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
        { AcGePoint3d(750, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128295X (connector)
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
        { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
        { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
        { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
        { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
        { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

        { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension - 400) / 2, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension - 400) / 2, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 300), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 030490X (120cm tie)

    },
    { // Placements for Block 128255X (waler)

    },
    { // Placements for Block 030170X (150cm tie)
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 300), 0.0 },
        { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 300), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 300), 0.0 },
        { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 300), M_PI_2 },
    },
    { // Placements for Block 030020X (170cm tie)

    },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
        };

    std::vector<std::vector<BlockPlacement>> placements1200 = {
    { // Placements for Block 128283X (45cm panel)

    },
    { // Placements for Block 128281X (75cm panel)
        { AcGePoint3d(0, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
        { AcGePoint3d(750, 0, 0), 0.0 },
        { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
        { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
        { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 }
    },
    { // Placements for Block 128295X (connector)
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
        { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
        { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
        { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
        { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
        { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension / 3) - 25, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension / 3) - 25, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 3) + 25, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension / 3) + 25, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension / 3) - 25, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension / 3) - 25, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 3) + 25, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension / 3) + 25, 300), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension / 3) * 2 - 25, newBlockDimension + 100, 1050), M_PI },
        { AcGePoint3d((newBlockDimension / 3) * 2 - 25, -100, 1050), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 3) * 2 + 25, 1050), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension / 3) * 2 + 25, 1050), 3 * M_PI_2 },
        { AcGePoint3d((newBlockDimension / 3) * 2 - 25, newBlockDimension + 100, 300), M_PI },
        { AcGePoint3d((newBlockDimension / 3) * 2 - 25, -100, 300), 0.0 },
        { AcGePoint3d(newBlockDimension + 100, (newBlockDimension / 3) * 2 + 25, 300), M_PI_2 },
        { AcGePoint3d(-100, (newBlockDimension / 3) * 2 + 25, 300), 3 * M_PI_2 }
    },
    { // Placements for Block 030490X (120cm tie)

    },
    { // Placements for Block 128255X (waler)

    },
    { // Placements for Block 030170X (150cm tie)

    },
    { // Placements for Block 030020X (170cm tie)
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 3) + 25, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension / 3) - 25, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 3) + 25, 300), 0.0 },
        { AcGePoint3d((newBlockDimension / 3) - 25, newBlockDimension / 2, 300), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 3) * 2 + 25, 1050), 0.0 },
        { AcGePoint3d((newBlockDimension / 3) * 2 - 25, newBlockDimension / 2, 1050), M_PI_2 },
        { AcGePoint3d(newBlockDimension / 2, (newBlockDimension / 3) * 2 + 25, 300), 0.0 },
        { AcGePoint3d((newBlockDimension / 3) * 2 - 25, newBlockDimension / 2, 300), M_PI_2 },
    },
    { // Placements for Block 030180X (200cm tie)

    },
    { // Placements for Block 030710X (250cm tie)

    },
    { // Placements for Block 030720X (300cm tie)

    }
    };

    std::vector<std::vector<BlockPlacement>> placements1250 = {
{ // Placements for Block 128283X (45cm panel)

},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
    { AcGePoint3d(750, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 300), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler)
            { AcGePoint3d(newBlockDimension - 625, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 625, newBlockDimension + 100, 300), M_PI }
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };

    std::vector<std::vector<BlockPlacement>> placements1300 = {
{ // Placements for Block 128283X (45cm panel)

},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
    { AcGePoint3d(750, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 450) / 2, -145, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 450) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 450) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 450) / 2, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 450) / 2, 300), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 450) / 2, 300), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 450) / 2 + 450, -145, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 450) / 2 + 450, 1050), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 450) / 2 + 450, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 450) / 2 + 450, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 450) / 2 + 450, 300), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 450) / 2 + 450, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
            { AcGePoint3d(650, -100, 1050), 0.0 },
            { AcGePoint3d(650, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 650, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 650, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 650, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 650, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 650, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 650, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 450) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2 + 450, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 450) / 2 + 450, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 450) / 2 + 450, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };

    std::vector<std::vector<BlockPlacement>> placements1350 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -145, 1050), 0.0 },// dim+125 x -145
    { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 1050), M_PI_2 },// dim+145 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 145, 1050), M_PI },//same x dim+145
    { AcGePoint3d(-145, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d(-145, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 500) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2, 300), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 500) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, -145, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 500) / 2 + 500, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 500) / 2 + 500, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 500) / 2 + 500, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 500) / 2 + 500, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
            { AcGePoint3d(1200, -100, 1050), 0.0 },
            { AcGePoint3d(1200, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 1200, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 1200, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 1200, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 1200, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 1200, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 1200, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2 + 500, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2 + 500, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };

    std::vector<std::vector<BlockPlacement>> placements1400 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 550) / 2, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 550) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 550) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 550) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 550) / 2, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 550) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 550) / 2, 300), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 550) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 550) / 2 + 550, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 550) / 2 + 550, -145, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 550) / 2 + 550, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 550) / 2 + 550, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 550) / 2 + 550, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 550) / 2 + 550, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 550) / 2 + 550, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 550) / 2 + 550, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
            { AcGePoint3d(1075, -100, 1050), 0.0 },
            { AcGePoint3d(1075, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 1075, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 1075, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 550) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 550) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 550) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 550) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 550) / 2 + 550, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 550) / 2 + 550, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 550) / 2 + 550, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 550) / 2 + 550, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };


    std::vector<std::vector<BlockPlacement>> placements1450 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 600) / 2, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 600) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 600) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 600) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 600) / 2, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 600) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 600) / 2, 300), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 600) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 600) / 2 + 600, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 600) / 2 + 600, -145, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 600) / 2 + 600, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 600) / 2 + 600, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 600) / 2 + 600, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 600) / 2 + 600, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 600) / 2 + 600, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 600) / 2 + 600, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
            { AcGePoint3d(1075, -100, 1050), 0.0 },
            { AcGePoint3d(1075, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 1075, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 1075, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 600) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 600) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 600) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 600) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 600) / 2 + 600, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 600) / 2 + 600, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 600) / 2 + 600, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 600) / 2 + 600, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };


    std::vector<std::vector<BlockPlacement>> placements1500 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 650) / 2, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 650) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 650) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 650) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 650) / 2, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 650) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 650) / 2, 300), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 650) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 650) / 2 + 650, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 650) / 2 + 650, -145, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 650) / 2 + 650, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 650) / 2 + 650, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 650) / 2 + 650, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 650) / 2 + 650, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 650) / 2 + 650, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 650) / 2 + 650, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
            { AcGePoint3d(1075, -100, 1050), 0.0 },
            { AcGePoint3d(1075, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 1075, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 1075, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 650) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 650) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 650) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 650) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 650) / 2 + 650, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 650) / 2 + 650, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 650) / 2 + 650, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 650) / 2 + 650, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };


    std::vector<std::vector<BlockPlacement>> placements1550 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension + 145, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 700) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 700) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension + 145, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 700) / 2, 300), M_PI_2 },
    { AcGePoint3d(-145, (newBlockDimension - 700) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, -145, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 700) / 2 + 700, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 700) / 2 + 700, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, -145, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 145, (newBlockDimension - 700) / 2 + 700, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 700) / 2 + 700, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
            { AcGePoint3d(1075, -100, 1050), 0.0 },
            { AcGePoint3d(1075, -100, 300), 0.0 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 1050), M_PI_2 },
            { AcGePoint3d(newBlockDimension + 100, 1075, 300), M_PI_2 },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 1050), M_PI },
            { AcGePoint3d(newBlockDimension - 1075, newBlockDimension + 100, 300), M_PI },
            { AcGePoint3d(-100, newBlockDimension - 1075, 1050), 3 * M_PI_2 },
            { AcGePoint3d(-100, newBlockDimension - 1075, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2 + 700, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2 + 700, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };


    std::vector<std::vector<BlockPlacement>> placements1600 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2 + 750, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2 + 750, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2 + 750, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2 + 750, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2 + 750, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2 + 750, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)

}
    };


    std::vector<std::vector<BlockPlacement>> placements1650 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 800) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 800) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 800) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 800) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 800) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 800) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 800) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 800) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 800) / 2 + 800, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 800) / 2 + 800, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 800) / 2 + 800, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 800) / 2 + 800, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 800) / 2 + 800, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 800) / 2 + 800, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 800) / 2 + 800, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 800) / 2 + 800, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 800) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 800) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 800) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 800) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 800) / 2 + 800, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 800) / 2 + 800, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 800) / 2 + 800, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 800) / 2 + 800, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030720X (300cm tie)

}
    };


    std::vector<std::vector<BlockPlacement>> placements1700 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2 + 750, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2 + 750, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 750) / 2 + 750, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 750) / 2 + 750, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2 + 750, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 750) / 2 + 750, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 750) / 2 + 750, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030720X (300cm tie)

}
    };


    std::vector<std::vector<BlockPlacement>> placements1750 = {
{ // Placements for Block 128283X (45cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(450, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 450, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 450, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 450, 0), 3 * M_PI_2 },
    { AcGePoint3d(1200, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1200, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1200, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1200, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 700) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 700) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 700) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 700) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 700) / 2 + 700, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 700) / 2 + 700, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 700) / 2 + 700, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 700) / 2 + 700, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2 + 700, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 700) / 2 + 700, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 700) / 2 + 700, newBlockDimension / 2, 300), M_PI_2 }
},
{ // Placements for Block 030720X (300cm tie)

}
    };

    std::vector<std::vector<BlockPlacement>> placements1800 = {
{ // Placements for Block 128283X (45cm panel)

},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
    { AcGePoint3d(750, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 },
    { AcGePoint3d(1500, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1500, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1500, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1500, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2 + 1200, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2 + 1200, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2 + 1200, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2 + 1200, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2 + 300, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2 + 300, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2 + 300, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2 + 300, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2 + 1200, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2 + 1200, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension / 2, 300), M_PI_2 },

    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2 + 300, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2 + 300, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension / 2, 300), M_PI_2 }
}
    };

    std::vector<std::vector<BlockPlacement>> placements1850 = {
{ // Placements for Block 128283X (45cm panel)

},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
    { AcGePoint3d(750, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 },
    { AcGePoint3d(1500, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1500, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1500, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1500, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2 + 1200, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2 + 1200, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1200) / 2 + 1200, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1200) / 2 + 1200, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2 + 300, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2 + 300, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 300) / 2 + 300, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 300) / 2 + 300, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2 + 1200, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1200) / 2 + 1200, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1200) / 2 + 1200, newBlockDimension / 2, 300), M_PI_2 },

    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2 + 300, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 300) / 2 + 300, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 300) / 2 + 300, newBlockDimension / 2, 300), M_PI_2 }
}
    };

    std::vector<std::vector<BlockPlacement>> placements1900 = {
{ // Placements for Block 128283X (45cm panel)

},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
    { AcGePoint3d(750, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 },
    { AcGePoint3d(1500, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1500, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1500, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1500, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1150) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1150) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1150) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1150) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1150) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1150) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1150) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1150) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1150) / 2 + 1150, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1150) / 2 + 1150, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1150) / 2 + 1150, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1150) / 2 + 1150, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1150) / 2 + 1150, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1150) / 2 + 1150, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1150) / 2 + 1150, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1150) / 2 + 1150, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1150) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1150) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1150) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1150) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1150) / 2 + 1150, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1150) / 2 + 1150, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1150) / 2 + 1150, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1150) / 2 + 1150, newBlockDimension / 2, 300), M_PI_2 },

    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 300), M_PI_2 }
}
    };

    std::vector<std::vector<BlockPlacement>> placements1950 = {
{ // Placements for Block 128283X (45cm panel)

},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
    { AcGePoint3d(750, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 },
    { AcGePoint3d(1500, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1500, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1500, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1500, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1100) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1100) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1100) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1100) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1100) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1100) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1100) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1100) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1100) / 2 + 1100, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1100) / 2 + 1100, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1100) / 2 + 1100, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1100) / 2 + 1100, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1100) / 2 + 1100, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1100) / 2 + 1100, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1100) / 2 + 1100, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1100) / 2 + 1100, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 400) / 2 + 400, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 400) / 2 + 400, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1100) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1100) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1100) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1100) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1100) / 2 + 1100, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1100) / 2 + 1100, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1100) / 2 + 1100, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1100) / 2 + 1100, newBlockDimension / 2, 300), M_PI_2 },

    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 400) / 2 + 400, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 400) / 2 + 400, newBlockDimension / 2, 300), M_PI_2 }
}
    };

    std::vector<std::vector<BlockPlacement>> placements2000 = {
{ // Placements for Block 128283X (45cm panel)

},
{ // Placements for Block 128281X (75cm panel)
    { AcGePoint3d(0, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 0, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension, 0), 3 * M_PI_2 },
    { AcGePoint3d(750, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 750, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 750, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 750, 0), 3 * M_PI_2 },
    { AcGePoint3d(1500, 0, 0), 0.0 },
    { AcGePoint3d(newBlockDimension, 1500, 0), M_PI_2 },
    { AcGePoint3d(newBlockDimension - 1500, newBlockDimension, 0), M_PI },
    { AcGePoint3d(0, newBlockDimension - 1500, 0), 3 * M_PI_2 }
},
{ // Placements for Block 128295X (connector)
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
    { AcGePoint3d(newBlockDimension + 125, -100, 1050), 0.0 },// dim+125 x -100
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 1050), M_PI_2 },// dim+100 x dim+125
    { AcGePoint3d(-125, newBlockDimension + 100, 1050), M_PI },//same x dim+100
    { AcGePoint3d(-100, -125, 1050), 3 * M_PI_2 },//same
    { AcGePoint3d(newBlockDimension + 125, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, newBlockDimension + 125, 300), M_PI_2 },
    { AcGePoint3d(-125, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d(-100, -125, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1250) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1250) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1250) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1250) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1250) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1250) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1250) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1250) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 1250) / 2 + 1250, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 1250) / 2 + 1250, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1250) / 2 + 1250, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1250) / 2 + 1250, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 1250) / 2 + 1250, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 1250) / 2 + 1250, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 1250) / 2 + 1250, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 1250) / 2 + 1250, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 500) / 2, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 500) / 2, 300), 3 * M_PI_2 },

    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension + 100, 1050), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, -100, 1050), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2 + 500, 1050), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 500) / 2 + 500, 1050), 3 * M_PI_2 },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension + 100, 300), M_PI },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, -100, 300), 0.0 },
    { AcGePoint3d(newBlockDimension + 100, (newBlockDimension - 500) / 2 + 500, 300), M_PI_2 },
    { AcGePoint3d(-100, (newBlockDimension - 500) / 2 + 500, 300), 3 * M_PI_2 }
},
{ // Placements for Block 030490X (120cm tie)

},
{ // Placements for Block 128255X (waler) 
},
{ // Placements for Block 030170X (150cm tie)

},
{ // Placements for Block 030020X (170cm tie)

},
{ // Placements for Block 030180X (200cm tie)

},
{ // Placements for Block 030710X (250cm tie)

},
{ // Placements for Block 030720X (300cm tie)
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1250) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1250) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1250) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1250) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1250) / 2 + 1250, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 1250) / 2 + 1250, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 1250) / 2 + 1250, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 1250) / 2 + 1250, newBlockDimension / 2, 300), M_PI_2 },

    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2, newBlockDimension / 2, 300), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2 + 500, 1050), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension / 2, 1050), M_PI_2 },
    { AcGePoint3d(newBlockDimension / 2, (newBlockDimension - 500) / 2 + 500, 300), 0.0 },
    { AcGePoint3d((newBlockDimension - 500) / 2 + 500, newBlockDimension / 2, 300), M_PI_2 }
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
        case 750:
            selectedPlacements = placements750;
            break;
        case 800: // tie 150
            selectedPlacements = placements800;
            break;
        case 850: // no waler
            selectedPlacements = placements850;
            break;
        case 900:
            selectedPlacements = placements900;
            break;
        case 950:
            selectedPlacements = placements950;
            break;
        case 1000: // waler
            selectedPlacements = placements1000;
            break;
        case 1050: // 2 x 75 panel, dual ties, no waler
            selectedPlacements = placements1050;
            break;
        case 1100:
            selectedPlacements = placements1100;
            break;
        case 1150:
            selectedPlacements = placements1150;
            break;
        case 1200: // tie 170
            selectedPlacements = placements1200;
            break;
        case 1250: // waler
            selectedPlacements = placements1250;
            break;
        case 1300:
            selectedPlacements = placements1300;
            break;
        case 1350: // 200 tie, 45, 75, 75
            selectedPlacements = placements1350;
            break;
        case 1400:
            selectedPlacements = placements1400;
            break;
        case 1450:
            selectedPlacements = placements1450;
            break;
        case 1500:
            selectedPlacements = placements1500;
            break;
        case 1550:
            selectedPlacements = placements1550;
            break;
        case 1600: // no waler
            selectedPlacements = placements1600;
            break;
        case 1650: // 250 tie
            selectedPlacements = placements1650;
            break;
        case 1700:
            selectedPlacements = placements1700;
            break;
        case 1750:
            selectedPlacements = placements1750;
            break;
        case 1800: // quad tie, 300 tie
            selectedPlacements = placements1800;
            break;
        case 1850:
            selectedPlacements = placements1850;
            break;
        case 1900:
            selectedPlacements = placements1900;
            break;
        case 1950:
            selectedPlacements = placements1950;
            break;
        case 2000:
            selectedPlacements = placements2000;
            break;
        case 1234:
            selectedPlacements = placements1234;
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

    acutPrintf(_T("\nComposite block created: Column%dx%d%s"), userDimension, userDimension, (userDimension >= 200 && userDimension <= 550) ? L"WithoutTies" : L"WithTies");

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
