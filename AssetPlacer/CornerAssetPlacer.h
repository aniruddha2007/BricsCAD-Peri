#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <map>
#include "gept3dar.h"  // For AcGePoint3d
#include "dbsymtb.h"   // For AcDbObjectId

struct Panel {
    double width;
    double thickness;
    double height;
    std::wstring blockName;

    Panel(double w, double t, double h, const wchar_t* name)
        : width(w), thickness(t), height(h), blockName(name) {}
};

struct PanelConfig {
    Panel* panelIdA;
    Panel* panelIdB;
    Panel* outsidePanelIds[6]; // Array to hold outside panel pointers (max 6)
    Panel* compensatorIdA;
    Panel* compensatorIdB;
};

struct PanelDimensions {
    std::vector<Panel> panels;

    PanelDimensions() {
        // Initialize with the given panel dimensions and block names
        panels.push_back(Panel(0, 0, 0, L""));  // Dummy panel for 0 width
        panels.push_back(Panel(50, 100, 1350, L"128287X"));
        panels.push_back(Panel(100, 100, 1350, L"128292X"));
        panels.push_back(Panel(150, 100, 1350, L"128285X"));
        panels.push_back(Panel(300, 100, 1350, L"128284X"));
        panels.push_back(Panel(450, 100, 1350, L"128283X"));
        panels.push_back(Panel(600, 100, 1350, L"128282X"));
        panels.push_back(Panel(750, 100, 1350, L"128281X"));
    }

    // Function to get panel by width (if needed)
    Panel* getPanelByWidth(double width) {
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
    // Public method to identify walls, ensuring declaration matches definition
    static void identifyWalls();

private:
    // Method to detect polylines in the drawing
    static std::vector<AcGePoint3d> detectPolylines();
    // Method to load an asset block from the block table
    static AcDbObjectId loadAsset(const wchar_t* blockName);
    // Method to get panel configuration based on distance
    static PanelConfig getPanelConfig(double distance, PanelDimensions& panelDims);
    // Method to place an asset at a specific corner with a given rotation
    static void placeAssetAtCorner(const AcGePoint3d& corner, double rotation, AcDbObjectId assetId);
    // Method to place corner post and panels (Inside corner)
    static void placeInsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, AcDbObjectId panelIdA, AcDbObjectId panelIdB, double distance, AcDbObjectId compensatorIdA, AcDbObjectId compensatorIdB);
    // Method to place corner post and panels (Outside corner)
    static void placeOutsideCornerPostAndPanels(const AcGePoint3d& corner, double rotation, AcDbObjectId cornerPostId, const PanelConfig& config, AcDbObjectId outsidePanelIdA, AcDbObjectId outsidePanelIdB, AcDbObjectId outsidePanelIdC, AcDbObjectId outsidePanelIdD, AcDbObjectId outsidePanelIdE, AcDbObjectId outsidePanelIdF, AcDbObjectId outsideCompensatorIdA, AcDbObjectId outsideCompensatorIdB, double distance);
    // Method to add text annotation at a specific position
    static void addTextAnnotation(const AcGePoint3d& position, const wchar_t* text);

    // Helper method to calculate distance between first two polylines
    static double calculateDistanceBetweenPolylines();
    // Helper method to identify the end of the first loop
    static int identifyFirstLoopEnd(const std::vector<AcGePoint3d>& corners);
    // Helper method to split corners into two loops
    static std::pair<std::vector<AcGePoint3d>, std::vector<AcGePoint3d>> splitLoops(const std::vector<AcGePoint3d>& corners, int firstLoopEnd);
    // Helper method to process corners, determining rotation and inside/outside placement
    static void processCorners(const std::vector<AcGePoint3d>& corners, AcDbObjectId cornerPostId, const PanelConfig& config, double distance, const std::vector<bool>& loopIsClockwise);
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
