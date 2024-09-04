#include "stdafx.h"
#include "PlaceColumn.h"
#include <acdb.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbapserv.h>
#include <aced.h>
#include "DefineHeight.h"
#include "H5Cpp.h" // Include HDF5 C++ header
#include <string>
#include <vector>

using namespace H5;

void PlaceColumn(const std::string& h5FilePath)
{
    // Ask the user for the column name
    ACHAR blockNameInput[256];
    if (acedGetString(Adesk::kFalse, _T("\nEnter the column name (e.g., 200x200): "), blockNameInput) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

    // Convert ACHAR* to std::string without using wstring conversion
#ifdef UNICODE
    std::wstring wBlockNameInput(blockNameInput);
    std::string blockNameStr(wBlockNameInput.begin(), wBlockNameInput.end());
#else
    std::string blockNameStr(blockNameInput);
#endif

    acutPrintf(_T("\nColumn Name: %s"), blockNameStr.c_str());

    // Use the global variable for height
    int totalHeightRequired = globalVarHeight;
    int currentHeight = 0;

    try {
        // Open the HDF5 file
        H5File file(h5FilePath, H5F_ACC_RDONLY);
        acutPrintf(_T("\nHDF5 file opened successfully."));

        // Navigate to the group corresponding to the specified column name
        std::string columnGroupPath = "/columns/" + blockNameStr;
        acutPrintf(_T("\nAttempting to open group: %s"), columnGroupPath.c_str());

        // Check if the group exists before trying to open it
        if (!file.nameExists(columnGroupPath)) {
            acutPrintf(_T("\nError: Column group '%s' does not exist in the HDF5 file."), columnGroupPath.c_str());
            return;
        }

        Group columnGroup = file.openGroup(columnGroupPath);
        acutPrintf(_T("\nColumn group '%s' opened successfully."), columnGroupPath.c_str());

        // Get the number of datasets (blocks) in this group
        hsize_t numBlocks = columnGroup.getNumObjs();
        acutPrintf(_T("\nNumber of blocks in column: %llu"), numBlocks);

        // Prepare a vector to store the block data
        std::vector<H5::DataSet> selectedBlockData;

        for (hsize_t i = 0; i < numBlocks; i++) {
            std::string blockName = columnGroup.getObjnameByIdx(i);
            acutPrintf(_T("\nLoading block: %s"), blockName.c_str());

            DataSet blockDataset = columnGroup.openDataSet(blockName);

            // Add block data to vector for later use
            selectedBlockData.push_back(blockDataset);
        }

        // Prompt the user to pick a base point on the screen
        AcGePoint3d basePoint;
        ads_point adsBasePoint;
        if (acedGetPoint(nullptr, _T("\nSpecify the base point for column insertion: "), adsBasePoint) != RTNORM) {
            acutPrintf(_T("\nPoint selection was canceled."));
            return;
        }
        basePoint.set(adsBasePoint[X], adsBasePoint[Y], adsBasePoint[Z]);

        // Open the block table
        AcDbBlockTable* pBlockTable;
        acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, AcDb::kForRead);
        acutPrintf(_T("\nBlock table opened successfully."));

        // Open model space for writing
        AcDbBlockTableRecord* pModelSpace;
        pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
        acutPrintf(_T("\nModel space opened successfully."));

        // Loop through available blocks and place them accordingly
        for (const auto& blockData : selectedBlockData) {
            // Read the block data from the dataset
            double blockInfo[7];
            blockData.read(blockInfo, PredType::NATIVE_DOUBLE);

            AcGePoint3d blockPos(blockInfo[0], blockInfo[1], blockInfo[2]);
            double rotation = blockInfo[3];
            AcGeScale3d blockScale(blockInfo[4], blockInfo[5], blockInfo[6]);

            // Calculate the final insertion point relative to the user-specified base point
            AcGePoint3d insertionPoint = basePoint + blockPos.asVector();
            insertionPoint.z += currentHeight; // Adjust Z for stacking

            acutPrintf(_T("\nInserting block at (%.2f, %.2f, %.2f) with rotation %.2f"), insertionPoint.x, insertionPoint.y, insertionPoint.z, rotation);

            // Check if the block name exists in the block table
            AcDbBlockTableRecord* pBlockDef;
#ifdef UNICODE
            std::wstring wBlockName = std::wstring(blockNameStr.begin(), blockNameStr.end());
            if (pBlockTable->getAt(wBlockName.c_str(), pBlockDef, AcDb::kForRead) != Acad::eOk) {
                acutPrintf(_T("\nBlock definition not found: %ls"), wBlockName.c_str());
                continue;
            }
#else
            if (pBlockTable->getAt(ACHAR(blockNameStr.c_str()), pBlockDef, AcDb::kForRead) != Acad::eOk) {
                acutPrintf(_T("\nBlock definition not found: %s"), blockNameStr.c_str());
                continue;
            }
#endif

            // Create a new block reference
            AcDbBlockReference* pBlockRef = new AcDbBlockReference();
            pBlockRef->setBlockTableRecord(pBlockDef->objectId());
            pBlockRef->setPosition(insertionPoint);
            pBlockRef->setRotation(rotation);
            pBlockRef->setScaleFactors(blockScale);

            // Add the block reference to model space
            if (pModelSpace->appendAcDbEntity(pBlockRef) == Acad::eOk) {
                acutPrintf(_T("\nBlock '%s' inserted successfully at (%.2f, %.2f, %.2f)."),
                    blockNameStr.c_str(), insertionPoint.x, insertionPoint.y, insertionPoint.z);
            }
            else {
                acutPrintf(_T("\nFailed to insert block '%s'."), blockNameStr.c_str());
            }

            // Clean up
            pBlockRef->close();
            pBlockDef->close();

            currentHeight += blockInfo[2];  // Update height with the Z position
        }

        // Clean up
        pModelSpace->close();
        pBlockTable->close();
    }
    catch (FileIException& error) {
        error.printErrorStack();
    }
    catch (DataSetIException& error) {
        error.printErrorStack();
    }
    catch (DataSpaceIException& error) {
        error.printErrorStack();
    }
    catch (AttributeIException& error) {
        error.printErrorStack();
    }
}
