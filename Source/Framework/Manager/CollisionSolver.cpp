#include "CollisionSolver.h"
#include <algorithm>
#include <float.h>
#include "../../Graphics/Geometry/Model/Model.h"

namespace CollisionSolver {

    

    // 軽量版（あらかじめボックスローカル空間 [原点がボックス中心] に変換された頂点を使用する）
    bool CheckTriangleVsBox(const Math::Vector3& p0, const Math::Vector3& p1, const Math::Vector3& p2, const Math::Vector3& extents, Math::Vector3& outPushLocal)
    {
        using namespace DirectX;
        XMVECTOR P[3] = { XMLoadFloat3(&p0), XMLoadFloat3(&p1), XMLoadFloat3(&p2) };
        XMVECTOR E = XMLoadFloat3(&extents);

        XMVECTOR boxAxes[3] = { g_XMIdentityR0, g_XMIdentityR1, g_XMIdentityR2 };
        XMVECTOR triEdges[3] = {
            XMVectorSubtract(P[1], P[0]),
            XMVectorSubtract(P[2], P[1]),
            XMVectorSubtract(P[0], P[2])
        };

        // 三角形の法線
        XMVECTOR triNormal = XMVector3Normalize(XMVector3Cross(triEdges[0], triEdges[1]));

        // 分離軸の候補（最大13本）
        XMVECTOR axes[13];
        axes[0] = boxAxes[0];
        axes[1] = boxAxes[1];
        axes[2] = boxAxes[2];
        axes[3] = triNormal;
        int axisCount = 4;

        // ボックスの辺と三角形の辺の外積軸 (エッジ同士の衝突)
        // キャラクター操作において、エッジ同士のクロス積軸は不要な斜めの押し出し（引っかかり）を生むためコメントアウト
        /*
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                XMVECTOR axis = XMVector3Cross(boxAxes[i], triEdges[j]);
                XMVECTOR lenSq = XMVector3LengthSq(axis);
                if (XMVectorGetX(lenSq) > 0.0001f) {
                    axes[axisCount++] = XMVector3Normalize(axis);
                }
            }
        }
        */

        float minOverlap = FLT_MAX;
        XMVECTOR bestAxis = XMVectorZero();

        for (int i = 0; i < axisCount; i++) {
            XMVECTOR axis = axes[i];

            // ボックスの投影半径
            XMVECTOR absAxis = XMVectorAbs(axis);
            XMVECTOR boxProjV = XMVector3Dot(absAxis, E);
            float boxProj = XMVectorGetX(boxProjV);

            // 三角形の投影範囲
            float t0 = XMVectorGetX(XMVector3Dot(axis, P[0]));
            float t1 = XMVectorGetX(XMVector3Dot(axis, P[1]));
            float t2 = XMVectorGetX(XMVector3Dot(axis, P[2]));
            float triMin = std::min({ t0, t1, t2 });
            float triMax = std::max({ t0, t1, t2 });

            // 重なり量の計算
            float overlap1 = triMax - (-boxProj); 
            float overlap2 = boxProj - triMin;    

            if (overlap1 <= 0 || overlap2 <= 0) return false;

            float overlap = (overlap1 < overlap2) ? overlap1 : overlap2;

            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = (overlap1 < overlap2) ? axis : XMVectorNegate(axis);
            }
        }

