#pragma once

#include <sqlite3.h>
#include "dbsymtb.h"

class BlockLoader {
public:
    static void loadBlocksFromDatabase();
    static void loadBlockIntoBricsCAD(const char* blockName, const char* blockPath);
private:
    static ACHAR* charToACHAR(const char* str);
};

//#include "C:\Program Files\Bricsys\BRXSDK\BRX24.2.03.0\inc\AcDb\AcDbHostApplicationServices.h"