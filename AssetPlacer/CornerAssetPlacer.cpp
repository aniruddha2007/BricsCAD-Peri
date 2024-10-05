// Created by:Ani  (2024-05-31)
// Modified by:Ani (2024-07-04)
// TODO:
// CornerAssetPlacer.cpp
// This file contains the implementation of the CornerAssetPlacer class.
// The CornerAssetPlacer class is used to place assets at corners in BricsCAD.
// The detectPolylines function is used to detect polylines in the drawing.
// The addTextAnnotation function is used to add text annotations to the drawing.
// The placeAssetsAtCorners function is used to place assets at the detected corners.
// The loadAsset function is used to load an asset from the block table.
// The placeInsideCornerPostAndPanels function is used to place assets at inside corners.
// The placeOutsideCornerPostAndPanels function is used to place assets at outside corners.
// The recreateModelSpace function is used to recreate the model space.
// The CornerAssetPlacer class is part of the AssetPlacer namespace.
/////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CornerAssetPlacer.h"
#include "SharedDefinations.h"
#include "GeometryUtils.h"
#include "SharedConfigs.h"
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <limits>
#include <string>
#include <chrono>
#include <thread>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include <AcDb/AcDbBlockTable.h>
#include <AcDb/AcDbBlockTableRecord.h>
#include <AcDb/AcDbPolyline.h>
#include "gepnt3d.h"
#include "DefineHeight.h"
#include "DefineScale.h" 

// Static member definition
std::map<AcGePoint3d, std::vector<AcGePoint3d>, CornerAssetPlacer::Point3dComparator> CornerAssetPlacer::wallMap;

const int BATCH_SIZE = 30; // Process 30 entities at a time

const double TOLERANCE = 0.19; // Tolerance for angle comparison

// ADD TEXT ANNOTATION TO DRAWING__ Only enable for debugging
//void CornerAssetPlacer::addTextAnnotation(const AcGePoint3d& position, const wchar_t* text) {
//    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
//    if (!pDb) {
//        acutPrintf(_T("\nNo working database found."));
//        return;
//    }
//
//    AcDbBlockTable* pBlockTable;
//    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
//    if (es != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get block table. Error status: %d\n"), es);
//        return;
//    }
//
//    AcDbBlockTableRecord* pModelSpace;
//    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
//    if (es != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get model space. Error status: %d\n"), es);
//        pBlockTable->close();
//        return;
//    }
//
//    AcDbText* pText = new AcDbText(position, text, AcDbObjectId::kNull, 0.2, 0);
//    es = pModelSpace->appendAcDbEntity(pText);
//    if (es == Acad::eOk) {
//        acutPrintf(_T("| Added text annotation: %s"), text);
//    }
//    else {
//        acutPrintf(_T("\nFailed to add text annotation. Error status: %d\n"), es);
//    }
//    pText->close();  // Decrement reference count
//
//    pModelSpace->close();  // Decrement reference count
//    pBlockTable->close();  // Decrement reference count
//}

// LOAD ASSET FROM BLOCK TABLE
AcDbObjectId CornerAssetPlacer::loadAsset(const wchar_t* blockName) {
    if (wcslen(blockName) == 0) {
        return AcDbObjectId::kNull;
    }
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) return AcDbObjectId::kNull;

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) return AcDbObjectId::kNull;

    AcDbObjectId blockId;
    if (pBlockTable->getAt(blockName, blockId) != Acad::eOk) {
        pBlockTable->close();
        acutPrintf(_T("\nFailed to get block ID for block name: %s"), blockName);
        return AcDbObjectId::kNull;
    }

    pBlockTable->close();
    return blockId;
}

//struct PanelDimensions {
//    std::vector<Panel> panels;
//
//    PanelDimensions() {
//        // Initialize with the given panel dimensions and block names
//        panels.push_back(Panel(50, 100, 1350, L"128287X"));
//        panels.push_back(Panel(100, 100, 1350, L"128292X"));
//        panels.push_back(Panel(150, 100, 1350, L"128285X"));
//        panels.push_back(Panel(300, 100, 1350, L"128284X"));
//        panels.push_back(Panel(450, 100, 1350, L"128283X"));
//        panels.push_back(Panel(600, 100, 1350, L"128282X"));
//        panels.push_back(Panel(750, 100, 1350, L"128281X"));
//    }
//
//    // Function to get panel by width (if needed)
//    Panel* getPanelByWidth(double width) {
//        for (auto& panel : panels) {
//            if (panel.width == width) {
//                return &panel;
//            }
//        }
//        return nullptr;  // Return nullptr if no matching panel is found
//    }
//};