        XMStoreFloat3(&outPushLocal, XMVectorScale(bestAxis, minOverlap));
        return true;
    }

    // 球 (中心 cSphereWorld, 半径 radius) と Box (ワールド行列 world, オフセット bOffset, 範囲 bExtents) の交差判定
    bool CheckSphereVsBox(const Math::Vector3& cSphereWorld, float radius, const Math::Vector3& bOffset, const Math::Vector3& extents, const Math::Matrix& boxWorld, Math::Vector3& outPush)
    {
        Math::Matrix invBoxWorld = boxWorld.Invert();
        Math::Vector3 cSphereLocal = Math::Vector3::Transform(cSphereWorld, invBoxWorld) - bOffset;

        Math::Vector3 bMin = -extents;
        Math::Vector3 bMax = extents;
        Math::Vector3 closestPointLocal = {
            std::clamp(cSphereLocal.x, bMin.x, bMax.x),
            std::clamp(cSphereLocal.y, bMin.y, bMax.y),
            std::clamp(cSphereLocal.z, bMin.z, bMax.z)
        };

        Math::Vector3 diffLocal = cSphereLocal - closestPointLocal;
        float distSq = diffLocal.LengthSquared();

        if (distSq <= (radius * radius)) {
            float dist = sqrt(distSq);
            Math::Vector3 pushVectorLocal = { 0, 0, 0 };

            if (dist > 0.0f) {
                pushVectorLocal = (diffLocal / dist) * (radius - dist);
            }
            else {
                float outX = (cSphereLocal.x > 0) ? (bMax.x - cSphereLocal.x) : (bMin.x - cSphereLocal.x);
                float outY = (cSphereLocal.y > 0) ? (bMax.y - cSphereLocal.y) : (bMin.y - cSphereLocal.y);
                float outZ = (cSphereLocal.z > 0) ? (bMax.z - cSphereLocal.z) : (bMin.z - cSphereLocal.z);

                if (std::abs(outX) < std::abs(outY) && std::abs(outX) < std::abs(outZ)) pushVectorLocal.x = outX;
                else if (std::abs(outY) < std::abs(outZ)) pushVectorLocal.y = outY;
                else pushVectorLocal.z = outZ;
            }
            outPush = Math::Vector3::TransformNormal(pushVectorLocal, boxWorld);
            return true;
        }
        return false;
    }


// 球とBoxの判定処理 (OBB対応) - コンポーネント用
static bool CheckSphereVsBox(CollisionResult &result,
                             const Math::Vector3 &cSphereWorld, float radius,
                             const Math::Vector3 &bOffset, const Math::Vector3 &extents,
                             const Math::Matrix &boxWorld, bool isASphere) {
  if (CheckSphereVsBox(cSphereWorld, radius, bOffset, extents, boxWorld,
                               result.pushVector)) {
    result.isHit = true;
    if (!isASphere)
      result.pushVector = -result.pushVector;
    return true;
  }
  return false;
}

// カプセルの2つの端点(線分)を求める関数
static void GetCapsulePoints(const CollisionShapeCapsule *cap,
                             const Math::Matrix &world, Math::Vector3 &p1,
                             Math::Vector3 &p2) {
  Math::Vector3 localOffsetStr = Math::Vector3(0, cap->height * 0.5f, 0);
  Math::Vector3 localP1 = cap->m_offset + localOffsetStr;
  Math::Vector3 localP2 = cap->m_offset - localOffsetStr;
  p1 = Math::Vector3::Transform(localP1, world);
  p2 = Math::Vector3::Transform(localP2, world);
}

