




#include "StdAfx.h"
#include "SettingsCommands.h"
#include "DefineHeight.h"
#include "DefineScale.h"
#include "acdocman.h"
#include "aced.h"
#include "adscodes.h"
#include "rxregsvc.h"


void SettingsCommands::openSettings() {
    const ACHAR* options[] = { _T("Height"), _T("Scale"), NULL };

    ads_real result;
    ads_printf(_T("\nSelect an option [1-Height/2-Scale]: "));
    if (acedGetReal(NULL, &result) == RTNORM) {
        if (result == 1.0) {
            DefineHeight::defineHeight();
        }
        else if (result == 2.0) {
            DefineScale::defineScale();
        }
        else {
            acutPrintf(_T("\nInvalid option selected."));
        }
    }
    else {
        acutPrintf(_T("\nInvalid input. Please enter a valid number."));
    }
}


void SettingsCommands::initApp() {
    acedRegCmds->addCommand(_T("SETTINGS_CMD"), _T("SETTINGS"), _T("SETTINGS"), ACRX_CMD_MODAL, openSettings);
}


void SettingsCommands::unloadApp() {
    acedRegCmds->removeGroup(_T("SETTINGS_CMD"));
}
