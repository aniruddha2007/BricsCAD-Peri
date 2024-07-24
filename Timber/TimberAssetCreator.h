#pragma once
#include "gepnt3d.h"
#include "dbents.h"
#include "dbapserv.h"
#include "dbsymtb.h"
#include "AcDb.h"

class TimberAssetCreator {
public:
    static AcDbObjectId createTimberAsset(double length, double height);
};