//===================================================
// (図形A, 行列A, 図形B, 行列B を渡して交差しているか判定する)
//===================================================
bool CheckCollisionShape(CollisionResult& result,
                             const CollisionShape* shapeA, const Math::Matrix& worldA,
                             const CollisionShape* shapeB, const Math::Matrix& worldB) {
  if (!shapeA || !shapeB)
    return false;

  auto typeA = shapeA->GetShapeId();
  auto typeB = shapeB->GetShapeId();

  if (typeB == CollisionShape::ShapeId::Mesh) {
    const auto *meshShape = static_cast<const CollisionShapeMesh *>(shapeB);
    if (!meshShape->m_model) return false;

    Math::Matrix boxWorldInv = worldA.Invert();
    Math::Matrix worldB_withOffset = Math::Matrix::CreateTranslation(meshShape->m_offset) * worldB;

    bool hit = false;
    Math::Vector3 totalPush = {0, 0, 0};
    std::vector<Math::Vector3> hitPushes;

    for (const auto &node : meshShape->m_model->GetNodes()) {
      Math::Matrix nodeWorld = worldB_withOffset; // The vertices are already in Model Space!
      DirectX::XMMATRIX mNodeWorld = DirectX::XMLoadFloat4x4(&nodeWorld);

      Math::Matrix nodeToBoxLocal;
      DirectX::XMMATRIX mNodeToBoxLocal;
      if (typeA == CollisionShape::ShapeId::Box) {
        nodeToBoxLocal = nodeWorld * boxWorldInv;
        mNodeToBoxLocal = DirectX::XMLoadFloat4x4(&nodeToBoxLocal);
      }

      for (const auto &mesh : node.meshes) {
        if (!mesh) continue;
        const auto& verts = mesh->GetVertices();
        for (const auto &face : mesh->GetFaces()) {
          Math::Vector3 v0 = verts[face.Idx[0]].Position;
          Math::Vector3 v1 = verts[face.Idx[1]].Position;
          Math::Vector3 v2 = verts[face.Idx[2]].Position;

          if (typeA == CollisionShape::ShapeId::Sphere) {
            const auto *sphere = static_cast<const CollisionShapeSphere *>(shapeA);

            DirectX::XMVECTOR V0 = DirectX::XMVector3Transform(v0, mNodeWorld);
            DirectX::XMVECTOR V1 = DirectX::XMVector3Transform(v1, mNodeWorld);
            DirectX::XMVECTOR V2 = DirectX::XMVector3Transform(v2, mNodeWorld);

            Math::Vector3 cSphere = Math::Vector3::Transform(sphere->m_offset, worldA);
            DirectX::XMVECTOR sCenter = DirectX::XMLoadFloat3(&cSphere);
            DirectX::XMVECTOR sRadV = DirectX::XMVectorReplicate(sphere->radius);
            DirectX::XMVECTOR sMin = DirectX::XMVectorSubtract(sCenter, sRadV);
            DirectX::XMVECTOR sMax = DirectX::XMVectorAdd(sCenter, sRadV);

            DirectX::XMVECTOR triMin = DirectX::XMVectorMin(V0, DirectX::XMVectorMin(V1, V2));
            DirectX::XMVECTOR triMax = DirectX::XMVectorMax(V0, DirectX::XMVectorMax(V1, V2));
            DirectX::XMVECTOR diff = DirectX::XMVectorMax(
                DirectX::XMVectorSubtract(triMin, sMax),
                DirectX::XMVectorSubtract(sMin, triMax));
            if (DirectX::XMVectorGetX(diff) > 0 || DirectX::XMVectorGetY(diff) > 0 || DirectX::XMVectorGetZ(diff) > 0)
              continue;

            Math::Vector3 vp[3];
            DirectX::XMStoreFloat3(&vp[0], V0);
            DirectX::XMStoreFloat3(&vp[1], V1);
            DirectX::XMStoreFloat3(&vp[2], V2);

            Math::Vector3 triP = ClosestPointOnTriangle(cSphere, vp[0], vp[1], vp[2]);
            Math::Vector3 toSphere = cSphere - triP;
            float distSq = toSphere.LengthSquared();
            if (distSq <= sphere->radius * sphere->radius) {
              float dist = sqrt(distSq);
              hit = true;
              if (dist > 0.0001f)
                totalPush += (toSphere / dist) * (sphere->radius - dist);
              else
                totalPush += Math::Vector3(0, 1, 0) * sphere->radius;
            }
          }
          else if (typeA == CollisionShape::ShapeId::Box) {
            const auto *box = static_cast<const CollisionShapeBox *>(shapeA);

            DirectX::XMVECTOR offset = DirectX::XMLoadFloat3(&box->m_offset);
            DirectX::XMVECTOR P0 = DirectX::XMVectorSubtract(DirectX::XMVector3Transform(v0, mNodeToBoxLocal), offset);
            DirectX::XMVECTOR P1 = DirectX::XMVectorSubtract(DirectX::XMVector3Transform(v1, mNodeToBoxLocal), offset);
            DirectX::XMVECTOR P2 = DirectX::XMVectorSubtract(DirectX::XMVector3Transform(v2, mNodeToBoxLocal), offset);

            Math::Vector3 extents = {box->m_width * 0.5f, box->m_height * 0.5f, box->m_depth * 0.5f};
            DirectX::XMVECTOR E = DirectX::XMLoadFloat3(&extents);
            DirectX::XMVECTOR negE = DirectX::XMVectorNegate(E);

            DirectX::XMVECTOR triMin = DirectX::XMVectorMin(P0, DirectX::XMVectorMin(P1, P2));
            DirectX::XMVECTOR triMax = DirectX::XMVectorMax(P0, DirectX::XMVectorMax(P1, P2));
            DirectX::XMVECTOR diff = DirectX::XMVectorMax(
                DirectX::XMVectorSubtract(triMin, E),
                DirectX::XMVectorSubtract(negE, triMax));
            if (DirectX::XMVectorGetX(diff) > 0 || DirectX::XMVectorGetY(diff) > 0 || DirectX::XMVectorGetZ(diff) > 0)
              continue;

            Math::Vector3 p[3];
            DirectX::XMStoreFloat3(&p[0], P0);
            DirectX::XMStoreFloat3(&p[1], P1);
            DirectX::XMStoreFloat3(&p[2], P2);

            Math::Vector3 pushLocal;
            if (CheckTriangleVsBox(p[0], p[1], p[2], extents, pushLocal)) {
              hit = true;
              Math::Vector3 pushWorld = Math::Vector3::TransformNormal(pushLocal, worldA);
              bool isAdded = false;
              for (auto &existingPush : hitPushes) {
                float dot = existingPush.Dot(pushWorld);
                float lengths = existingPush.Length() * pushWorld.Length();
                if (dot > lengths * 0.99f) {
                  if (pushWorld.LengthSquared() > existingPush.LengthSquared()) {
                    existingPush = pushWorld;
                  }
                  isAdded = true;
                  break;
                }
              }
              if (!isAdded) {
                hitPushes.push_back(pushWorld);
              }
            }
          }
          else if (typeA == CollisionShape::ShapeId::Capsule) {
            const auto *cap = static_cast<const CollisionShapeCapsule *>(shapeA);
            Math::Vector3 p1, p2;
            GetCapsulePoints(cap, worldA, p1, p2);

            DirectX::XMVECTOR V0 = DirectX::XMVector3Transform(v0, mNodeWorld);
            DirectX::XMVECTOR V1 = DirectX::XMVector3Transform(v1, mNodeWorld);
            DirectX::XMVECTOR V2 = DirectX::XMVector3Transform(v2, mNodeWorld);

            DirectX::XMVECTOR cP1 = DirectX::XMLoadFloat3(&p1);
            DirectX::XMVECTOR cP2 = DirectX::XMLoadFloat3(&p2);
            DirectX::XMVECTOR capMin = DirectX::XMVectorMin(cP1, cP2);
            DirectX::XMVECTOR capMax = DirectX::XMVectorMax(cP1, cP2);
            DirectX::XMVECTOR capRadV = DirectX::XMVectorReplicate(cap->radius);
            capMin = DirectX::XMVectorSubtract(capMin, capRadV);
            capMax = DirectX::XMVectorAdd(capMax, capRadV);

            DirectX::XMVECTOR triMin = DirectX::XMVectorMin(V0, DirectX::XMVectorMin(V1, V2));
            DirectX::XMVECTOR triMax = DirectX::XMVectorMax(V0, DirectX::XMVectorMax(V1, V2));
            DirectX::XMVECTOR diff = DirectX::XMVectorMax(
                DirectX::XMVectorSubtract(triMin, capMax),
                DirectX::XMVectorSubtract(capMin, triMax));
            if (DirectX::XMVectorGetX(diff) > 0 || DirectX::XMVectorGetY(diff) > 0 || DirectX::XMVectorGetZ(diff) > 0)
              continue;

            Math::Vector3 vp[3];
            DirectX::XMStoreFloat3(&vp[0], V0);
            DirectX::XMStoreFloat3(&vp[1], V1);
            DirectX::XMStoreFloat3(&vp[2], V2);

            Math::Vector3 cSeg, cTri;
            ClosestPointOnSegmentToTriangle(p1, p2, vp[0], vp[1], vp[2], cSeg, cTri);
            Math::Vector3 toSeg = cSeg - cTri;
            float distSq = toSeg.LengthSquared();
            if (distSq <= cap->radius * cap->radius) {
              float dist = sqrt(distSq);
              hit = true;
              if (dist > 0.0001f)
                totalPush += (toSeg / dist) * (cap->radius - dist);
              else
                totalPush += Math::Vector3(0, 1, 0) * cap->radius;
            }
          }
        }
      }
    }

    if (hit) {
      if (typeA == CollisionShape::ShapeId::Box) {
        for (const auto &p : hitPushes) {
          totalPush += p;
        }
      }
      result.isHit = true;
      result.pushVector = totalPush;
      return true;
    }
  }

  return false;
}

