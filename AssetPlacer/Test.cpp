#include "stdafx.h"
#include "Test.h"
#include <AcDb/AcDbDynBlockReference.h>
#include <AcDb/AcDbDynBlockReferenceProperty.h>
#include <acdb.h>
#include <aced.h>
#include "AcDb/AcDbEvalVariant.h" // For AcDbEvalVariant
//include for acutPrintf
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <limits>
#include <chrono>
#include <thread>
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "AcDb.h"
#include <AcDb/AcDbBlockTable.h>
#include <AcDb/AcDbBlockTableRecord.h>
#include <AcDb/AcDbAttribute.h>
#include <AcDb/AcDbPolyline.h>
#include <dbapserv.h>
#include <acutads.h>
#include <AcDb/AcDbDynBlockReference.h>
#include "aced.h"
#include "gepnt3d.h"
#include <fstream> // For file handling
#include <iostream> // For console output
#include <AcDb/AcDbSmartObjectPointer.h>
#include "SharedDefinations.h"
//
//
//
//// Function to open a block reference in the database for block name "117466X" and return its AcDbObjectId
//
//AcDbObjectId loadAssetProp(const wchar_t* blockName) {
//    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
//
//    if (!pDb) {
//        acutPrintf(_T("\nNo working database found."));
//        return AcDbObjectId::kNull;
//    }
//
//    AcDbBlockTable* pBlockTable;
//    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get block table."));
//        return AcDbObjectId::kNull;
//    }
//
//    AcDbObjectId assetId;
//    if (pBlockTable->getAt(blockName, assetId) != Acad::eOk) {
//        acutPrintf(_T("\nFailed to get block table record for block '%s'."), blockName);
//        pBlockTable->close();
//        return AcDbObjectId::kNull;
//    }
//
//    pBlockTable->close();
//    return assetId;
//}
//
//
//void listDynamicBlockProperties(AcDbObjectId blockRefId) {
//    // Ensure the ID is valid
//    if (blockRefId.isNull()) {
//        acutPrintf(_T("Invalid block reference ID.\n"));
//        return;
//    }
//
//    // Create an instance of AcDbDynBlockReference
//    AcDbDynBlockReference dynBlockRef(blockRefId);
//	//check if the block reference is a dynamic block
//	if (!dynBlockRef.isDynamicBlock()) {
//		acutPrintf(_T("Block reference is not a dynamic block.\n"));
//		return;
//	}
//
//	//get dynBlockRef type name
//
//    AcDbDynBlockReferencePropertyArray propArray;
//    dynBlockRef.getBlockProperties(propArray);
//
//	//check if the property array is empty
//	if (propArray.isEmpty()) {
//		acutPrintf(_T("No properties found for the block reference.\n"));
//		return;
//	}
//
//	//debugging print
//	acutPrintf(_T("\nProperty Array Length: %f"), propArray.length());
//	// Print properties
//	for (int i = 0; i < propArray.length(); i++) {
//		AcDbDynBlockReferenceProperty prop = propArray[i];
//		acutPrintf(_T("\nProperty: %s"), prop.propertyName().constPtr());
//		acutPrintf(_T("\nDescription: %s"), prop.description().constPtr());
//		acutPrintf(_T("\nValue: %s"), prop.value());
//		acutPrintf(_T("\nUnits Type: %d"), prop.unitsType());
//		acutPrintf(_T("\n"));
//	}
//
//}
//
////void checkBlockType(AcDbObjectId blockRefId) {
////    if (blockRefId.isNull()) {
////        acutPrintf(_T("Invalid block reference ID.\n"));
////        return;
////    }
////
////    // Open the block reference
////    AcDbSmartObjectPointer<AcDbBlockReference> pBlockRef(blockRefId, AcDb::kForRead);
////    if (pBlockRef.openStatus() != Acad::eOk) {
////        acutPrintf(_T("Failed to open block reference.\n"));
////        return;
////    }
////
////    // Check if it is a dynamic block
////    if (AcDbDynBlockReference::isDynamicBlock(blockRefId)) {
////        acutPrintf(_T("The block is a dynamic block.\n"));
////    }
////    else {
////        acutPrintf(_T("The block is not a dynamic block.\n"));
////    }
////
////    // Get the block table record ID
////    AcDbObjectId blockTableRecordId = pBlockRef->blockTableRecord();
////
////    // Open the block table record
////    AcDbSmartObjectPointer<AcDbBlockTableRecord> pBlockTableRecord(blockTableRecordId, AcDb::kForRead);
////    if (pBlockTableRecord.openStatus() != Acad::eOk) {
////        acutPrintf(_T("Failed to open block table record.\n"));
////        return;
////    }
////
////    // Check if the block is anonymous
////    if (pBlockTableRecord->isAnonymous()) {
////        acutPrintf(_T("The block is anonymous.\n"));
////    }
////    else {
////        acutPrintf(_T("The block is not anonymous.\n"));
////    }
////
////    // Check if the block is an external reference (xref)
////    if (pBlockTableRecord->isFromExternalReference()) {
////        acutPrintf(_T("The block is an external reference (xref).\n"));
////    }
////    else {
////        acutPrintf(_T("The block is not an external reference.\n"));
////    }
////
////    // Check if the block is a layout block
////    if (pBlockTableRecord->isLayout()) {
////        acutPrintf(_T("The block is a layout block.\n"));
////    }
////    else {
////        acutPrintf(_T("The block is not a layout block.\n"));
////    }
////}
//
//void test() {
//    /*AcDbObjectId blockId = loadAssetProp(ASSET_117466);*/
//
//	////print block id
// //   if (! blockId.isNull()) {
// //       acutPrintf(_T("\nBlock '117466X' found. Printing properties:\n"));
//	//	checkBlockType(blockId);
// //       /*listDynamicBlockProperties(blockId);*/
// //   }
// //   else {
// //       acutPrintf(_T("\nBlock '117466X' not found."));
//
//    //}
//}
