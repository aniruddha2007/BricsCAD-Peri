#pragma once

#include <string>

//Pi values for calculating the pre defined angles
#ifndef M_PI
#define M_PI 3.141592653589793238462643383279
#endif
#ifndef M_PI_2
#define M_PI_2 1.5707963267948966192313216916395
#endif
#ifndef M_PI_4
#define M_PI_4 0.78539816339744830961566084581975
#endif
#ifndef M_3PI_2
#define M_3PI_2 4.7123889803846898576939650749185
#endif
// Undefine the conflicting macros
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif


// Asset names defined as string constants
//Example usage: 
// #include "SharedDefinations.h"
// AcDbObjectId cornerPostId = loadAsset(ASSET_128286X.c_str());

const std::wstring ASSET_128247 = L"128247X";
const std::wstring ASSET_128280 = L"128280X";
const std::wstring ASSET_128285 = L"128285X";
const std::wstring ASSET_128286 = L"128286X";
const std::wstring ASSET_030005 = L"030005X";
const std::wstring ASSET_030110 = L"030110X";
const std::wstring ASSET_124777 = L"124777X";
const std::wstring ASSET_128265 = L"128265X";
const std::wstring ASSET_128281 = L"128281X";
const std::wstring ASSET_128283 = L"128283X";
const std::wstring ASSET_128284 = L"128284X";
const std::wstring ASSET_128295 = L"128295X";
const std::wstring ASSET_129837 = L"129837X";
const std::wstring ASSET_129838 = L"129838X";
const std::wstring ASSET_129839 = L"129839X";
const std::wstring ASSET_129840 = L"129840X";
const std::wstring ASSET_129841 = L"129841X";
const std::wstring ASSET_129842 = L"129842X";
const std::wstring ASSET_129864 = L"129864X";
const std::wstring ASSET_128282 = L"128282X";
const std::wstring ASSET_030010 = L"030010X";
const std::wstring ASSET_126666 = L"126666X";
const std::wstring ASSET_128287 = L"128287X";
const std::wstring ASSET_128292 = L"128292X";
const std::wstring ASSET_129879 = L"129879X";
const std::wstring ASSET_129884 = L"129884X";
const std::wstring ASSET_030020 = L"030020X";
const std::wstring ASSET_030130 = L"030130X";
const std::wstring ASSET_117325 = L"117325X";
const std::wstring ASSET_117466 = L"117466X";
const std::wstring ASSET_117467 = L"117467X";
const std::wstring ASSET_117468 = L"117468X";
const std::wstring ASSET_128254 = L"128254X";
const std::wstring ASSET_128255 = L"128255X";
const std::wstring ASSET_128257 = L"128257X";
const std::wstring ASSET_128293 = L"128293X";
const std::wstring ASSET_128294 = L"128294X";

//Asset names with their respective codes for all the assets
//DUO Couplers	128247
//Panel DP 135 * 90	128280
//Panel DP 135 * 15	128285
//Corner post DC 135 * 10	128286
//Tie(0.5 to 3.5)	030005
//Wingnut counterplate DW 15, galv 	030110
//Peri anchor bolt	124777
//DUO corner ties 	128265
//Multi panel DMP 135 * 75	128281
//Multi panel DMP 135 * 45	128283
//Panel DP 135 * 30	128284
//DUO corner connectors 	128295
//Panel DP 60 * 90	129837
//Multi panel DMP 60 * 75	129838
//Panel DP 60 * 60	129839
//Multi panel DMP 60 * 45	129840
//Panel DP 60 * 30	129841
//Panel DP 60 * 15	129842
//Corner post DC 60 * 10	129864
//Panel DP 135 * 60	128282
//Tie	030010
//Base Plate - 3	126666
//Wall Thickness compensator DWC 135 * 5	128287
//Wall Thickness compensator DWC 135 * 10	128292
//Wall Thickness compensator DWC 60 * 5	129879
//Wall Thickness compensator DWC 60 * 10	129884
//Tie	030020
//Cam nut DW 15, galv	030130
//PP post	117325
//Push pull prop RS 210, galv	117466
//Push pull prop RS 300, galv	117467
//Push pull prop RS 450, galv	117468
//Duo grip DW 15	128254
//DUO compensation waler 62 	128255
//Duo Scaffold Bracket 70	128257
//Duo tube holder	128293
//Duo brace Connector	128294