//Panel Configurations according to the distance
PanelConfig CornerAssetPlacer::getPanelConfig(double distance, PanelDimensions& panelDims) {
    PanelConfig config = {};

    if (distance == 150) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
    }
    else if (distance == 200) {
        config.panelIdA = panelDims.getPanelByWidth(150);
		config.panelIdB = panelDims.getPanelByWidth(150);
		config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
		config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);

    }
    else if (distance == 250) {
        config.panelIdA = panelDims.getPanelByWidth(150);
		config.panelIdB = panelDims.getPanelByWidth(150);
		config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
		config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
		config.compensatorIdA = panelDims.getPanelByWidth(50);
		config.compensatorIdB = panelDims.getPanelByWidth(50);
    }
    else if (distance == 300) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
    }
    else if (distance == 350) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
    }
    else if (distance == 400) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
    }
    else if (distance == 450) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
    }
    else if (distance == 500) {
		config.panelIdA = panelDims.getPanelByWidth(150);
		config.panelIdB = panelDims.getPanelByWidth(150);
		config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
		config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
		config.compensatorIdA = panelDims.getPanelByWidth(0);
		config.compensatorIdB = panelDims.getPanelByWidth(0);
	}
	else if (distance == 550) {
		config.panelIdA = panelDims.getPanelByWidth(150);
		config.panelIdB = panelDims.getPanelByWidth(150);
		config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
		config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
		config.compensatorIdA = panelDims.getPanelByWidth(50);
		config.compensatorIdB = panelDims.getPanelByWidth(50);
	}
	else if (distance == 600) {
		config.panelIdA = panelDims.getPanelByWidth(150);
		config.panelIdB = panelDims.getPanelByWidth(150);
		config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
		config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
		config.compensatorIdA = panelDims.getPanelByWidth(100);
		config.compensatorIdB = panelDims.getPanelByWidth(100);
	}
	else if (distance == 650) {
		config.panelIdA = panelDims.getPanelByWidth(150);
		config.panelIdB = panelDims.getPanelByWidth(150);
		config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
		config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
	}
    else if (distance == 700) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
    }
    else if (distance == 750) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
    }
    else if (distance == 800) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(300);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(300);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
    }
    else if (distance == 850) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(300);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(300);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 900) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(300);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(300);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 950) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
        }
    else if (distance == 1000) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 1050) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 1100) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
        }
    else if (distance == 1150) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 1200) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 1250) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
        }
    else if (distance == 1300) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 1350) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(0);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(0);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 1400) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
        }
    else if (distance == 1450) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 1500) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 1550) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
        }
    else if (distance == 1600) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 1650) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 1700) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
    }
    else if (distance == 1750) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 1800) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(450);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 1850) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
        }
    else if (distance == 1900) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 1950) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(600);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
        }
    else if (distance == 2000) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(0);
        config.compensatorIdB = panelDims.getPanelByWidth(0);
        }
    else if (distance == 2050) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(50);
        config.compensatorIdB = panelDims.getPanelByWidth(50);
        }
    else if (distance == 2100) {
        config.panelIdA = panelDims.getPanelByWidth(150);
        config.panelIdB = panelDims.getPanelByWidth(150);
        config.outsidePanelIds[0] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[1] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[2] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[3] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[4] = panelDims.getPanelByWidth(750);
        config.outsidePanelIds[5] = panelDims.getPanelByWidth(750);
        config.compensatorIdA = panelDims.getPanelByWidth(100);
        config.compensatorIdB = panelDims.getPanelByWidth(100);
    }

    // Continue adding other cases...


        // Debug output to verify panel assignment
        //acutPrintf(_T("\nPanel Configuration for Distance %f:"), distance);
        //for (int i = 0; i < 6; ++i) {
        //    if (config.outsidePanelIds[i]) {
        //        acutPrintf(_T("\nOutside Panel %d: Block Name: %s, Width: %f"), i, config.outsidePanelIds[i]->blockName.c_str(), config.outsidePanelIds[i]->width);
        //    }
        //    else {
        //        acutPrintf(_T("\nOutside Panel %d: Block not assigned or is null."), i);
        //    }
        //}
        //if (config.compensatorIdA) {
        //    acutPrintf(_T("\nCompensator A: Block Name: %s, Width: %f"), config.compensatorIdA->blockName.c_str(), config.compensatorIdA->width);
        //}
        //else {
        //    acutPrintf(_T("\nCompensator A: Block not assigned or is null."));
        //}
        //if (config.compensatorIdB) {
        //    acutPrintf(_T("\nCompensator B: Block Name: %s, Width: %f"), config.compensatorIdB->blockName.c_str(), config.compensatorIdB->width);
        //}
        //else {
        //    acutPrintf(_T("\nCompensator B: Block not assigned or is null."));
        //}

    return config;
}

std::vector<CornerConfig> CornerAssetPlacer::generateCornerConfigs(const std::vector<AcGePoint3d>& corners, const PanelConfig& config) {
    std::vector<CornerConfig> cornerConfigs;

    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        CornerConfig cornerConfig;
        cornerConfig.position = corners[cornerNum];

        // Calculate previous and next points relative to the current corner
        AcGePoint3d prev = corners[(cornerNum + corners.size() - 1) % corners.size()];
        AcGePoint3d next = corners[(cornerNum + 1) % corners.size()];

        cornerConfig.startPoint = prev;
        cornerConfig.endPoint = next;

        AcGeVector3d prevDirection = (cornerConfig.position - prev).normal();
        AcGeVector3d nextDirection = (next - cornerConfig.position).normal();

        // Determine if it's an inside (concave) or outside (convex) corner
        double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;

        cornerConfig.isInside = crossProductZ < 0;  // True if it's an inside corner

        if (!cornerConfig.isInside) {
            // Calculate the adjustment for outside corners
            double adjustment = 0.0;
            if (config.outsidePanelIds[0]) adjustment += config.outsidePanelIds[0]->width;
            if (config.outsidePanelIds[2]) adjustment += config.outsidePanelIds[2]->width;
            if (config.outsidePanelIds[4]) adjustment += config.outsidePanelIds[4]->width;
            if (config.compensatorIdA) adjustment += config.compensatorIdA->width;

            cornerConfig.outsideCornerAdjustment = adjustment;
        }
        else {
            cornerConfig.outsideCornerAdjustment = 250.0;  // For inside corners, use a fixed adjustment
        }

        // Debug output
        //acutPrintf(_T("\nCorner %d: Position: %f, %f, Is Inside: %d, Adjustment: %f"),
        //    cornerNum, cornerConfig.position.x, cornerConfig.position.y, cornerConfig.isInside, cornerConfig.outsideCornerAdjustment);

        cornerConfigs.push_back(cornerConfig);
    }

    return cornerConfigs;
}

// Function to check if a double value is an integer within a tolerance
bool isItInteger(double value, double tolerance = 1e-9) {
    return std::abs(value - std::round(value)) < tolerance;
}

