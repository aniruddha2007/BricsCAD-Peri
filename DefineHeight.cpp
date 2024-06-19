#include "stdafx.h"
#include "DefineHeight.h"
#include "acutads.h"
#include "aced.h"

void DefineHeight::defineHeight() {
    ads_real height;
    ads_printf(_T("\nEnter the height of the structure: "));
    if (acedGetReal(NULL, &height) == RTNORM) {
        acutPrintf(_T("\nHeight defined as: %lf"), height);
        // Store or use the height as needed in your application
        // For example, save it to a global variable or use it directly
    }
    else {
        acutPrintf(_T("\nInvalid input. Please enter a valid number."));
    }
}
