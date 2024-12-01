


















#include "StdAfx.h"
#include "BlockLoader.h"
#include <acadstrc.h>
#include <adscodes.h>
#include <acutads.h>
#include <dbapserv.h>
#include <dbents.h>
#include <afxdlgs.h>  
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <atlstr.h>   

using json = nlohmann::json;


void BlockLoader::loadBlocksFromJson() {
    
    CFileDialog fileDlg(TRUE, _T("json"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"));
    if (fileDlg.DoModal() != IDOK) {
        
        return;
    }

    CString filePath = fileDlg.GetPathName();
    

    
    CW2A pszConvertedAnsiString(filePath);
    std::string jsonPath(pszConvertedAnsiString);
    

    
    std::ifstream jsonFile(jsonPath);
    if (!jsonFile.is_open()) {
        
        return;
    }

    json j;
    jsonFile >> j;
    jsonFile.close();

    
    for (const auto& item : j) {
        if (item.contains("file_path")) {
            std::string filePath = item["file_path"];
            

            
            std::string blockName = extractFileNameFromPath(filePath);
            loadBlockIntoBricsCAD(blockName.c_str(), filePath.c_str());
        }
        else {
            acutPrintf(L"Error: file_path not found in JSON.\n");
        }
    }

    acutPrintf(L"Assets successfully loaded.\n");
}


std::string BlockLoader::extractFileNameFromPath(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    std::string fileName = (std::string::npos == pos) ? path : path.substr(pos + 1);
    
    size_t dotPos = fileName.find_last_of(".");
    if (dotPos != std::string::npos) {
        fileName = fileName.substr(0, dotPos);
    }
    return fileName;
}


void BlockLoader::loadBlockIntoBricsCAD(const char* blockName, const char* blockPath) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (pDb == nullptr) {
        acutPrintf(L"Failed to get the working database.\n");
        return;
    }

    AcDbDatabase* pSourceDb = new AcDbDatabase(Adesk::kFalse); 
    Acad::ErrorStatus es = pSourceDb->readDwgFile(charToACHAR(blockPath)); 
    if (es != Acad::eOk) {
        acutPrintf(L"Failed to read DWG file: %s\n", acadErrorStatusText(es));
        delete pSourceDb;
        return;
    }

    AcDbObjectId outBlockId;
    es = pDb->insert(outBlockId, charToACHAR(blockName), pSourceDb, true); 
    if (es != Acad::eOk) {
        acutPrintf(L"Failed to insert block: %s. Error: %s\n", blockName, acadErrorStatusText(es));
    }
    else {
        
    }

    delete pSourceDb; 
}


ACHAR* BlockLoader::charToACHAR(const char* str) {
    size_t newsize = strlen(str) + 1;
    wchar_t* wstr = new wchar_t[newsize];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wstr, newsize, str, _TRUNCATE);
    return wstr;
}
