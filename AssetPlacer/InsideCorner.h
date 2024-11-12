#pragma once

class InsideCorner {
public:
    static std::vector<AcGePoint3d> InsideCorner::getPolylineCorners();
    static void InsideCorner::placeAssetsAtCorners();
    static void InsideCorner::placeInsideCornerPostAndPanels(
        const AcGePoint3d& corner,
        double rotation,
        AcDbObjectId cornerPostId,
        AcDbObjectId panelIdA,
        AcDbObjectId panelIdB,
        double distance,
        AcDbObjectId compensatorIdA,
        AcDbObjectId compensatorIdB);
private:
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