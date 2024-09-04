#pragma once
#include <vector>
#include "AcGe/AcGePoint3d.h"

struct CornerConfig {
    AcGePoint3d position;
    AcGePoint3d startPoint;
    AcGePoint3d endPoint;
    bool isInside;
    double outsideCornerAdjustment;

    // Constructors
    CornerConfig()
        : position(AcGePoint3d::kOrigin),
        startPoint(AcGePoint3d::kOrigin),
        endPoint(AcGePoint3d::kOrigin),
        isInside(true),
        outsideCornerAdjustment(0.0) {}

    CornerConfig(const AcGePoint3d& pos, const AcGePoint3d& start, const AcGePoint3d& end, bool inside, double adjustment)
        : position(pos),
        startPoint(start),
        endPoint(end),
        isInside(inside),
        outsideCornerAdjustment(adjustment) {}
};

// Declaration of the global vector (only declaration, not definition)
extern std::vector<CornerConfig> g_cornerConfigs;
