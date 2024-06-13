// Property of Bricsys NV. All rights reserved. 
// This file is part of the BRX SDK, and its use is subject to
// the terms of the BRX SDK license agreement.
/////////////////////////////////////////////////////////////////////////

// BrxApp.cpp
#include "StdAfx.h"
#include <afxdllx.h>
#include "AssetPlacer/CornerAssetPlacer.h"
#include "AssetPlacer/WallAssetPlacer.h"
#include "WallPanelConnector.h"
#include "Blocks/BlockLoader.h"

AC_IMPLEMENT_EXTENSION_MODULE(MyBrxApp)

void initApp();
void unloadApp();

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    if (DLL_PROCESS_ATTACH == dwReason)
    {
        _hdllInstance = hInstance;
        MyBrxApp.AttachInstance(hInstance);
        InitAcUiDLL();
        initApp();
    }
    else if (DLL_PROCESS_DETACH == dwReason)
    {
        unloadApp();
        MyBrxApp.DetachInstance();
    }
    return TRUE;
}

void initApp() {
    // Register commands
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PlaceCorners", L"PlaceCorners", ACRX_CMD_MODAL, &CornerAssetPlacer::placeAssetsAtCorners);
	acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PlaceWalls", L"PlaceWalls", ACRX_CMD_MODAL, &WallPlacer::placeWalls);
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PlaceConnectors", L"PlaceConnectors", ACRX_CMD_MODAL, &WallPanelConnector::placeConnectors);
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"LoadBlocks", L"LoadBlocks", ACRX_CMD_MODAL, &BlockLoader::loadBlocksFromDatabase);
}

void unloadApp() {
    // Unregister commands
    acedRegCmds->removeGroup(L"MY_PLUGIN_GROUP");
}