// Function to recreate the model space
bool recreateModelSpace(AcDbDatabase* pDb) {
    AcDbBlockTable* pBlockTable;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table for write access. Error status: %d\n"), es);
        return false;
    }

    AcDbBlockTableRecord* pModelSpace;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space for write access. Error status: %d\n"), es);
        pBlockTable->close();
        return false;
    }

    es = pModelSpace->erase();
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to erase model space. Error status: %d\n"), es);
        pModelSpace->close();
        pBlockTable->close();
        return false;
    }
    pModelSpace->close();

    AcDbBlockTableRecord* pNewModelSpace = new AcDbBlockTableRecord();
    pNewModelSpace->setName(ACDB_MODEL_SPACE);
    es = pBlockTable->add(pNewModelSpace);
    if (es != Acad::eOk) {
        acutPrintf(_T("\nFailed to add new model space. Error status: %d\n"), es);
        pNewModelSpace->close();
        pBlockTable->close();
        return false;
    }
    pNewModelSpace->close();
    pBlockTable->close();

    return true;
}

std::vector<AcGePoint3d> CornerAssetPlacer::detectPolylines() {
    //acutPrintf(_T("\nDetecting polylines..."));
    std::vector<AcGePoint3d> corners;
    wallMap.clear();  // Clear previous data

    auto pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return corners;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return corners;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return corners;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return corners;
    }

    int entityCount = 0;
    while (!pIter->done()) {
        AcDbEntity* pEnt;
        Acad::ErrorStatus es = pIter->getEntity(pEnt, AcDb::kForRead);
        if (es == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                AcDbPolyline* pPolyline = AcDbPolyline::cast(pEnt);
                if (pPolyline) {
                    processPolyline(pPolyline, corners, 90.0, TOLERANCE);  // Assuming 90.0 degrees as the threshold for corners
                }
            }
            pEnt->close();
            entityCount++;

            if (entityCount % BATCH_SIZE == 0) {
                //acutPrintf(_T("\nProcessed %d entities. Pausing to avoid resource exhaustion.\n"), entityCount);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Adjusted sleep time for potentially better performance
            }
        }
        else {
            acutPrintf(_T("\nFailed to get entity. Error status: %d\n"), es);
        }

        pIter->step();
    }

    // Filter out extra pairs of corners
    filterClosePoints(corners, TOLERANCE);

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    //acutPrintf(_T("\nDetected %d corners from polylines."), corners.size());
    return corners;
}

bool arePerpendicular(const AcGeVector3d& v1, const AcGeVector3d& v2, double tolerance = TOLERANCE) {
    // Calculate the cross product of the two vectors
    AcGeVector3d crossProduct = v1.crossProduct(v2);

    // Check if the magnitude of the cross product is close to zero within the tolerance
    return crossProduct.length() < tolerance;
}

double crossProduct2(const AcGePoint3d& o, const AcGePoint3d& a, const AcGePoint3d& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool directionOfDrawing2(std::vector<AcGePoint3d>& points) {
    // Ensure the shape is closed
    if (!(points.front().x == points.back().x && points.front().y == points.back().y)) {
        points.push_back(points.front());
    }

    double totalTurns = 0.0;

    for (size_t i = 1; i < points.size() - 1; ++i) {
        totalTurns += crossProduct2(points[i - 1], points[i], points[i + 1]);
    }

    return totalTurns > 0.0;
}

//Function to calculate the distance between two polylines
double CornerAssetPlacer::calculateDistanceBetweenPolylines() {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        return -1.0;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        return -1.0;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        pBlockTable->close();
        return -1.0;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        pModelSpace->close();
        pBlockTable->close();
        return -1.0;
    }

    AcDbPolyline* pFirstPolyline = nullptr;
    AcDbPolyline* pSecondPolyline = nullptr;

    // Find the first two polylines
    for (pIter->start(); !pIter->done(); pIter->step()) {
        AcDbEntity* pEnt;
        if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk) {
            if (pEnt->isKindOf(AcDbPolyline::desc())) {
                if (!pFirstPolyline) {
                    pFirstPolyline = AcDbPolyline::cast(pEnt);
                }
                else if (!pSecondPolyline) {
                    pSecondPolyline = AcDbPolyline::cast(pEnt);
                    pEnt->close();
                    break; // Found both polylines, no need to continue
                }
            }
            pEnt->close();
        }
    }

    double distance = -1.0;
    if (pFirstPolyline && pSecondPolyline) {
        distance = getPolylineDistance(pFirstPolyline, pSecondPolyline);
    }

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();
    return distance;
}

int CornerAssetPlacer::identifyFirstLoopEnd(const std::vector<AcGePoint3d>& corners) {
    int closeLoopCounter = -1;
    int loopIndex = 0;
    double outerPointCounter = corners[0].x;
    int outerLoopIndexValue = 0;
    int firstLoopEnd = corners.size() - 1;

    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        closeLoopCounter++;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[cornerNum + 1];
        AcGeVector3d direction = (end - start).normal();

        if (start.x > outerPointCounter) {
            outerPointCounter = start.x;
            outerLoopIndexValue = loopIndex;
        }

        if (!isItInteger(direction.x) || !isItInteger(direction.y)) {
            if (cornerNum < corners.size() - 1) {
                closeLoopCounter = -1;
                loopIndex = 1;
                firstLoopEnd = cornerNum;
            }
        }
    }

    return firstLoopEnd;
}

// Function to split the corners into two loops based on the first loop end
std::pair<std::vector<AcGePoint3d>, std::vector<AcGePoint3d>> CornerAssetPlacer::splitLoops(
    const std::vector<AcGePoint3d>& corners, int firstLoopEnd) {

    std::vector<AcGePoint3d> firstLoop(corners.begin(), corners.begin() + firstLoopEnd + 1);
    std::vector<AcGePoint3d> secondLoop(corners.begin() + firstLoopEnd + 1, corners.end());

    return { firstLoop, secondLoop };
}

