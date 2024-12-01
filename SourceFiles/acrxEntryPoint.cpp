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
#include <thread>
#include <chrono>
#include "AssetPlacer/CornerAssetPlacer.h"          // Include the header for the CornerAssetPlacer class
#include "AssetPlacer/WallAssetPlacer.h"            // Include the header for the WallPlacer class
#include "AssetPlacer/InsideCorner.h"                 // Include the header for the InsideCorner class
#include "AssetPlacer/OutsideCorner.h"                 // Include the header for the OutsideCorner class
#include "Resource.h"                               // Include the header for the resource file
#include "BrxSpecific/ribbon/AcRibbonCombo.h"       // Include the header for the AcRibbonCombo class
#include "BrxSpecific/ribbon/AcRibbonTab.h"         // Include the header for the AcRibbonTab class
#include "BrxSpecific/ribbon/AcRibbonPanel.h"       // Include the header for the AcRibbonPanel class
#include "BrxSpecific/ribbon/AcRibbonButton.h"      // Include the header for the AcRibbonButton class
#include "Blocks/BlockLoader.h"                     // Include the header for the BlockLoader class
#include "WallPanelConnectors/WallPanelConnector.h" // Include the header for the WallPanelConnector class
#include "WallPanelConnectors/StackedWallPanelConnector.h" // Include the header for the StackedWallPanelConnector class
#include "WallPanelConnectors/Stacked15PanelConnector.h"   // Include the header for the Stacked15PanelConnector class
#include "WallPanelConnectors/WalerConnector.h"     // Include the header for the WalerConnector class
#include "Props/props.h"
#include "Tie/TiePlacer.h" 				            // Include the header for the TiePlacer class
#include "DefineHeight.h"                           // Include the header for the DefineHeight class
#include "DefineScale.h"                            // Include the header for the DefineScale class
#include "SettingsCommands.h"
#include "Columns/PlaceColumn.h"
#include "Columns/ExtractColumn.h"
#include "Scafold/PlaceBracket-PP.h"
#include <openssl/sha.h>
#include <wininet.h>


#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Wininet.lib")

// Function to compute the hash of the license file content
std::string hashFileContent(const std::string& content) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.c_str()), content.size(), hash);

    // Convert hash to a readable string
    std::string fileHash;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        char buf[3];
        sprintf(buf, "%02x", hash[i]);
        fileHash += buf;
    }
    return fileHash;
}

// Function to verify the license file
bool verifyLicenseFile(const std::string& filePath) {
    const std::string expectedHash = "c26a715d9349ff25fb13ee5100f3c090f382782b24f279f6f43514419d84ddfc";

    // Open and read the license file
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        acutPrintf(_T("\nLicense file not found: %s"), filePath.c_str());
        return false;
    }

    std::string fileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Compute the hash of the file contents
    std::string fileHash = hashFileContent(fileContents);

    // Compare the file hash with the expected hash
    return fileHash == expectedHash;
}

bool isTimezoneTaiwan() {
    TIME_ZONE_INFORMATION tzInfo;
    DWORD result = GetTimeZoneInformation(&tzInfo);

    if (result == TIME_ZONE_ID_INVALID) {
        acutPrintf(_T("Failed to verify a genuine copy."));
        return false;
    }

    // Check if the timezone name is Taiwan's "Taipei Standard Time" (CST)
    std::wstring timezoneName = tzInfo.StandardName;

    if (timezoneName == L"Taipei Standard Time") {
        return true;
    }

    return false;
}

const std::string  BLOCKS_FILE_NAME = "OneDrive - PERI Group\\Documents\\AP-PeriCAD-Automation-Tools\\[03]Plugin\\AP-Columns_12-11-24.json";
const std::string  LICENSE_FILE_NAME = "OneDrive - PERI Group\\Documents\\AP-PeriCAD-Automation-Tools\\license.apdg";

char username[UNLEN + 1];
DWORD username_len = UNLEN + 1;


class CBrxApp : public AcRxArxApp
{
public:
    CBrxApp() : AcRxArxApp() {}

    virtual void RegisterServerComponents() {}

