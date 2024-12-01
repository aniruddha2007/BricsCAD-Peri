#include "stdafx.h"
#include "DefineScale.h"
#include "acutads.h"
#include "aced.h"



AcGeScale3d globalVarScale(1, 1, 1);


void DefineScale::defineScale() {
    ads_real scale;
    ads_printf(_T("\nEnter the scale factor (e.g., 1 for (1,1,1) or 0.1 for (0.1,0.1,0.1)): "));
    if (acedGetReal(NULL, &scale) == RTNORM) {
        acutPrintf(_T("\nScale factor defined as: %lf"), scale);
        globalVarScale.set(scale, scale, scale);
        
        
    }
    else {
        acutPrintf(_T("\nInvalid input. Please enter a valid number."));
    }
}