// Function to determine if a loop is clockwise or counterclockwise
//void CornerAssetPlacer::processCorners(
//    const std::vector<AcGePoint3d>& corners, AcDbObjectId cornerPostId, const PanelConfig& config,
//    double distance, const std::vector<bool>& loopIsClockwise, const std::vector<bool>& isInsideLoop) {
//
//    int loopIndex = 0;
//    int loopIndexLastPanel = 0;
//    int closeLoopCounter = -1;
//    int outerLoopIndexValue = 0;
//
//    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
//        // Convert Panels to AcDbObjectId using loadAsset
//        AcDbObjectId panelIdA = loadAsset(config.panelIdA->blockName.c_str());
//        AcDbObjectId panelIdB = loadAsset(config.panelIdB->blockName.c_str());
//        AcDbObjectId compensatorIdA = loadAsset(config.compensatorIdA->blockName.c_str());
//        AcDbObjectId compensatorIdB = loadAsset(config.compensatorIdB->blockName.c_str());
//
//        // For outside corner panels
//        AcDbObjectId outsidePanelIds[6];
//        for (int i = 0; i < 6; ++i) {
//            if (config.outsidePanelIds[i] && config.outsidePanelIds[i]->width > 0) {  // Skip dummy panels
//                outsidePanelIds[i] = loadAsset(config.outsidePanelIds[i]->blockName.c_str());
//                if (outsidePanelIds[i] == AcDbObjectId::kNull) {
//                    acutPrintf(_T("\nFailed to load outside panel asset %d."), i);
//                }
//            }
//            else {
//                outsidePanelIds[i] = AcDbObjectId::kNull;  // Assign null for dummy panels
//            }
//        }
//        AcDbObjectId outsideCompensatorIdA = loadAsset(config.compensatorIdA->blockName.c_str());
//        AcDbObjectId outsideCompensatorIdB = loadAsset(config.compensatorIdB->blockName.c_str());
//
//        double rotation = 0.0;
//        AcGePoint3d start = corners[cornerNum];
//        AcGePoint3d end = corners[(cornerNum + 1) % corners.size()]; // Wrap around the loop
//        AcGeVector3d direction = (end - start).normal();
//
//        closeLoopCounter++;
//
//        // Determine if the corner is inside or outside based on loop membership
//        bool isInside = isInsideLoop[loopIndex];
//
//        if (!isItInteger(direction.x) || !isItInteger(direction.y)) {
//            if (cornerNum < corners.size() - 1) {
//                start = corners[cornerNum];
//                end = corners[cornerNum - closeLoopCounter];
//                closeLoopCounter = -1;
//                loopIndexLastPanel = 1;
//            }
//        }
//
//        direction = (end - start).normal();
//        acutPrintf(_T("\nCorner %d: %f, %f"), cornerNum, corners[cornerNum].x, corners[cornerNum].y);
//        acutPrintf(_T("\nrotation: %f"), atan2(direction.y, direction.x));
//        rotation = atan2(direction.y, direction.x);
//        rotation = normalizeAngle(rotation);
//        acutPrintf(_T("\nrotation: %f"), rotation);
//        rotation = snapToExactAngle(rotation, TOLERANCE);
//        acutPrintf(_T("\nrotation: %f"), rotation);
//
//        adjustRotationForCorner(rotation, corners, cornerNum);
//
//        if (isInside) {
//            placeInsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, distance, compensatorIdA, compensatorIdB);
//        }
//        else {
//            placeOutsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, config, outsidePanelIds[0], outsidePanelIds[1], outsidePanelIds[2], outsidePanelIds[3], outsidePanelIds[4], outsidePanelIds[5], outsideCompensatorIdA, outsideCompensatorIdB, distance);
//        }
//
//        loopIndex = loopIndexLastPanel;
//    }
//
//}

// Function to adjust the rotation for a corner based on the direction of the corner
void CornerAssetPlacer::adjustRotationForCorner(double& rotation, const std::vector<AcGePoint3d>& corners, size_t cornerNum) {
    AcGeVector3d prevDirection = corners[cornerNum] - corners[cornerNum > 0 ? cornerNum - 1 : corners.size() - 1];
    AcGeVector3d nextDirection = corners[(cornerNum + 1) % corners.size()] - corners[cornerNum];
    double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;

    if (crossProductZ > 0) {
        rotation += M_PI_2;
    }
}

