#include "stdafx.h"
#include "PlaceColumn.h"
#include <acdb.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbapserv.h>
#include <aced.h>
#include <nlohmann/json.hpp> 
#include <fstream>
#include <string>
#include <sstream>
#include "DefineHeight.h"

using json = nlohmann::json;

void PlaceColumn(const std::string& jsonFilePath)
{
    
    ACHAR blockNameInput[256];
    if (acedGetString(Adesk::kFalse, _T("\nEnter the block name: "), blockNameInput) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

    
#ifdef UNICODE
    std::wstring wBlockNameInput(blockNameInput);
    std::string blockNameStr = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wBlockNameInput);
#else
    std::string blockNameStr(blockNameInput);
#endif

    
    std::ifstream inFile(jsonFilePath);
    if (!inFile.is_open()) {
        acutPrintf(_T("\nFailed to open the JSON file."));
        return;
    }

    json blocksJson;
    inFile >> blocksJson;
    inFile.close();

    if (blocksJson.empty()) {
        acutPrintf(_T("\nNo blocks found in the JSON file."));
        return;
    }

    
    bool blockFound = false;
    json selectedBlockData;
    for (const auto& blockData : blocksJson["columns"]) {
        if (blockData["blockname"] == blockNameStr) {
            selectedBlockData = blockData;
            blockFound = true;
            break;
        }
    }

    if (!blockFound) {
        acutPrintf(_T("\nBlock '%s' not found in the JSON file."), blockNameInput);
        return;
    }

    
    AcGePoint3d basePoint;
    ads_point adsBasePoint;
    if (acedGetPoint(nullptr, _T("\nSpecify the base point for block insertion: "), adsBasePoint) != RTNORM) {
        acutPrintf(_T("\nPoint selection was canceled."));
        return;
    }
    basePoint.set(adsBasePoint[X], adsBasePoint[Y], adsBasePoint[Z]);

    
    AcDbBlockTable* pBlockTable;
    acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, AcDb::kForRead);

    
    AcDbBlockTableRecord* pModelSpace;
    pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);

    
    double cumulativeHeight = 0.0;

    
    for (const auto& blockData : selectedBlockData["blocks"]) {
        
        if (cumulativeHeight >= globalVarHeight) {
            acutPrintf(_T("\nReached the target height of %d mm."), globalVarHeight);
            break;
        }

        
        AcGePoint3d firstBlockPos(
            selectedBlockData["blocks"][0]["position"]["x"].get<double>(),
            selectedBlockData["blocks"][0]["position"]["y"].get<double>(),
            selectedBlockData["blocks"][0]["position"]["z"].get<double>()
        );

        
        for (const auto& blockData : selectedBlockData["blocks"]) {
            std::string blockNameStr = blockData["name"];
            AcGePoint3d blockPos(
                blockData["position"]["x"].get<double>(),
                blockData["position"]["y"].get<double>(),
                blockData["position"]["z"].get<double>()
            );
            double blockRotation = blockData["rotation"];
            AcGeScale3d blockScale(
                blockData["scale"]["x"].get<double>(),
                blockData["scale"]["y"].get<double>(),
                blockData["scale"]["z"].get<double>()
            );

            
#ifdef UNICODE
            std::wstring wBlockName = std::wstring(blockNameStr.begin(), blockNameStr.end());
            const wchar_t* blockName = wBlockName.c_str();
#else
            const char* blockName = blockNameStr.c_str();
#endif

            
            AcDbBlockTableRecord* pBlockDef;
            if (pBlockTable->getAt(blockName, pBlockDef, AcDb::kForRead) != Acad::eOk) {
                acutPrintf(_T("\nBlock definition not found: %s"), blockName);
                continue;
            }

            
            AcGeVector3d offset = blockPos - firstBlockPos;

            
            AcGePoint3d insertionPoint = basePoint + offset;

            
            AcDbBlockReference* pBlockRef = new AcDbBlockReference();
            pBlockRef->setBlockTableRecord(pBlockDef->objectId());
            pBlockRef->setPosition(insertionPoint);
            pBlockRef->setRotation(blockRotation);
            pBlockRef->setScaleFactors(blockScale);

            
            if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
                acutPrintf(_T("\nBlock '%s' inserted successfully at (%.2f, %.2f, %.2f)."),
                    blockName, insertionPoint.x, insertionPoint.y, insertionPoint.z);
            }
            else {
                acutPrintf(_T("\nFailed to insert block '%s'."), blockName);
            }

            
            pBlockRef->close();
            pBlockDef->close();
        }

        
        pModelSpace->close();
        pBlockTable->close();
    }
}