#pragma once
#include "CollisionShape.h"

namespace CollisionSolver {

    Math::Vector3 ClosestPointOnSegmentToPoint(const Math::Vector3& A, const Math::Vector3& B, const Math::Vector3& P);

    void ClosestPointsBetweenTwoSegments(const Math::Vector3& p1, const Math::Vector3& p2, const Math::Vector3& p3, const Math::Vector3& p4, Math::Vector3& c1, Math::Vector3& c2);

    Math::Vector3 ClosestPointOnTriangle(const Math::Vector3& p, const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2);

    void ClosestPointOnSegmentToTriangle(const Math::Vector3& p1, const Math::Vector3& p2, const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2, Math::Vector3& outClosestOnSegment, Math::Vector3& outClosestOnTriangle);

    bool CheckTriangleVsBox(const Math::Vector3& p0, const Math::Vector3& p1, const Math::Vector3& p2, const Math::Vector3& bExtents, Math::Vector3& outPushLocal);

    bool CheckSphereVsBox(const Math::Vector3& cSphereWorld, float radius, const Math::Vector3& bOffset, const Math::Vector3& extents, const Math::Matrix& boxWorld, Math::Vector3& outPush);

    bool CheckCollisionShape(CollisionResult& result,
                             const CollisionShape* shapeA, const Math::Matrix& worldA,
                             const CollisionShape* shapeB, const Math::Matrix& worldB);
}