void ClosestPointOnSegmentToTriangle(const Math::Vector3& p1, const Math::Vector3& p2, const Math::Vector3& v0, const Math::Vector3& v1, const Math::Vector3& v2, Math::Vector3& outClosestOnSegment, Math::Vector3& outClosestOnTriangle) {
    Math::Vector3 closestOnTri = ClosestPointOnTriangle(p1, v0, v1, v2);
    float minDistSq = (p1 - closestOnTri).LengthSquared();
    outClosestOnSegment = p1;
    outClosestOnTriangle = closestOnTri;

    closestOnTri = ClosestPointOnTriangle(p2, v0, v1, v2);
    float dSq = (p2 - closestOnTri).LengthSquared();
    if (dSq < minDistSq) {
        minDistSq = dSq;
        outClosestOnSegment = p2;
        outClosestOnTriangle = closestOnTri;
    }

    Math::Vector3 triEdges[3][2] = { {v0, v1}, {v1, v2}, {v2, v0} };
    for (int i = 0; i < 3; ++i) {
        Math::Vector3 tc1, tc2;
        ClosestPointsBetweenTwoSegments(p1, p2, triEdges[i][0], triEdges[i][1], tc1, tc2);
        float distSq = (tc1 - tc2).LengthSquared();
        if (distSq < minDistSq) {
            minDistSq = distSq;
            outClosestOnSegment = tc1;
            outClosestOnTriangle = tc2;
        }
    }

    Math::Vector3 dirVec = p2 - p1;
    float len = dirVec.Length();
    if (len > 0) {
        Math::Vector3 normDir = dirVec / len;
        float dist = 0;
        DirectX::XMVECTOR P1 = DirectX::XMLoadFloat3(&p1);
        DirectX::XMVECTOR NormDir = DirectX::XMLoadFloat3(&normDir);
        DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&v0);
        DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&v1);
        DirectX::XMVECTOR V2 = DirectX::XMLoadFloat3(&v2);
        if (DirectX::TriangleTests::Intersects(P1, NormDir, V0, V1, V2, dist)) {
            if (dist >= 0 && dist <= len) {
                Math::Vector3 hitP = p1 + normDir * dist;
                outClosestOnSegment = hitP;
                outClosestOnTriangle = hitP;
            }
        }
    }
}

}
Math::Vector3 CollisionSolver::ClosestPointOnSegmentToPoint(const Math::Vector3& A, const Math::Vector3& B, const Math::Vector3& P) {
    Math::Vector3 AB = B - A;
    float lenSq = AB.LengthSquared();
    if (lenSq == 0.0f) return A;
    float t = std::clamp(AB.Dot(P - A) / lenSq, 0.0f, 1.0f);
    return A + AB * t;
}

