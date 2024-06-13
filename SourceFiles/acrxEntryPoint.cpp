// Property of Bricsys NV. All rights reserved.
// This file is part of the BRX SDK, and its use is subject to
// the terms of the BRX SDK license agreement.
/////////////////////////////////////////////////////////////////////////
// acrxEntryPoint.cpp
#include "StdAfx.h"
#include <aced.h>
#include <rxmfcapi.h>
#include <Windows.h>
#include <Shlwapi.h>
#include <fstream>
#include <iostream>
#include <string>
#include "AssetPlacer/CornerAssetPlacer.h" // Include the header for the CornerAssetPlacer class
#include "AssetPlacer/WallAssetPlacer.h"   // Include the header for the WallPlacer class
#include "Resource.h"          // Include the header for the resource file
#include "BrxSpecific/ribbon/AcRibbonCombo.h"  // Include the header for the AcRibbonCombo class
#include "BrxSpecific/ribbon/AcRibbonTab.h"    // Include the header for the AcRibbonTab class
#include "BrxSpecific/ribbon/AcRibbonPanel.h"  // Include the header for the AcRibbonPanel class
#include "BrxSpecific/ribbon/AcRibbonButton.h" // Include the header for the AcRibbonButton class
#include "Blocks/BlockLoader.h"       // Include the header for the BlockLoader class
#include "WallPanelConnector.h" 	 // Include the header for the WallPanelConnector class

#pragma comment(lib, "Shlwapi.lib")

class CBrxApp : public AcRxArxApp
{
public:
    CBrxApp() : AcRxArxApp() {}

    virtual void RegisterServerComponents() {}

