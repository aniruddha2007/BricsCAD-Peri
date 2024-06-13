#include "StdAfx.h"
#include "BlockLoader.h"
#include <acadstrc.h>
#include <dbsymtb.h>
#include <dbents.h>
#include <adscodes.h>
#include <acutads.h>
#include <string>
#include "C:\Program Files\Bricsys\BRXSDK\BRX24.2.03.0\inc\AcDb\AcDbHostApplicationServices.h"
#include <afxdlgs.h>  // For CFileDialog

void BlockLoader::loadBlocksFromDatabase() {
    // Prompt user to select the database file
    CFileDialog fileDlg(TRUE, _T("db"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("SQLite Database Files (*.db)|*.db|All Files (*.*)|*.*||"));
    if (fileDlg.DoModal() != IDOK) {
        acutPrintf(L"Database selection cancelled.\n");
        return;
    }

    CString filePath = fileDlg.GetPathName();
    CT2CA pszConvertedAnsiString(filePath);
    std::string dbPath(pszConvertedAnsiString);

    // Open the database
    sqlite3* db;
    int rc = sqlite3_open(dbPath.c_str(), &db);

    if (rc) {
        acutPrintf(L"Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // Prepare the SQL statement
    sqlite3_stmt* stmt;
    const char* sql = "SELECT name, path FROM files";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK) {
        acutPrintf(L"Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // Loop through the results and load the blocks
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        const unsigned char* path = sqlite3_column_text(stmt, 1);

        if (name && path) {
            acutPrintf(L"Loading block: %s from path: %s\n", name, path);
            loadBlockIntoBricsCAD(reinterpret_cast<const char*>(name), reinterpret_cast<const char*>(path));
        }
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void BlockLoader::loadBlockIntoBricsCAD(const char* blockName, const char* blockPath) {
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (pDb == nullptr) {
        acutPrintf(L"Failed to get the working database.\n");
        return;
    }

    AcDbBlockTable* pBlockTable;
    pDb->getBlockTable(pBlockTable, AcDb::kForWrite);

    AcDbBlockTableRecord* pBlockTableRecord = new AcDbBlockTableRecord();
    pBlockTableRecord->setName(charToACHAR(blockName));

    AcDbBlockTableRecord* pExistingBlockTableRecord;
    if (pBlockTable->getAt(charToACHAR(blockName), pExistingBlockTableRecord, AcDb::kForWrite) == Acad::eOk) {
        acutPrintf(L"Block %s already exists.\n", blockName);
        pExistingBlockTableRecord->close();
    }
    else {
        if (pBlockTable->add(pBlockTableRecord) == Acad::eOk) {
            acutPrintf(L"Block %s added successfully.\n", blockName);
        }
        else {
            acutPrintf(L"Failed to add block %s.\n", blockName);
        }
        pBlockTableRecord->close();
    }

    pBlockTable->close();
}

ACHAR* BlockLoader::charToACHAR(const char* str) {
    size_t newsize = strlen(str) + 1;
    wchar_t* wstr = new wchar_t[newsize];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wstr, newsize, str, _TRUNCATE);
    return wstr;
}
