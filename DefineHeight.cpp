#include "stdafx.h"
#include "DefineHeight.h"
#include "acutads.h"
#include "aced.h"


int globalVarHeight = 1350;


void DefineHeight::defineHeight() {
    ads_real height;
    ads_printf(_T("\nEnter the height of the structure (mm): "));
    if (acedGetReal(NULL, &height) == RTNORM) {
        acutPrintf(_T("\nHeight defined as: %lfmm"), height);
        globalVarHeight = static_cast<int>(height); 
        
        
    }
    else {
        acutPrintf(_T("\nInvalid input. Please enter a valid number."));
    }
}
