





#include "StdAfx.h"
#include <afxdllx.h>
#include "AssetPlacer/CornerAssetPlacer.h"
#include "AssetPlacer/WallAssetPlacer.h"
#include "WallPanelConnectors/WallPanelConnector.h"
#include "Blocks/BlockLoader.h"
#include "DefineHeight.h"
#include "DefineScale.h"
#include "Tie/TiePlacer.h"
#include "SettingsCommands.h"


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
    
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PlaceCorners", L"PlaceCorners", ACRX_CMD_MODAL, &CornerAssetPlacer::placeAssetsAtCorners);
	acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PlaceWalls", L"PlaceWalls", ACRX_CMD_MODAL, &WallPlacer::placeWalls);
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PlaceConnectors", L"PlaceConnectors", ACRX_CMD_MODAL, &WallPanelConnector::placeConnectors);
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PlaceTies", L"PlaceTies", ACRX_CMD_MODAL, &TiePlacer::placeTies);
    
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"LoadBlocks", L"LoadBlocks", ACRX_CMD_MODAL, &BlockLoader::loadBlocksFromJson);
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"DefineHeight", L"DefineHeight", ACRX_CMD_MODAL, &DefineHeight::defineHeight);
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"DefineScale", L"DefineScale", ACRX_CMD_MODAL, &DefineScale::defineScale);;
    acedRegCmds->addCommand(L"MY_PLUGIN_GROUP", L"PeriSettings", L"PeriSettings", ACRX_CMD_MODAL, &SettingsCommands::openSettings);
}

void unloadApp() {
    
    acedRegCmds->removeGroup(L"MY_PLUGIN_GROUP");
}