void CollisionSolver::ClosestPointsBetweenTwoSegments(const Math::Vector3& p1, const Math::Vector3& p2, const Math::Vector3& p3, const Math::Vector3& p4, Math::Vector3& c1, Math::Vector3& c2) {
    using namespace DirectX;
    XMVECTOR P1 = XMLoadFloat3(&p1);
    XMVECTOR P2 = XMLoadFloat3(&p2);
    XMVECTOR P3 = XMLoadFloat3(&p3);
    XMVECTOR P4 = XMLoadFloat3(&p4);

    XMVECTOR d1 = XMVectorSubtract(P2, P1);
    XMVECTOR d2 = XMVectorSubtract(P4, P3);
    XMVECTOR r = XMVectorSubtract(P1, P3);
    float a = XMVectorGetX(XMVector3Dot(d1, d1));
    float e = XMVectorGetX(XMVector3Dot(d2, d2));
    float f = XMVectorGetX(XMVector3Dot(d2, r));

    float s = 0.0f, t = 0.0f;
    float c = XMVectorGetX(XMVector3Dot(d1, r));
    float b = XMVectorGetX(XMVector3Dot(d1, d2));
    float denom = a * e - b * b;

    if (denom != 0.0f) {
        s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
    } else {
        s = 0.0f;
    }

    t = (b * s + f) / e;
    if (t < 0.0f) {
        t = 0.0f;
        if (a != 0.0f) s = std::clamp(-c / a, 0.0f, 1.0f);
    } else if (t > 1.0f) {
        t = 1.0f;
        if (a != 0.0f) s = std::clamp((b - c) / a, 0.0f, 1.0f);
    }

    XMStoreFloat3(&c1, XMVectorAdd(P1, XMVectorScale(d1, s)));
    XMStoreFloat3(&c2, XMVectorAdd(P3, XMVectorScale(d2, t)));
}