// Function to place the corner post and panels for an inside corner
void CornerAssetPlacer::placeAssetsAtCorners() {
    std::vector<AcGePoint3d> corners = detectPolylines();

    if (corners.empty()) {
        acutPrintf(_T("\nNo corners detected."));
        return;
    }

    double distance = calculateDistanceBetweenPolylines();
    if (distance < 0) {
        acutPrintf(_T("\nFailed to calculate distance between polylines."));
        return;
    }

    PanelDimensions panelDims;
    PanelConfig config = getPanelConfig(distance, panelDims);

    if (!config.panelIdA || !config.panelIdB) {
        acutPrintf(_T("\nFailed to load panels for distance %f"), distance);
        return;
    }

    std::vector<CornerConfig> cornerConfigs = generateCornerConfigs(corners, config);
    g_cornerConfigs = cornerConfigs;  // Store the corner configurations for later use

    // Debug output to verify corner configurations
    //for (size_t i = 0; i < cornerConfigs.size(); ++i) {
    //    acutPrintf(_T("\nCorner %d: Position: %f, %f, Is Inside: %d, Adjustment: %f"),
    //        i, cornerConfigs[i].position.x, cornerConfigs[i].position.y, cornerConfigs[i].isInside, cornerConfigs[i].outsideCornerAdjustment);
    //}
    

    // Load the corner post asset
    AcDbObjectId cornerPostId = loadAsset(L"128286X");
    if (cornerPostId == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load corner post asset."));
        return;
    }

    // Convert Panel pointers to AcDbObjectId using loadAsset
    AcDbObjectId panelIdA = loadAsset(config.panelIdA->blockName.c_str());
    AcDbObjectId panelIdB = loadAsset(config.panelIdB->blockName.c_str());

    if (panelIdA == AcDbObjectId::kNull || panelIdB == AcDbObjectId::kNull) {
        acutPrintf(_T("\nFailed to load panel assets."));
        return;
    }

    // Load outside panels and compensators
    AcDbObjectId outsidePanelIds[6];
    for (int i = 0; i < 6; ++i) {
        if (config.outsidePanelIds[i] && config.outsidePanelIds[i]->width > 0) {  // Skip dummy panels
            outsidePanelIds[i] = loadAsset(config.outsidePanelIds[i]->blockName.c_str());
            if (outsidePanelIds[i] == AcDbObjectId::kNull) {
                acutPrintf(_T("\nFailed to load outside panel asset %d."), i);
            }
        }
        else {
            outsidePanelIds[i] = AcDbObjectId::kNull;  // Assign null for dummy panels
        }
    }

    AcDbObjectId compensatorIdA = loadAsset(config.compensatorIdA ? config.compensatorIdA->blockName.c_str() : L"");
    AcDbObjectId compensatorIdB = loadAsset(config.compensatorIdB ? config.compensatorIdB->blockName.c_str() : L"");

    int firstLoopEnd = identifyFirstLoopEnd(corners);
    std::pair<std::vector<AcGePoint3d>, std::vector<AcGePoint3d>> loops = splitLoops(corners, firstLoopEnd);
    std::vector<AcGePoint3d>& firstLoop = loops.first;
    std::vector<AcGePoint3d>& secondLoop = loops.second;

    std::vector<bool> loopIsClockwise = {
        directionOfDrawing2(firstLoop),
        directionOfDrawing2(secondLoop)
    };

    int loopIndex = 0;
    int loopIndexLastPanel = 0;
    int closeLoopCounter = -1;
    int outerLoopIndexValue = 0;

    bool isClockwise = isPolylineClockwise(corners);

    for (size_t cornerNum = 0; cornerNum < corners.size(); ++cornerNum) {
        double rotation = 0.0;
        AcGePoint3d start = corners[cornerNum];
        AcGePoint3d end = corners[(cornerNum + 1) % corners.size()];
        AcGeVector3d direction = (end - start).normal();

        closeLoopCounter++;
        bool isInside = false;

        if (!isItInteger(direction.x) || !isItInteger(direction.y)) {
            if (cornerNum < corners.size() - 1) {
                start = corners[cornerNum];
                end = corners[cornerNum - closeLoopCounter];
                closeLoopCounter = -1;
                loopIndexLastPanel = 1;
            }
        }

        direction = (end - start).normal();
        rotation = atan2(direction.y, direction.x);
        //acutPrintf(_T("\nCorner %d: %f, %f"), cornerNum, corners[cornerNum].x, corners[cornerNum].y);
        rotation = normalizeAngle(rotation);
        //acutPrintf(_T("\nRotation: %f"), rotation);
        rotation = snapToExactAngle(rotation, TOLERANCE);
        //acutPrintf(_T("\nRotation: %f"), rotation);

        if (!(loopIndex == outerLoopIndexValue)) {
            isInside = true;
        }
        if (!loopIsClockwise[loopIndex]) {
            isInside = !isInside;
        }

        adjustRotationForCorner(rotation, corners, cornerNum);
        // **Special Case**: Determine if the corner is convex or concave
        AcGeVector3d prevDirection = corners[cornerNum] - corners[cornerNum > 0 ? cornerNum - 1 : corners.size() - 1];
        AcGeVector3d nextDirection = corners[(cornerNum + 1) % corners.size()] - corners[cornerNum];
        double crossProductZ = prevDirection.x * nextDirection.y - prevDirection.y * nextDirection.x;

        if (crossProductZ < 0) {
            // Convex corner
            //acutPrintf(_T("\nConvex corner detected at %f, %f"), corners[cornerNum].x, corners[cornerNum].y);
            // Add logic specific to convex corners here if needed
            if (!isInside) {
                placeOutsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, config, outsidePanelIds[0], outsidePanelIds[1], outsidePanelIds[2], outsidePanelIds[3], outsidePanelIds[4], outsidePanelIds[5], compensatorIdA, compensatorIdB, distance);
            }
            else {
                placeInsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, distance, compensatorIdA, compensatorIdB);
            }
        }
        else {
            // Concave corner
            //acutPrintf(_T("\nConcave corner detected at %f, %f"), corners[cornerNum].x, corners[cornerNum].y);
            // Add logic specific to concave corners here if needed
            if (!isInside) {
                placeInsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, panelIdA, panelIdB, distance, compensatorIdA, compensatorIdB);
            }
            else {
                placeOutsideCornerPostAndPanels(corners[cornerNum], rotation, cornerPostId, config, outsidePanelIds[0], outsidePanelIds[1], outsidePanelIds[2], outsidePanelIds[3], outsidePanelIds[4], outsidePanelIds[5], compensatorIdA, compensatorIdB, distance);
                
            }
        }


        loopIndex = loopIndexLastPanel;
    }
}

