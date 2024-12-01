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
#include "AssetPlacer/CornerAssetPlacer.h"          
#include "AssetPlacer/WallAssetPlacer.h"            
#include "AssetPlacer/InsideCorner.h"                 
#include "AssetPlacer/OutsideCorner.h"                 
#include "Resource.h"                               
#include "BrxSpecific/ribbon/AcRibbonCombo.h"       
#include "BrxSpecific/ribbon/AcRibbonTab.h"         
#include "BrxSpecific/ribbon/AcRibbonPanel.h"       
#include "BrxSpecific/ribbon/AcRibbonButton.h"      
#include "Blocks/BlockLoader.h"                     
#include "WallPanelConnectors/WallPanelConnector.h" 
#include "WallPanelConnectors/StackedWallPanelConnector.h" 
#include "WallPanelConnectors/Stacked15PanelConnector.h"   
#include "WallPanelConnectors/WalerConnector.h"     
#include "Props/props.h"
#include "Tie/TiePlacer.h" 				            
#include "DefineHeight.h"                           
#include "DefineScale.h"                            
#include "SettingsCommands.h"
#include "Columns/PlaceColumn.h"
#include "Columns/ExtractColumn.h"
#include "Scafold/PlaceBracket-PP.h"
#include <openssl/sha.h>
#include <wininet.h>


#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Wininet.lib")


std::string hashFileContent(const std::string& content) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.c_str()), content.size(), hash);

    
    std::string fileHash;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        char buf[3];
        sprintf(buf, "%02x", hash[i]);
        fileHash += buf;
    }
    return fileHash;
}


bool verifyLicenseFile(const std::string& filePath) {
    const std::string expectedHash = "c26a715d9349ff25fb13ee5100f3c090f382782b24f279f6f43514419d84ddfc";

    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        acutPrintf(_T("\nLicense file not found: %s"), filePath.c_str());
        return false;
    }

    std::string fileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    
    std::string fileHash = hashFileContent(fileContents);

    
    return fileHash == expectedHash;
}

bool isTimezoneTaiwan() {
    TIME_ZONE_INFORMATION tzInfo;
    DWORD result = GetTimeZoneInformation(&tzInfo);

    if (result == TIME_ZONE_ID_INVALID) {
        acutPrintf(_T("Failed to verify a genuine copy."));
        return false;
    }

    
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
		
        
        GetUserNameA(username, &username_len);

        
        std::string usernameW(username, username + strlen(username));
        
        std::string licenseFilePath = "C:\\Users\\" + usernameW + "\\" + LICENSE_FILE_NAME;

        
        if (!verifyLicenseFile(licenseFilePath)) {
            acutPrintf(_T("\nLicense verification failed. Exiting application."));
            return AcRx::kRetError; 
        }

        AcRx::AppRetCode result = AcRxArxApp::On_kInitAppMsg(pAppData);
        acrxRegisterAppMDIAware(pAppData); 
        acrxUnlockApplication(pAppData);   

        
        acutPrintf(_T("\nLoading PERICAD plugin..."));
        acutPrintf(_T("\nVersion: 1.0.0"));
        acutPrintf(_T("\n License file verified successfully for PERI TAIWAN."));
        acutPrintf(_T("\nWelcome to PERICAD plugin developed for BRICSCAD V24 by Ani."));
        acutPrintf(_T("\nType 'ListCmds' to see the available commands."));
        acutPrintf(_T("\nFor more information please contact ani@aniruddhapandit.com\n"));

        
        
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
      
        BlockLoader::loadBlocksFromJson(); 

        return result;
    }

    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pAppData)
    {
        acedRegCmds->removeGroup(_T("BRXAPP")); 
        SettingsCommands::unloadApp(); 
        return AcRxArxApp::On_kUnloadAppMsg(pAppData);
    }

    virtual AcRx::AppRetCode On_kLoadDwgMsg(void* pAppData)
    {
        return AcRx::kRetOK; 
    }

    virtual AcRx::AppRetCode On_kUnloadDwgMsg(void* pAppData)
    {
        return AcRx::kRetOK; 
    }

    virtual AcRx::AppRetCode On_kQuitMsg(void* pAppData)
    {
        return AcRx::kRetOK; 
    }

	
    static void BrxPlaceInsideCorners(void)
    {
        acutPrintf(_T("\nRunning PlaceInsideCorners."));
        InsideCorner::placeAssetsAtCorners();
    }

	
	static void BrxPlaceOutsideCorners(void)
	{
		acutPrintf(_T("\nRunning PlaceOutsideCorners."));
		OutsideCorner::placeAssetsAtCorners();
	}

    
    static void BrxAppPlaceBrackets(void)
	{
		acutPrintf(_T("\nRunning PlaceBrackets."));
        PlaceBracket::placeBrackets();
	}

    
    static void BrxAppPlacePushPullProps(void)
    {
        acutPrintf(_T("\nRunning PlaceProps."));
        PlaceProps::placeProps();
    }

    
    static void BrxAppPlaceCorners(void)
    {
        acutPrintf(_T("\nRunning PlaceCorners."));
        CornerAssetPlacer::placeAssetsAtCorners();
    }

    
    static void BrxAppPlaceWalls(void)
    {
        acutPrintf(_T("\nRunning PlaceWalls."));
        WallPlacer::placeWalls();
    }

     
    static void BrxAppPlaceConnectors(void)
    {
        acutPrintf(_T("\nRunning PlaceConnectors."));
        WallPanelConnector::placeConnectors();
        StackedWallPanelConnectors::placeStackedWallConnectors();
        Stacked15PanelConnector::place15panelConnectors();
        WalerConnector::placeConnectors();
        acutPrintf(_T("\nConnectors placed."));
    }

    
    static void BrxAppPlaceTies(void)
	{
		acutPrintf(_T("\nRunning PlaceTies."));
		TiePlacer::placeTies();
	}

     
    static void BrxAppPlaceColumns(void)
	{
		acutPrintf(_T("\nRunning PlaceColumns."));
        GetUserNameA(username, &username_len);

        
        std::string usernameW(username, username + strlen(username));
        
		std::string jsonFilePath = "C:\\Users\\" + usernameW + "\\" + BLOCKS_FILE_NAME;
		PlaceColumn(jsonFilePath);
	}

    
    static void BrxAppExtractColumn(void)
	{
		acutPrintf(_T("\nRunning ExtractColumn."));
        ExtractColumn();
	}

    
    static void BrxAppLoadBlocks(void)
    {
        acutPrintf(_T("\n Loading Blocks....."));
        BlockLoader::loadBlocksFromJson();
    }

    
    static void BrxAppDefineHeight(void)
    {
        acutPrintf(_T("\nDefining Height..."));
        DefineHeight::defineHeight();
    }

    
    static void BrxAppDefineScale(void)
    {
        acutPrintf(_T("\nRunning DefineScale."));
        
        DefineScale::defineScale();
    }

    
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

    
    void loadCustomMenu()
    {
        wchar_t modulePath[MAX_PATH];
        GetModuleFileName(NULL, modulePath, MAX_PATH);

        
        PathRemoveFileSpec(modulePath);

        
        std::wstring cuiFilePath = std::wstring(modulePath) + L"\\CustomMenu.cui";

        

        if (acedCommandS(RTSTR, L"_.CUILOAD", RTSTR, cuiFilePath.c_str(), RTNONE) != RTNORM) {
            
        }
        else {
            
        }
    }
};

IMPLEMENT_ARX_ENTRYPOINT(CBrxApp)


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