Math::Vector3 CollisionSolver::ClosestPointOnTriangle(const Math::Vector3& p, const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c) {
    using namespace DirectX;
    XMVECTOR P = XMLoadFloat3(&p);
    XMVECTOR A = XMLoadFloat3(&a);
    XMVECTOR B = XMLoadFloat3(&b);
    XMVECTOR C = XMLoadFloat3(&c);

    XMVECTOR ab = XMVectorSubtract(B, A);
    XMVECTOR ac = XMVectorSubtract(C, A);
    XMVECTOR ap = XMVectorSubtract(P, A);
    float d1 = XMVectorGetX(XMVector3Dot(ab, ap));
    float d2 = XMVectorGetX(XMVector3Dot(ac, ap));
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    XMVECTOR bp = XMVectorSubtract(P, B);
    float d3 = XMVectorGetX(XMVector3Dot(ab, bp));
    float d4 = XMVectorGetX(XMVector3Dot(ac, bp));
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        Math::Vector3 res;
        XMStoreFloat3(&res, XMVectorAdd(A, XMVectorScale(ab, v)));
        return res;
    }

    XMVECTOR cp = XMVectorSubtract(P, C);
    float d5 = XMVectorGetX(XMVector3Dot(ab, cp));
    float d6 = XMVectorGetX(XMVector3Dot(ac, cp));
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        Math::Vector3 res;
        XMStoreFloat3(&res, XMVectorAdd(A, XMVectorScale(ac, w)));
        return res;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        Math::Vector3 res;
        XMStoreFloat3(&res, XMVectorAdd(B, XMVectorScale(XMVectorSubtract(C, B), w)));
        return res;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    Math::Vector3 res;
    XMStoreFloat3(&res, XMVectorAdd(XMVectorAdd(A, XMVectorScale(ab, v)), XMVectorScale(ac, w)));
    return res;
}