    virtual AcRx::AppRetCode On_kInitAppMsg(void* pAppData)
    {
		//add license file verification here
        // Get the current working directory
        GetUserNameA(username, &username_len);

        //set a variable as username
        std::string usernameW(username, username + strlen(username));
        // Construct the full path for the license file
        std::string licenseFilePath = "C:\\Users\\" + usernameW + "\\" + LICENSE_FILE_NAME;

        // Verify the license file using the constructed path
        if (!verifyLicenseFile(licenseFilePath)) {
            acutPrintf(_T("\nLicense verification failed. Exiting application."));
            return AcRx::kRetError; // Return an error if the verification fails
        }

        AcRx::AppRetCode result = AcRxArxApp::On_kInitAppMsg(pAppData);
        acrxRegisterAppMDIAware(pAppData); // is able to work in MDI context
        acrxUnlockApplication(pAppData);   // allows to unload the module during session

        // Place your initialization code and base info here
        acutPrintf(_T("\nLoading PERICAD plugin..."));
        acutPrintf(_T("\nVersion: 1.0.0"));
        acutPrintf(_T("\n License file verified successfully for PERI TAIWAN."));
        acutPrintf(_T("\nWelcome to PERICAD plugin developed for BRICSCAD V24 by Ani."));
        acutPrintf(_T("\nType 'ListCmds' to see the available commands."));
        acutPrintf(_T("\nFor more information please contact ani@aniruddhapandit.com\n"));

        // Register the commands
        //acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceCorners"), _T("PlaceCorners"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppPlaceCorners(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceWalls"), _T("PlaceWalls"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppPlaceWalls(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceConnectors"), _T("PlaceConnectors"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppPlaceConnectors(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceTies"), _T("PlaceTies"), ACRX_CMD_MODAL, []() { TiePlacer::placeTies(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceColumns"), _T("PlaceColumns"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppPlaceColumns(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("ExtractColumn"), _T("ExtractColumn"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppExtractColumn(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("DefineHeight"), _T("DefineHeight"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppDefineHeight(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("DefineScale"), _T("DefineScale"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppDefineScale(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("LoadBlocks"), _T("LoadBlocks"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppLoadBlocks(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceBrackets"), _T("PlaceBrackets"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppPlaceBrackets(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("ListCMDS"), _T("ListCMDS"), ACRX_CMD_MODAL, []() { CBrxApp::BrxListCMDS(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceProps"), _T("PlaceProps"), ACRX_CMD_MODAL, []() { CBrxApp::BrxAppPlacePushPullProps(); });
        acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceInsideCorners"), _T("PlaceInsideCorners"), ACRX_CMD_MODAL, []() { CBrxApp::BrxPlaceInsideCorners(); });
		acedRegCmds->addCommand(_T("BRXAPP"), _T("PlaceOutsideCorners"), _T("PlaceOutsideCorners"), ACRX_CMD_MODAL, []() { CBrxApp::BrxPlaceOutsideCorners(); });
      
        BlockLoader::loadBlocksFromJson(); // Load blocks from the database

        return result;
    }

    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pAppData)
    {
        acedRegCmds->removeGroup(_T("BRXAPP")); // Clean up registered commands
        SettingsCommands::unloadApp(); // Clean up registered settings commands
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

	//PlaceInsideCorners command
    static void BrxPlaceInsideCorners(void)
    {
        acutPrintf(_T("\nRunning PlaceInsideCorners."));
        InsideCorner::placeAssetsAtCorners();
    }

	//PlaceOutsideCorners command
	static void BrxPlaceOutsideCorners(void)
	{
		acutPrintf(_T("\nRunning PlaceOutsideCorners."));
		OutsideCorner::placeAssetsAtCorners();
	}

    // PlaceBrackets command
    static void BrxAppPlaceBrackets(void)
	{
		acutPrintf(_T("\nRunning PlaceBrackets."));
        PlaceBracket::placeBrackets();
	}

    // PlaceProps command
    static void BrxAppPlacePushPullProps(void)
    {
        acutPrintf(_T("\nRunning PlaceProps."));
        PlaceProps::placeProps();
    }

    // PlaceCorners command only for testing
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

     //PlaceConnectors command
    static void BrxAppPlaceConnectors(void)
    {
        acutPrintf(_T("\nRunning PlaceConnectors."));
        WallPanelConnector::placeConnectors();
        StackedWallPanelConnectors::placeStackedWallConnectors();
        Stacked15PanelConnector::place15panelConnectors();
        WalerConnector::placeConnectors();
        acutPrintf(_T("\nConnectors placed."));
    }

    // PlaceTies command
    static void BrxAppPlaceTies(void)
	{
		acutPrintf(_T("\nRunning PlaceTies."));
		TiePlacer::placeTies();
	}

     //PlaceColumns command
    static void BrxAppPlaceColumns(void)
	{
		acutPrintf(_T("\nRunning PlaceColumns."));
        GetUserNameA(username, &username_len);

        //set a variable as username
        std::string usernameW(username, username + strlen(username));
        // Construct the full path for the JSON file
		std::string jsonFilePath = "C:\\Users\\" + usernameW + "\\" + BLOCKS_FILE_NAME;
		PlaceColumn(jsonFilePath);
	}

    //ExtractColumn command
    static void BrxAppExtractColumn(void)
	{
		acutPrintf(_T("\nRunning ExtractColumn."));
        ExtractColumn();
	}

    // LoadBlocks command
    static void BrxAppLoadBlocks(void)
    {
        acutPrintf(_T("\n Loading Blocks....."));
        BlockLoader::loadBlocksFromJson();
    }

    // DefineHeight command
    static void BrxAppDefineHeight(void)
    {
        acutPrintf(_T("\nDefining Height..."));
        DefineHeight::defineHeight();
    }

    // DefineScale command NOT TO BE USED
    static void BrxAppDefineScale(void)
    {
        acutPrintf(_T("\nRunning DefineScale."));
        //DefineScale::testhdf();
        DefineScale::defineScale();
    }

    // ListCMDS command
    static void BrxListCMDS(void)
    {
        acutPrintf(_T("\nAvailable commands:"));
        acutPrintf(_T("\nPlaceCorners: To only place Corner Assets at the corners., WILL BE REMOVED FOR FINAL VERSION"));
		acutPrintf(_T("\nPlaceInsideCorners: To only place Inside Corner Assets at the corners."));
		acutPrintf(_T("\nPlaceOutsideCorners: To only place Outside Corner Assets at the corners."));
        acutPrintf(_T("\nPlaceWalls: To only place Walls."));
        acutPrintf(_T("\nPlaceConnectors: To Place all types of Connectors. Duo Couplers, Waler, Duo Grip DW 15. "));
        acutPrintf(_T("\nPlaceTies: To only place Ties."));
        acutPrintf(_T("\nPlaceColunms: To only place Columns."));
        acutPrintf(_T("\nPlaceBrackets: To only place Brackets."));
		acutPrintf(_T("\nPlaceProps: To only place Props."));
        acutPrintf(_T("\nDefineHeight: Define Height, specify height in mm."));
        acutPrintf(_T("\nDefineScale: Define Scale factor (e.g., 1 for (1,1,1) or 0.1 for (0.1,0.1,0.1)), NOT IMPLEMENTED CORRECTLY"));
        acutPrintf(_T("\nLoadBlocks: To load custom blocks database."));
        acutPrintf(_T("\nDoAll: only for testing purposes, NOT IMPLEMENTED"));
        acutPrintf(_T("\nListCMDS: Prints this Menu"));
        acutPrintf(_T("\nPeriSettings: Settings"));
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

        //acutPrintf(L"Loading CUI file from: %s\n", cuiFilePath.c_str());  // Debug output

        if (acedCommandS(RTSTR, L"_.CUILOAD", RTSTR, cuiFilePath.c_str(), RTNONE) != RTNORM) {
            //acutPrintf(L"Failed to load CUI file from: %s\n", cuiFilePath.c_str());  // Error message
        }
        else {
            //acutPrintf(L"Successfully loaded CUI file from: %s\n", cuiFilePath.c_str());  // Success message
        }
    }
};

IMPLEMENT_ARX_ENTRYPOINT(CBrxApp)

// Define the commands
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceCorners, PlaceCorners, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceWalls, PlaceWalls, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, LoadBlocks, LoadBlocks, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceConnectors, PlaceConnectors, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceTies, PlaceTies, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceColumns, PlaceColumns, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, ExtractColumn, ExtractColumn, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, DefineHeight, DefineHeight, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, DefineScale, DefineScale, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CBrxApp, BrxApp, PlaceBrackets, PlaceBrackets, ACRX_CMD_MODAL, NULL)