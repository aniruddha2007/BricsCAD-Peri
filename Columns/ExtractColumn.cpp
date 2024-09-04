#include "stdafx.h"
#include "ExtractColumn.h"
#include <acdb.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbapserv.h>
#include <aced.h>
#include <hdf5.h>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

void ExtractColumn()
{
    // Ask the user for the column height
    int columnHeight;
    if (acedGetInt(_T("\nEnter the column height (e.g., 1350 or 600): "), &columnHeight) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

    // Ask the user for the column name
    ACHAR columnName[256];
    if (acedGetString(Adesk::kFalse, _T("\nEnter the column name: "), columnName) != RTNORM) {
        acutPrintf(_T("\nOperation canceled."));
        return;
    }

    // Convert ACHAR* (columnName) to std::string
    std::string columnNameStr;
#ifdef UNICODE
    columnNameStr = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(columnName);
#else
    columnNameStr = columnName;
#endif

    // Start a transaction to access the model space
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    AcDbBlockTable* pBlockTable;
    if (pDb->getSymbolTable(pBlockTable, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to open the block table."));
        return;
    }

    AcDbBlockTableRecord* pModelSpace;
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk) {
        acutPrintf(_T("\nFailed to open the model space."));
        pBlockTable->close();
        return;
    }

    AcDbBlockTableRecordIterator* pIter;
    if (pModelSpace->newIterator(pIter) != Acad::eOk) {
        acutPrintf(_T("\nFailed to create a model space iterator."));
        pModelSpace->close();
        pBlockTable->close();
        return;
    }

    // Prepare HDF5 file
    hid_t file_id, group_id, dataset_id, dataspace_id;
    herr_t status;

    // Open or create the HDF5 file
    file_id = H5Fopen("C:\\Users\\aniru\\OneDrive\\Desktop\\work\\columns.h5", H5F_ACC_RDWR, H5P_DEFAULT);

    if (file_id < 0) {
        file_id = H5Fcreate("C:\\Users\\aniru\\OneDrive\\Desktop\\work\\columns.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        if (file_id < 0) {
            acutPrintf(_T("\nFailed to create HDF5 file."));
            delete pIter;
            pModelSpace->close();
            pBlockTable->close();
            return;
        }
    }

    // Create or open a group for the column
    std::string groupName = "/columns/" + columnNameStr + "_" + std::to_string(columnHeight);
    group_id = H5Gcreate2(file_id, groupName.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (group_id < 0) {
        acutPrintf(_T("\nFailed to create group in HDF5 file."));
        H5Fclose(file_id);
        delete pIter;
        pModelSpace->close();
        pBlockTable->close();
        return;
    }

    // Iterate through model space and extract block data
    std::vector<std::vector<double>> blockDataList;

    for (pIter->start(); !pIter->done(); pIter->step())
    {
        AcDbEntity* pEntity;
        pIter->getEntity(pEntity, AcDb::kForRead);

        if (pEntity->isKindOf(AcDbBlockReference::desc())) {
            AcDbBlockReference* pBlockRef = AcDbBlockReference::cast(pEntity);

            // Get block name using AcString
            AcDbObjectId blockId = pBlockRef->blockTableRecord();
            AcDbBlockTableRecord* pBlockDef;
            if (acdbOpenObject(pBlockDef, blockId, AcDb::kForRead) != Acad::eOk) {
                acutPrintf(_T("\nFailed to open block definition."));
                pEntity->close();
                continue;
            }

            AcString blockName;
            pBlockDef->getName(blockName);

            // Get position, rotation, and scale
            AcGePoint3d position = pBlockRef->position();
            double rotation = pBlockRef->rotation();
            AcGeScale3d scale = pBlockRef->scaleFactors();

            // Store block data in vector for HDF5 writing
            std::vector<double> blockData = {
                position.x, position.y, position.z,
                rotation,
                scale.sx, scale.sy, scale.sz
            };

            blockDataList.push_back(blockData);

            pBlockDef->close();
        }

        pEntity->close();
    }

    // Create dataspace for dataset
    hsize_t dims[2] = { blockDataList.size(), 7 };
    dataspace_id = H5Screate_simple(2, dims, NULL);

    // Write each block as a separate dataset in the group
    for (size_t i = 0; i < blockDataList.size(); ++i) {
        std::string datasetName = "block_" + std::to_string(i);
        dataset_id = H5Dcreate2(group_id, datasetName.c_str(), H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        if (dataset_id >= 0) {
            // Write the block data to the dataset
            status = H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, blockDataList[i].data());
            H5Dclose(dataset_id);
        }
    }

    // Close resources
    H5Sclose(dataspace_id);
    H5Gclose(group_id);
    H5Fclose(file_id);

    delete pIter;
    pModelSpace->close();
    pBlockTable->close();

    acutPrintf(_T("\nColumn data saved successfully to HDF5."));
}
