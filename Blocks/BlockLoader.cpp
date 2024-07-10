#include "StdAfx.h"
#include "BlockLoader.h"
#include <acadstrc.h>
#include <adscodes.h>
#include <acutads.h>
#include <dbapserv.h>
#include <dbents.h>
#include <afxdlgs.h>  // For CFileDialog
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <atlstr.h>   // For CString to std::string conversion

using json = nlohmann::json;

void BlockLoader::loadBlocksFromJson() {
    // Prompt user to select the JSON file
    CFileDialog fileDlg(TRUE, _T("json"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"));
    if (fileDlg.DoModal() != IDOK) {
        acutPrintf(L"JSON file selection cancelled.\n");
        return;
    }

    CString filePath = fileDlg.GetPathName();
    acutPrintf(L"Selected JSON file: %s\n", (LPCTSTR)filePath);

    // Convert CString to std::string
    CW2A pszConvertedAnsiString(filePath);
    std::string jsonPath(pszConvertedAnsiString);
    acutPrintf(L"Converted JSON path: %s\n", jsonPath.c_str());

    // Read and parse the JSON file
    std::ifstream jsonFile(jsonPath);
    if (!jsonFile.is_open()) {
        acutPrintf(L"Can't open JSON file: %s\n", jsonPath.c_str());
        return;
    }

    json j;
    jsonFile >> j;
    jsonFile.close();

    // Loop through the JSON data and print the file paths
    for (const auto& item : j) {
        if (item.contains("file_path")) {
            std::string filePath = item["file_path"];
            acutPrintf(L"File path: %s\n", filePath.c_str());

            // Load block into BricsCAD
            std::string blockName = extractFileNameFromPath(filePath);
            loadBlockIntoBricsCAD(blockName.c_str(), filePath.c_str());
        }
        else {
            acutPrintf(L"Error: file_path not found in JSON.\n");
        }
    }

    acutPrintf(L"JSON file processed successfully.\n");
}

std::string BlockLoader::extractFileNameFromPath(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    std::string fileName = (std::string::npos == pos) ? path : path.substr(pos + 1);
    // Remove file extension
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

    AcDbDatabase* pSourceDb = new AcDbDatabase(Adesk::kFalse); // Create a new source database
    Acad::ErrorStatus es = pSourceDb->readDwgFile(charToACHAR(blockPath)); // Read the source DWG file
    if (es != Acad::eOk) {
        acutPrintf(L"Failed to read DWG file: %s\n", acadErrorStatusText(es));
        delete pSourceDb;
        return;
    }

    AcDbObjectId outBlockId;
    es = pDb->insert(outBlockId, charToACHAR(blockName), pSourceDb, true); // Insert the block
    if (es != Acad::eOk) {
        acutPrintf(L"Failed to insert block: %s. Error: %s\n", blockName, acadErrorStatusText(es));
    }
    else {
        acutPrintf(L"Block %s inserted successfully.\n", blockName);
    }

    delete pSourceDb; // Clean up
}

ACHAR* BlockLoader::charToACHAR(const char* str) {
    size_t newsize = strlen(str) + 1;
    wchar_t* wstr = new wchar_t[newsize];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wstr, newsize, str, _TRUNCATE);
    return wstr;
}