// PLACE ASSETS AT INSIDE CORNERS
void CornerAssetPlacer::placeInsideCornerPostAndPanels(
    const AcGePoint3d& corner,
    double rotation,
    AcDbObjectId cornerPostId,
    AcDbObjectId panelIdA,
    AcDbObjectId panelIdB,
    double distance,
    AcDbObjectId compensatorIdA,
    AcDbObjectId compensatorIdB) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return;
    }

    int wallHeight = globalVarHeight;
    int currentHeight = 0;
    int panelHeights[] = { 1350, 600 };

    for (int panelNum = 0; panelNum < 2; panelNum++) {
        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);

        for (int x = 0; x < numPanelsHeight; x++) {
            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d cornerWithHeight = corner;
            cornerWithHeight.z += currentHeight;
            pCornerPostRef->setPosition(cornerWithHeight);
            pCornerPostRef->setBlockTableRecord(cornerPostId);
            rotation = normalizeAngle(rotation);
            rotation = snapToExactAngle(rotation, TOLERANCE);
            pCornerPostRef->setRotation(rotation);
            pCornerPostRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
                //acutPrintf(_T("\nCorner post placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place corner post."));
            }
            pCornerPostRef->close();

            AcGeVector3d panelAOffset, panelBOffset, compensatorOffsetA, compensatorOffsetB;
            rotation = normalizeAngle(rotation);
            rotation = snapToExactAngle(rotation, TOLERANCE);

            if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                panelAOffset = AcGeVector3d(100.0, 0.0, 0.0);
                panelBOffset = AcGeVector3d(0.0, -250.0, 0.0);
                compensatorOffsetA = AcGeVector3d(250.0, 0.0, 0.0);
                compensatorOffsetB = AcGeVector3d(0.0, -300.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(0.0, 100.0, 0.0);
                panelBOffset = AcGeVector3d(250.0, 0.0, 0.0);
                compensatorOffsetA = AcGeVector3d(0.0, 250.0, 0.0);
                compensatorOffsetB = AcGeVector3d(300.0, 0.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
                panelAOffset = AcGeVector3d(-100.0, 0.0, 0.0);
                panelBOffset = AcGeVector3d(0.0, 250.0, 0.0);
                compensatorOffsetA = AcGeVector3d(-250.0, 0.0, 0.0);
                compensatorOffsetB = AcGeVector3d(0.0, 300.0, 0.0);
            }
            else if (areAnglesEqual(rotation, M_3PI_2, TOLERANCE)) {
                panelAOffset = AcGeVector3d(0.0, -100.0, 0.0);
                panelBOffset = AcGeVector3d(-250.0, 0.0, 0.0);
                compensatorOffsetA = AcGeVector3d(0.0, -250.0, 0.0);
                compensatorOffsetB = AcGeVector3d(-300.0, 0.0, 0.0);
            }
            else {
                acutPrintf(_T("\nInvalid rotation angle detected: %f at %f"), rotation, corner);
                continue;
            }

            AcGePoint3d panelPositionA = cornerWithHeight + panelAOffset;
            AcGePoint3d panelPositionB = cornerWithHeight + panelBOffset;

            AcDbBlockReference* pPanelARef = new AcDbBlockReference();
            pPanelARef->setPosition(panelPositionA);
            pPanelARef->setBlockTableRecord(panelIdA);
            pPanelARef->setRotation(rotation);
            pPanelARef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanelARef) == Acad::eOk) {
                //acutPrintf(_T("\nPanel A placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel A."));
            }
            pPanelARef->close();

            AcDbBlockReference* pPanelBRef = new AcDbBlockReference();
            pPanelBRef->setPosition(panelPositionB);
            pPanelBRef->setBlockTableRecord(panelIdB);
            pPanelBRef->setRotation(rotation + M_PI_2);
            pPanelBRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pPanelBRef) == Acad::eOk) {
                //acutPrintf(_T("\nPanel B placed successfully."));
            }
            else {
                acutPrintf(_T("\nFailed to place Panel B."));
            }
            pPanelBRef->close();

            // Place compensators only if distance is 150
            if (distance == 150) {
                AcGePoint3d compensatorPositionA = cornerWithHeight + compensatorOffsetA;
                AcGePoint3d compensatorPositionB = cornerWithHeight + compensatorOffsetB;

                AcDbBlockReference* pCompensatorARef = new AcDbBlockReference();
                pCompensatorARef->setPosition(compensatorPositionA);
                pCompensatorARef->setBlockTableRecord(compensatorIdA);
                pCompensatorARef->setRotation(rotation);
                pCompensatorARef->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorARef) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator A placed successfully."));
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator A."));
                }
                pCompensatorARef->close();

                AcDbBlockReference* pCompensatorBRef = new AcDbBlockReference();
                pCompensatorBRef->setPosition(compensatorPositionB);
                pCompensatorBRef->setBlockTableRecord(compensatorIdB);
                pCompensatorBRef->setRotation(rotation + M_PI_2);
                pCompensatorBRef->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorBRef) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator B placed successfully."));
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator B."));
                }
                pCompensatorBRef->close();
            }

            currentHeight += panelHeights[panelNum];
        }
    }

    pModelSpace->close();
    pBlockTable->close();
}

