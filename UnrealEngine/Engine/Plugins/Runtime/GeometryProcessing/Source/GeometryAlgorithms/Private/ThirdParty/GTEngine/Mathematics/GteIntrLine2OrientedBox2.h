// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine2AlignedBox2.h>

// The queries consider the box to be a solid.
//
// The test-intersection queries use the method of separating axes.  The
// find-intersection queries use parametric clipping against the four edges of
// the box.

namespace gte
{

template <typename Real>
class TIQuery<Real, Line2<Real>, OrientedBox2<Real>>
    :
    public TIQuery<Real, Line2<Real>, AlignedBox2<Real>>
{
public:
    struct Result
        :
        public TIQuery<Real, Line2<Real>, AlignedBox2<Real>>::Result
    {
        // No additional relevant information to compute.
    };

    Result operator()(Line2<Real> const& line, OrientedBox2<Real> const& box);
};

template <typename Real>
class FIQuery<Real, Line2<Real>, OrientedBox2<Real>>
    :
    public FIQuery<Real, Line2<Real>, AlignedBox2<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line2<Real>, AlignedBox2<Real>>::Result
    {
        // No additional relevant information to compute.
    };

    Result operator()(Line2<Real> const& line, OrientedBox2<Real> const& box);
};


template <typename Real>
typename TIQuery<Real, Line2<Real>, OrientedBox2<Real>>::Result
TIQuery<Real, Line2<Real>, OrientedBox2<Real>>::operator()(
    Line2<Real> const& line, OrientedBox2<Real> const& box)
{
    // Transform the line to the oriented-box coordinate system.
    Vector2<Real> diff = line.origin - box.center;
    Vector2<Real> lineOrigin
    {
        Dot(diff, box.axis[0]),
        Dot(diff, box.axis[1])
    };
    Vector2<Real> lineDirection
    {
        Dot(line.direction, box.axis[0]),
        Dot(line.direction, box.axis[1])
    };

    Result result;
    this->DoQuery(lineOrigin, lineDirection, box.extent, result);
    return result;
}

template <typename Real>
typename FIQuery<Real, Line2<Real>, OrientedBox2<Real>>::Result
FIQuery<Real, Line2<Real>, OrientedBox2<Real>>::operator()(
    Line2<Real> const& line, OrientedBox2<Real> const& box)
{
    // Transform the line to the oriented-box coordinate system.
    Vector2<Real> diff = line.origin - box.center;
    Vector2<Real> lineOrigin
    {
        Dot(diff, box.axis[0]),
        Dot(diff, box.axis[1])
    };
    Vector2<Real> lineDirection
    {
        Dot(line.direction, box.axis[0]),
        Dot(line.direction, box.axis[1])
    };

    Result result;
    this->DoQuery(lineOrigin, lineDirection, box.extent, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = line.origin + result.parameter[i] * line.direction;
    }
    return result;
}


}