    virtual AcRx::AppRetCode On_kInitAppMsg(void* pAppData)
    {
        AcRx::AppRetCode result = AcRxArxApp::On_kInitAppMsg(pAppData);
        acrxRegisterAppMDIAware(pAppData); // is able to work in MDI context
        acrxUnlockApplication(pAppData);   // allows to unload the module during session

        // Place your initialization code and base info here
        acutPrintf(_T("\nWelcome to BricsCAD PERI plugin development by Ani"));
        acutPrintf(_T("\nAvailable commands:"));
        acutPrintf(_T("\n MySandboxCommand : sample test command"));
        acutPrintf(_T("\n PlaceCorners: Identify Corners and Start placing them."));
        acutPrintf(_T("\n PlaceConnectors: Place connectors on wall panels."));
        acutPrintf(_T("\n LoadBlocks: Load current active database BRX."));
        acutPrintf(_T("\n DoAll: Run All Cmds In Order."));

        // Register the commands
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceCorners"), _T("PlaceCorners"), ACRX_CMD_MODAL, &CBrxApp::BrxAppPlaceCorners);
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceWalls"), _T("PlaceWalls"), ACRX_CMD_MODAL, &CBrxApp::BrxAppPlaceWalls);
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceConnectors"), _T("PlaceConnectors"), ACRX_CMD_MODAL, &CBrxApp::BrxAppPlaceConnectors);
        acedRegCmds->addCommand(_T("BRXAPP"), _T("LoadBlocks"), _T("LoadBlocks"), ACRX_CMD_MODAL, &CBrxApp::BrxAppLoadBlocks);
        acedRegCmds->addCommand(_T("BRXAPP"), _T("DoAll"), _T("DoAll"), ACRX_CMD_MODAL, &CBrxApp::BrxAppDoApp);
        acedRegCmds->addCommand(_T("BRXAPP"), _T("MySandboxCommand"), _T("MySandboxCommand"), ACRX_CMD_TRANSPARENT, &CBrxApp::BrxAppMySandboxCommand);

        loadCustomMenu();   // Load the custom menu
        BlockLoader::loadBlocksFromDatabase(); // Load blocks from the database

        return result;
    }

    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pAppData)
    {
        acedRegCmds->removeGroup(_T("BRXAPP")); // Clean up registered commands
        return AcRxArxApp::On_kUnloadAppMsg(pAppData);
    }

    virtual AcRx::AppRetCode On_kLoadDwgMsg(void* pAppData)
    {
        return AcRx::kRetOK; // Return OK if no action is needed
    }

    virtual AcRx::AppRetCode On_kUnloadDwgMsg(void* pAppData)
    {
        return AcRx::kRetOK; // Return OK if no action is needed
    }

    virtual AcRx::AppRetCode On_kQuitMsg(void* pAppData)
    {
        return AcRx::kRetOK; // Return OK if no action is needed
    }

    // SandBox Command for testing the SDK
    static void BrxAppMySandboxCommand(void)
    {
        acutPrintf(_T("\nRunning MySandboxCommand."));
    }

    // PlaceCorners command
    static void BrxAppPlaceCorners(void)
    {
        acutPrintf(_T("\nRunning PlaceCorners."));
        CornerAssetPlacer::placeAssetsAtCorners();
    }

    // PlaceWalls command
    static void BrxAppPlaceWalls(void)
    {
        acutPrintf(_T("\nRunning PlaceWalls."));
        WallPlacer::placeWalls();
    }

    // Settings command need to be defined
    static void BrxAppSettings(void)
    {
        acutPrintf(_T("\nRunning BrxAppSettings."));
        // Add settings dialog here
    }

    static void BrxAppPlaceConnectors(void)
    {
        acutPrintf(_T("\nRunning PlaceConnectors."));
        WallPanelConnector::placeConnectors();
    }

    // LoadBlocks command
    static void BrxAppLoadBlocks(void)
	{
		acutPrintf(_T("\nRunning LoadBlocks."));
		BlockLoader::loadBlocksFromDatabase();
	}

    // DoAll command
    static void BrxAppDoApp(void)
	{
		acutPrintf(_T("\nRunning DoAll."));
		CornerAssetPlacer::placeAssetsAtCorners();
		WallPlacer::placeWalls();
		WallPanelConnector::placeConnectors();
	}

    // Custom menu creation
    void createCustomMenu()
    {
        // Create a ribbon tab
        AcRibbonTab* pRibbonTab = new AcRibbonTab();
        pRibbonTab->setName(L"CustomTools");
        pRibbonTab->setTitle(L"Custom Tools");

        // Create a ribbon panel
        AcRibbonPanel* pRibbonPanel = new AcRibbonPanel();
        AcRibbonPanelSource* pRibbonPanelSource = new AcRibbonPanelSource();
        pRibbonPanelSource->setName(L"ScaffoldingTools");

        // Add a button for PlaceCorners
        AcRibbonButton* pPlaceCornersButton = new AcRibbonButton();
        pPlaceCornersButton->setName(L"PlaceCorners");
        pPlaceCornersButton->setText(L"Place Corners");
        pPlaceCornersButton->setToolTip(L"Place corner assets");
        pPlaceCornersButton->setMacroId(L"PlaceCorners");

        // Add a button for PlaceWalls
        AcRibbonButton* pPlaceWallsButton = new AcRibbonButton();
        pPlaceWallsButton->setName(L"PlaceWalls");
        pPlaceWallsButton->setText(L"Place Walls");
        pPlaceWallsButton->setToolTip(L"Place wall panels");
        pPlaceWallsButton->setMacroId(L"PlaceWalls");

        // Add a combo box
        AcRibbonCombo* pComboBox = new AcRibbonCombo();
        pComboBox->setName(L"SelectOption");
        pComboBox->setToolTip(L"Select an option from the list");

        // Add items to the combo box
        AcRibbonItem* option1 = new AcRibbonItem();
        option1->setName(L"Option 1");
        pComboBox->addItem(option1);

        AcRibbonItem* option2 = new AcRibbonItem();
        option2->setName(L"Option 2");
        pComboBox->addItem(option2);

        AcRibbonItem* option3 = new AcRibbonItem();
        option3->setName(L"Option 3");
        pComboBox->addItem(option3);

        // Add buttons and combo box to the ribbon panel source
        pRibbonPanelSource->addItem(pPlaceCornersButton);
        pRibbonPanelSource->addItem(pPlaceWallsButton);
        pRibbonPanelSource->addItem(pComboBox);

        // Set the panel source to the ribbon panel
        pRibbonPanel->setSource(pRibbonPanelSource);

        // Add the panel to the ribbon tab
        pRibbonTab->addPanel(pRibbonPanel);
    }

    // Load the custom menu from a CUI file
    void loadCustomMenu()
    {
        wchar_t modulePath[MAX_PATH];
        GetModuleFileName(NULL, modulePath, MAX_PATH);

        // Remove the file name from the path to get the directory
        PathRemoveFileSpec(modulePath);

        // Append the CUI file name to the directory path
        std::wstring cuiFilePath = std::wstring(modulePath) + L"\\CustomMenu.cui";

        acutPrintf(L"Loading CUI file from: %s\n", cuiFilePath.c_str());  // Debug output

        if (acedCommandS(RTSTR, L"_.CUILOAD", RTSTR, cuiFilePath.c_str(), RTNONE) != RTNORM) {
            acutPrintf(L"Failed to load CUI file from: %s\n", cuiFilePath.c_str());  // Error message
        }
        else {
            acutPrintf(L"Successfully loaded CUI file from: %s\n", cuiFilePath.c_str());  // Success message
        }
    }
};

IMPLEMENT_ARX_ENTRYPOINT(CBrxApp)

// Define the commands
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, MySandboxCommand, MySandboxCommand, ACRX_CMD_TRANSPARENT, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceCorners, PlaceCorners, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceWalls, PlaceWalls, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, LoadBlocks, LoadBlocks, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, Settings, Settings, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceConnectors, PlaceConnectors, ACRX_CMD_MODAL, NULL)