// PLACE ASSETS AT OUTSIDE CORNERS
void CornerAssetPlacer::placeOutsideCornerPostAndPanels(
    const AcGePoint3d& corner,
    double rotation,
    AcDbObjectId cornerPostId,
    const PanelConfig& config,
    AcDbObjectId outsidePanelIdA,
    AcDbObjectId outsidePanelIdB,
    AcDbObjectId outsidePanelIdC,
    AcDbObjectId outsidePanelIdD,
    AcDbObjectId outsidePanelIdE,
    AcDbObjectId outsidePanelIdF,
    AcDbObjectId outsideCompensatorIdA,
    AcDbObjectId outsideCompensatorIdB,
    double distance)
{
    //acutPrintf(_T("\nStarting placeOutsideCornerPostAndPanels function."));

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) {
        acutPrintf(_T("\nNo working database found."));
        return;
    }

    AcDbBlockTable* pBlockTable;
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get block table."));
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite) != Acad::eOk) {
        acutPrintf(_T("\nFailed to get model space."));
        pBlockTable->close();
        return;
    }

    int wallHeight = globalVarHeight;
    int currentHeight = 0;
    int panelHeights[] = { 1350, 600 };

    rotation = normalizeAngle(rotation);
    rotation = snapToExactAngle(rotation, TOLERANCE);

    for (int panelNum = 0; panelNum < 2; panelNum++) {
        int numPanelsHeight = static_cast<int>((wallHeight - currentHeight) / panelHeights[panelNum]);
        //acutPrintf(_T("\nPlacing panels of height %d. Number of panels: %d"), panelHeights[panelNum], numPanelsHeight);

        for (int x = 0; x < numPanelsHeight; x++) {
            //acutPrintf(_T("\nPlacing corner post at height %d"), currentHeight);

            AcDbBlockReference* pCornerPostRef = new AcDbBlockReference();
            AcGePoint3d cornerWithHeight = corner;
            cornerWithHeight.z += currentHeight;

            // Offsets for corner post based on rotation
            double offset = 100.0;  // This is the corner width
            int rotationDegrees = static_cast<int>(rotation * 180 / M_PI);
            switch (rotationDegrees) {
            case 90:
                cornerWithHeight.x -= offset;
                cornerWithHeight.y -= offset;
                break;
            case 180:
                cornerWithHeight.x += offset;
                cornerWithHeight.y -= offset;
                break;
            case -90:
            case 270:
                cornerWithHeight.x += offset;
                cornerWithHeight.y += offset;
                break;
            case 0:
                cornerWithHeight.x -= offset;
                cornerWithHeight.y += offset;
                break;
            default:
                acutPrintf(_T("\nInvalid rotation angle detected: %f at (%f, %f, %f)"), rotation, corner.x, corner.y, corner.z);
                continue;
            }

            pCornerPostRef->setPosition(cornerWithHeight);
            pCornerPostRef->setBlockTableRecord(cornerPostId);
            pCornerPostRef->setRotation(rotation);
            pCornerPostRef->setScaleFactors(AcGeScale3d(globalVarScale));

            if (pModelSpace->appendAcDbEntity(pCornerPostRef) == Acad::eOk) {
                //acutPrintf(_T("\nCorner post placed successfully at (%f, %f, %f)"), cornerWithHeight.x, cornerWithHeight.y, cornerWithHeight.z);
            }
            else {
                acutPrintf(_T("\nFailed to place corner post at (%f, %f, %f)"), cornerWithHeight.x, cornerWithHeight.y, cornerWithHeight.z);
            }
            pCornerPostRef->close();

            // Correctly declare and initialize outsidePanelIds array
            AcDbObjectId outsidePanelIds[] = { outsidePanelIdA, outsidePanelIdB, outsidePanelIdC, outsidePanelIdD, outsidePanelIdE, outsidePanelIdF };

            // Check if there are any valid panels to place
            bool hasValidPanels = false;
            for (int i = 0; i < 6; ++i) {
                if (!outsidePanelIds[i].isNull()) {
                    hasValidPanels = true;
                    break;
                }
            }

            if (!hasValidPanels) {
                acutPrintf(_T("\nNo valid outside panel IDs found. Skipping placement for this corner."));
                continue;
            }

            AcGeVector3d panelOffsets[6], compensatorOffsetA, compensatorOffsetB;
            double panelWidths[] = {
                config.outsidePanelIds[0] ? config.outsidePanelIds[0]->width : 0,
                config.outsidePanelIds[1] ? config.outsidePanelIds[1]->width : 0,
                config.outsidePanelIds[2] ? config.outsidePanelIds[2]->width : 0,
                config.outsidePanelIds[3] ? config.outsidePanelIds[3]->width : 0,
                config.outsidePanelIds[4] ? config.outsidePanelIds[4]->width : 0,
                config.outsidePanelIds[5] ? config.outsidePanelIds[5]->width : 0
            };
            double compensatorIdA = config.compensatorIdA ? config.compensatorIdA->width : 0;
            double compensatorIdB = config.compensatorIdB ? config.compensatorIdB->width : 0;
            double cornerWidth = 100.0;  // Width of the corner post

            //acutPrintf(_T("\nCalculated panel widths: %f, %f, %f, %f, %f, %f"),
                //panelWidths[0], panelWidths[1], panelWidths[2], panelWidths[3], panelWidths[4], panelWidths[5]);

            // Define panel offsets based on rotation and number of panels
            if (areAnglesEqual(rotation, 0, TOLERANCE)) {
                panelOffsets[0] = AcGeVector3d(cornerWidth + panelWidths[0], -(cornerWidth), 0.0);
                panelOffsets[1] = AcGeVector3d(cornerWidth, -(cornerWidth), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x + panelWidths[2]), panelOffsets[0].y, 0.0);
                panelOffsets[3] = AcGeVector3d(panelOffsets[1].x, ((panelOffsets[1].y) - panelWidths[1]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d(panelOffsets[2].x + panelWidths[4], panelOffsets[2].y, 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x, ((panelOffsets[3].y) - panelWidths[3]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x + compensatorIdA, panelOffsets[4].y, 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x, ((panelOffsets[5].y - panelWidths[5])), 0.0);
            }

            else if (areAnglesEqual(rotation, M_PI_2, TOLERANCE)) {
                // Adjust for 90-degree rotation
                panelOffsets[0] = AcGeVector3d(cornerWidth, cornerWidth + panelWidths[0], 0.0);
                panelOffsets[1] = AcGeVector3d(cornerWidth, cornerWidth, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x), (panelWidths[2] + panelOffsets[0].y), 0.0);
                panelOffsets[3] = AcGeVector3d((panelOffsets[1].x + panelWidths[1]), panelOffsets[1].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d((panelOffsets[2].x), (panelWidths[4] + panelOffsets[2].y), 0.0);
                panelOffsets[5] = AcGeVector3d((panelOffsets[3].x + panelWidths[3]), panelOffsets[3].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x, (compensatorIdA + panelOffsets[4].y), 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x + panelWidths[5], panelOffsets[5].y, 0.0);
            }

            else if (areAnglesEqual(rotation, M_PI, TOLERANCE)) {
                // Adjust for 180-degree rotation
                panelOffsets[0] = AcGeVector3d(-(cornerWidth + panelWidths[0]), cornerWidth, 0.0);
                panelOffsets[1] = AcGeVector3d(-cornerWidth, cornerWidth, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d((panelOffsets[0].x - panelWidths[2]), panelOffsets[0].y, 0.0);
                panelOffsets[3] = AcGeVector3d(panelOffsets[1].x, (panelOffsets[1].y + panelWidths[1]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d((panelOffsets[2].x - panelWidths[4]), panelOffsets[2].y, 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x, (panelOffsets[3].y + panelWidths[3]), 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d((panelOffsets[4].x - compensatorIdA), panelOffsets[4].y, 0.0);
                compensatorOffsetB = AcGeVector3d(panelOffsets[5].x, (panelOffsets[5].y + panelWidths[5]), 0.0);
            }

            else if (areAnglesEqual(rotation, M_3PI_2, TOLERANCE)) {
                // Adjust for 270-degree rotation
                panelOffsets[0] = AcGeVector3d(-cornerWidth, -(cornerWidth + panelWidths[0]), 0.0);
                panelOffsets[1] = AcGeVector3d(-cornerWidth, -cornerWidth, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[2] = AcGeVector3d(panelOffsets[0].x, (panelOffsets[0].y - panelWidths[2]), 0.0);
                panelOffsets[3] = AcGeVector3d((panelOffsets[1].x - panelWidths[1]), panelOffsets[1].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                panelOffsets[4] = AcGeVector3d(panelOffsets[2].x, (panelOffsets[2].y - panelWidths[4]), 0.0);
                panelOffsets[5] = AcGeVector3d(panelOffsets[3].x - panelWidths[3], panelOffsets[3].y, 0.0);
                /////////////////////////////////////////////////////////////////////////////////////
                compensatorOffsetA = AcGeVector3d(panelOffsets[4].x, (panelOffsets[4].y - compensatorIdA), 0.0);
                compensatorOffsetB = AcGeVector3d((panelOffsets[5].x - panelWidths[5]), panelOffsets[5].y, 0.0);
            }
            else {
                acutPrintf(_T("\nInvalid rotation angle detected: %f at (%f, %f, %f)"), rotation, corner.x, corner.y, corner.z);
                continue;
            }

            //acutPrintf(_T("\nCalculated panel offsets:"));
            for (int i = 0; i < 6; ++i) {
                //acutPrintf(_T("\nPanel %d offset: (%f, %f, %f)"), i, panelOffsets[i].x, panelOffsets[i].y, panelOffsets[i].z);
            }

            // Place panels using the calculated offsets
            //AcDbObjectId outsidePanelIds[] = { outsidePanelIdA, outsidePanelIdB, outsidePanelIdC, outsidePanelIdD, outsidePanelIdE, outsidePanelIdF };
            for (int i = 0; i < 6; i++) {
                if (!outsidePanelIds[i].isNull()) {
                    AcGePoint3d panelPosition = cornerWithHeight + panelOffsets[i];
                    AcDbBlockReference* pPanelRef = new AcDbBlockReference();
                    pPanelRef->setPosition(panelPosition);
                    pPanelRef->setBlockTableRecord(outsidePanelIds[i]);

                    // Add rotation to outside panels
                    double panelRotation = (i % 2 == 0) ? rotation : rotation + M_PI_2;
                    pPanelRef->setRotation(panelRotation + M_PI);
                    pPanelRef->setScaleFactors(AcGeScale3d(globalVarScale));

                    if (pModelSpace->appendAcDbEntity(pPanelRef) == Acad::eOk) {
                        //acutPrintf(_T("\nPanel %d placed successfully at (%f, %f, %f)"), i, panelPosition.x, panelPosition.y, panelPosition.z);
                    }
                    else {
                        acutPrintf(_T("\nFailed to place Panel %d at (%f, %f, %f)"), i, panelPosition.x, panelPosition.y, panelPosition.z);
                    }
                    pPanelRef->close();
                }
                else {
                    //acutPrintf(_T("\nOutside Panel ID %d is a dummy or null panel, skipping."), i);
                }
            }

            if (distance != 150) {
                // Place compensators at the defined positions
                AcGePoint3d compensatorPositionA = cornerWithHeight + compensatorOffsetA;
                AcGePoint3d compensatorPositionB = cornerWithHeight + compensatorOffsetB;

                AcDbBlockReference* pCompensatorRefA = new AcDbBlockReference();
                pCompensatorRefA->setPosition(compensatorPositionA);
                pCompensatorRefA->setBlockTableRecord(outsideCompensatorIdA);
                pCompensatorRefA->setRotation(rotation + M_PI);
                pCompensatorRefA->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorRefA) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator A placed successfully at (%f, %f, %f)"), compensatorPositionA.x, compensatorPositionA.y, compensatorPositionA.z);
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator A at (%f, %f, %f)"), compensatorPositionA.x, compensatorPositionA.y, compensatorPositionA.z);
                }
                pCompensatorRefA->close();

                AcDbBlockReference* pCompensatorRefB = new AcDbBlockReference();
                pCompensatorRefB->setPosition(compensatorPositionB);
                pCompensatorRefB->setBlockTableRecord(outsideCompensatorIdB);
                pCompensatorRefB->setRotation(rotation + M_PI_2 + M_PI);
                pCompensatorRefB->setScaleFactors(AcGeScale3d(globalVarScale));

                if (pModelSpace->appendAcDbEntity(pCompensatorRefB) == Acad::eOk) {
                    //acutPrintf(_T("\nCompensator B placed successfully at (%f, %f, %f)"), compensatorPositionB.x, compensatorPositionB.y, compensatorPositionB.z);
                }
                else {
                    acutPrintf(_T("\nFailed to place Compensator B at (%f, %f, %f)"), compensatorPositionB.x, compensatorPositionB.y, compensatorPositionB.z);
                }
                pCompensatorRefB->close();

                //acutPrintf(_T("\nFinished placing outside corner post and compensators."));
            }
            else {
                acutPrintf(_T("\nDistance is 150, skipping compensator placement."));
            }


            currentHeight += panelHeights[panelNum];
            //acutPrintf(_T("\nCompleted placement for height %d. Moving to the next height."), currentHeight);
        }

        //acutPrintf(_T("\nFinished placing outside corner post, panels, and compensators."));

        pModelSpace->close();
        pBlockTable->close();
    }
}
