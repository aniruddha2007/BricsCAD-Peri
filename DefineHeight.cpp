#include "stdafx.h"
#include "DefineHeight.h"
#include "acutads.h"
#include "aced.h"

//Default to 135
int globalVarHeight = 1350;

// Define the height of the structure
void DefineHeight::defineHeight() {
    ads_real height;
    ads_printf(_T("\nEnter the height of the structure (mm): "));
    if (acedGetReal(NULL, &height) == RTNORM) {
        acutPrintf(_T("\nHeight defined as: %lfmm"), height);
        globalVarHeight = height; // Convert 
        // Store or use the height as needed in your application
        // For example, save it to a global variable or use it directly
    }
    else {
        acutPrintf(_T("\nInvalid input. Please enter a valid number."));
    }
}
