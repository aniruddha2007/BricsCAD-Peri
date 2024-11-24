//CornerAssetPlacer.h
#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <map>
#include "gept3dar.h"  // For AcGePoint3d
#include "dbsymtb.h"   // For AcDbObjectId
#include "SharedConfigs.h"

struct Panels {
    double width;
    double thickness;
    double height;
    std::wstring blockName;

    Panels(double w, double t, double h, const wchar_t* name)
        : width(w), thickness(t), height(h), blockName(name) {}
};

struct PanelConfig {
    Panels* panelIdA;
    Panels* panelIdB;
    Panels* outsidePanelIds[6]; // Array to hold outside panel pointers (max 6)
    Panels* compensatorIdA;
    Panels* compensatorIdB;
};

struct PanelDimensions {
    std::vector<Panels> panels;

    PanelDimensions() {
        // Initialize with the given panel dimensions and block names
        panels.push_back(Panels(0, 0, 0, L""));  // Dummy panel for 0 width
        panels.push_back(Panels(50, 100, 1350, L"128287X"));
        panels.push_back(Panels(100, 100, 1350, L"128292X"));
        panels.push_back(Panels(150, 100, 1350, L"128285X"));
        panels.push_back(Panels(300, 100, 1350, L"128284X"));
        panels.push_back(Panels(450, 100, 1350, L"128283X"));
        panels.push_back(Panels(600, 100, 1350, L"128282X"));
        panels.push_back(Panels(750, 100, 1350, L"128281X"));
    }

    // Function to get panel by width (if needed)
    Panels* getPanelByWidth(double width) {
        for (auto& panel : panels) {
            if (panel.width == width) {
                return &panel;
            }
        }
        return nullptr;  // Return nullptr if no matching panel is found
    }
};

class CornerAssetPlacer {
public:
    // Public method to initiate asset placement at corners
    static void placeAssetsAtCorners();
    static AcDbObjectId loadAsset(const wchar_t* blockName);
    // Public method to identify walls, ensuring declaration matches definition
    static void identifyWalls();
    static int identifyFirstLoopEnd(const std::vector<AcGePoint3d>& corners);
    // Helper method to split corners into two loops
    static std::pair<std::vector<AcGePoint3d>, std::vector<AcGePoint3d>> splitLoops(const std::vector<AcGePoint3d>& corners, int firstLoopEnd);
    static PanelConfig getPanelConfig(double distance, PanelDimensions& panelDims);
    static std::vector<CornerConfig> generateCornerConfigs(const std::vector<AcGePoint3d>& corners, const PanelConfig& config);
    static bool directionOfDrawing2(std::vector<AcGePoint3d>& points);
    // Helper method to calculate distance between first two polylines
    static double calculateDistanceBetweenPolylines();
private:
    // Method to detect polylines in the drawing
    static std::vector<AcGePoint3d> detectPolylines();
    // Method to load an asset block from the block table
    
    // Method to get panel configuration based on distance
   
    // Method to place an asset at a specific corner with a given rotation
    static void placeAssetAtCorner(const AcGePoint3d& corner, double rotation, AcDbObjectId assetId);
    // Method to place corner post and panels (Inside corner)
    static void placeInsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelIdA, AcDbObjectId panelIdB, double distance, AcDbObjectId compensatorIdA, AcDbObjectId compensatorIdB);
    // Method to place corner post and panels (Outside corner)
    static void placeOutsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, const PanelConfig& config, AcDbObjectId outsidePanelIdA, AcDbObjectId outsidePanelIdB, AcDbObjectId outsidePanelIdC, AcDbObjectId outsidePanelIdD, AcDbObjectId outsidePanelIdE, AcDbObjectId outsidePanelIdF, AcDbObjectId outsideCompensatorIdA, AcDbObjectId outsideCompensatorIdB, double distance);
    // Method to add text annotation at a specific position
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);
    // Helper method to identify the end of the first loop
    
    // Helper method to process corners, determining rotation and inside/outside placement
    static void processCorners(const std::vector<AcGePoint3d>& corners, AcDbObjectId cornerPostId, const PanelConfig& config,
        double distance, const std::vector<bool>& loopIsClockwise, const std::vector<bool>& isInsideLoop);
    // Helper method to adjust rotation for a corner based on direction vectors
    static void adjustRotationForCorner(double& rotation, const std::vector<AcGePoint3d>& corners, size_t cornerNum);

    // Comparator for AcGePoint3d to be used in the map
    struct Point3dComparator {
        bool operator()(const AcGePoint3d& lhs, const AcGePoint3d& rhs) const {
            if (lhs.x != rhs.x)
                return lhs.x < rhs.x;
            if (lhs.y != rhs.y)
                return lhs.y < rhs.y;
            return lhs.z < rhs.z;
        }
    };

    // Static member to hold the wall mapping
    static std::map<AcGePoint3d, std::vector<AcGePoint3d>, Point3dComparator> wallMap;
};
