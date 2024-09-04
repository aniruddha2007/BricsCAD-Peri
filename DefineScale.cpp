#include "stdafx.h"
#include "DefineScale.h"
#include "acutads.h"
#include "aced.h"
//#include "hdf5.h"

// Default to (1, 1, 1)
AcGeScale3d globalVarScale(1, 1, 1);

// Define the scale factor
void DefineScale::defineScale() {
    ads_real scale;
    ads_printf(_T("\nEnter the scale factor (e.g., 1 for (1,1,1) or 0.1 for (0.1,0.1,0.1)): "));
    if (acedGetReal(NULL, &scale) == RTNORM) {
        acutPrintf(_T("\nScale factor defined as: %lf"), scale);
        globalVarScale.set(scale, scale, scale);
        // Store or use the scale as needed in your application
        // For example, save it to a global variable or use it directly
    }
    else {
        acutPrintf(_T("\nInvalid input. Please enter a valid number."));
    }
}

//void DefineScale::testhdf() {
//    hid_t file_id = H5Fcreate("C:\\Users\\aniru\\OneDrive\\Desktop\\work\\test.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
//    if (file_id < 0) {
//        acutPrintf(_T("\nFailed to create file"));
//    }
//    H5Fclose(file_id);
//    acutPrintf(_T("\nFile created successfully"));
//}