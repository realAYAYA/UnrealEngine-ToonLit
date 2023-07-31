// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>
#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteParametricCurve.h>
#include <memory>

namespace gte
{

template <typename Real>
class FrenetFrame2
{
public:
    // Construction.  The curve must persist as long as the FrenetFrame2
    // object does.
    FrenetFrame2(std::shared_ptr<ParametricCurve<2, Real>> const& curve);

    // The normal is perpendicular to the tangent, rotated clockwise by
    // pi/2 radians.
    void operator()(Real t, Vector2<Real>& position, Vector2<Real>& tangent,
        Vector2<Real>& normal) const;

    Real GetCurvature(Real t) const;

private:
    std::shared_ptr<ParametricCurve<2, Real>> mCurve;
};


template <typename Real>
class FrenetFrame3
{
public:
    // Construction.  The curve must persist as long as the FrenetFrame3
    // object does.
    FrenetFrame3(std::shared_ptr<ParametricCurve<3, Real>> const& curve);

    // The binormal is Cross(tangent, normal).
    void operator()(Real t, Vector3<Real>& position, Vector3<Real>& tangent,
        Vector3<Real>& normal, Vector3<Real>& binormal) const;

    Real GetCurvature(Real t) const;
    Real GetTorsion(Real t) const;

private:
    std::shared_ptr<ParametricCurve<3, Real>> mCurve;
};


template <typename Real>
FrenetFrame2<Real>::FrenetFrame2(std::shared_ptr<ParametricCurve<2, Real>> const& curve)
    :
    mCurve(curve)
{
}

template <typename Real>
void FrenetFrame2<Real>::operator()(Real t, Vector2<Real>& position,
    Vector2<Real>& tangent, Vector2<Real>& normal) const
{
    Vector2<Real> values[4];
    mCurve->Evaluate(t, 1, values);
    position = values[0];
    tangent = values[1];
    Normalize(tangent);
    normal = Perp(tangent);
}

template <typename Real>
Real FrenetFrame2<Real>::GetCurvature(Real t) const
{
    Vector2<Real> values[4];
    mCurve->Evaluate(t, 2, values);
    Real speedSqr = Dot(values[1], values[1]);
    if (speedSqr > (Real)0)
    {
        Real numer = DotPerp(values[1], values[2]);
        Real denom = std::pow(speedSqr, (Real)1.5);
        return numer / denom;
    }
    else
    {
        // Curvature is indeterminate, just return 0.
        return (Real)0;
    }
}



template <typename Real>
FrenetFrame3<Real>::FrenetFrame3(std::shared_ptr<ParametricCurve<3, Real>> const& curve)
    :
    mCurve(curve)
{
}

template <typename Real>
void FrenetFrame3<Real>::operator()(Real t, Vector3<Real>& position,
    Vector3<Real>& tangent, Vector3<Real>& normal, Vector3<Real>& binormal) const
{
    Vector3<Real> values[4];
    mCurve->Evaluate(t, 2, values);
    position = values[0];
    Real VDotV = Dot(values[1], values[1]);
    Real VDotA = Dot(values[1], values[2]);
    normal = VDotV * values[2] - VDotA * values[1];
    Normalize(normal);
    tangent = values[1];
    Normalize(tangent);
    binormal = Cross(tangent, normal);
}

template <typename Real>
Real FrenetFrame3<Real>::GetCurvature(Real t) const
{
    Vector3<Real> values[4];
    mCurve->Evaluate(t, 2, values);
    Real speedSqr = Dot(values[1], values[1]);
    if (speedSqr > (Real)0)
    {
        Real numer = Length(Cross(values[1], values[2]));
        Real denom = std::pow(speedSqr, (Real)1.5);
        return numer / denom;
    }
    else
    {
        // Curvature is indeterminate, just return 0.
        return (Real)0;
    }
}

template <typename Real>
Real FrenetFrame3<Real>::GetTorsion(Real t) const
{
    Vector3<Real> values[4];
    mCurve->Evaluate(t, 3, values);
    Vector3<Real> cross = Cross(values[1], values[2]);
    Real denom = Dot(cross, cross);
    if (denom > (Real)0)
    {
        Real numer = Dot(cross, values[3]);
        return numer / denom;
    }
    else
    {
        // Torsion is indeterminate, just return 0.
        return (Real)0;
    }
}

}
