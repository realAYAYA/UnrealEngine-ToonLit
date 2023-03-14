// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>
#include <array>
#include <limits>

namespace gte
{

// The intervals are [u0,u1] and [v0,v1], where u0 <= u1 and v0 <= v1, and
// where the endpoints are any finite floating-point numbers.  Degenerate
// intervals are allowed (u0 = u1 or v0 = v1).  The queries do not perform
// validation on the input intervals.  In the comments, maxReal refers to
// std::numeric_limits<Real>::max().

template <typename Real>
class TIQuery<Real, std::array<Real,2>, std::array<Real,2>>
{
public:
    // The query tests overlap, whether a single point or an entire interval.
    struct Result
    {
        bool intersect;

        // Dynamic queries (intervals moving with constant speeds).  If
        // 'intersect' is true, the contact times are valid and
        //     0 <= firstTime <= lastTime,  firstTime <= maxTime
        // If 'intersect' is false, there are two cases reported.  If the
        // intervals will intersect at firstTime > maxTime, the contact times
        // are reported just as when 'intersect' is true.  However, if the
        // intervals will not intersect, then firstTime = maxReal and
        // lastTime = -maxReal.
        Real firstTime, lastTime;
    };

    // Static query.
    Result operator()(std::array<Real, 2> const& interval0,
        std::array<Real, 2> const& interval1);

    // Dynamic query.  Current time is 0, maxTime > 0 is required.
    Result operator()(Real maxTime, std::array<Real, 2> const& interval0,
        Real speed0, std::array<Real, 2> const& interval1, Real speed1);
};

template <typename Real>
class FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>
{
public:
    // The query finds overlap, whether a single point or an entire interval.
    struct Result
    {
        bool intersect;

        // Static queries (no motion of intervals over time).  The number of
        // number of intersections is 0 (no overlap), 1 (intervals are just
        // touching), or 2 (intervals overlap in an interval).  If 'intersect'
        // is false, numIntersections is 0 and 'overlap' is set to
        // [maxReal,-maxReal].  If 'intersect' is true, numIntersections is
        // 1 or 2.  When 1, 'overlap' is set to [x,x], which is degenerate and
        // represents the single intersection point x.  When 2, 'overlap' is
        // the interval of intersection.
        int numIntersections;
        std::array<Real, 2> overlap;

        // Dynamic queries (intervals moving with constant speeds).  If
        // 'intersect' is true, the contact times are valid and
        //     0 <= firstTime <= lastTime,  firstTime <= maxTime
        // If 'intersect' is false, there are two cases reported.  If the
        // intervals will intersect at firstTime > maxTime, the contact times
        // are reported just as when 'intersect' is true.  However, if the
        // intervals will not intersect, then firstTime = maxReal and
        // lastTime = -maxReal.
        Real firstTime, lastTime;
    };

    // Static query.
    Result operator()(std::array<Real, 2> const& interval0,
        std::array<Real, 2> const& interval1);

    // Dynamic query.  Current time is 0, maxTime > 0 is required.
    Result operator()(Real maxTime, std::array<Real, 2> const& interval0,
        Real speed0, std::array<Real, 2> const& interval1, Real speed1);
};

// Template aliases for convenience.
template <typename Real>
using TIIntervalInterval =
TIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>;

template <typename Real>
using FIIntervalInterval =
FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>;


template <typename Real>
typename TIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::Result
TIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::operator()(
    std::array<Real, 2> const& interval0,
    std::array<Real, 2> const& interval1)
{
    Result result;
    result.intersect =
        interval0[0] <= interval1[1] && interval0[1] >= interval1[0];
    return result;
}

template <typename Real>
typename TIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::Result
TIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::operator()(
    Real maxTime, std::array<Real, 2> const& interval0, Real speed0,
    std::array<Real, 2> const& interval1, Real speed1)
{
    Result result;

    if (interval0[1] < interval1[0])
    {
        // interval0 initially to the left of interval1.
        Real diffSpeed = speed0 - speed1;
        if (diffSpeed > (Real)0)
        {
            // The intervals must move towards each other.  'intersect' is
            // true when the intervals will intersect by maxTime.
            Real diffPos = interval1[0] - interval0[1];
            Real invDiffSpeed = ((Real)1) / diffSpeed;
            result.intersect = (diffPos <= maxTime*diffSpeed);
            result.firstTime = diffPos*invDiffSpeed;
            result.lastTime = (interval1[1] - interval0[0])*invDiffSpeed;
            return result;
        }
    }
    else if (interval0[0] > interval1[1])
    {
        // interval0 initially to the right of interval1.
        Real diffSpeed = speed1 - speed0;
        if (diffSpeed > (Real)0)
        {
            // The intervals must move towards each other.  'intersect' is
            // true when the intervals will intersect by maxTime.
            Real diffPos = interval0[0] - interval1[1];
            Real invDiffSpeed = ((Real)1) / diffSpeed;
            result.intersect = (diffPos <= maxTime*diffSpeed);
            result.firstTime = diffPos*invDiffSpeed;
            result.lastTime = (interval0[1] - interval1[0])*invDiffSpeed;
            return result;
        }
    }
    else
    {
        // The intervals are initially intersecting.
        result.intersect = true;
        result.firstTime = (Real)0;
        if (speed1 > speed0)
        {
            result.lastTime = (interval0[1] - interval1[0])/(speed1 - speed0);
        }
        else if (speed1 < speed0)
        {
            result.lastTime = (interval1[1] - interval0[0])/(speed0 - speed1);
        }
        else
        {
            result.lastTime = std::numeric_limits<Real>::max();
        }
        return result;
    }

    result.intersect = false;
    result.firstTime = std::numeric_limits<Real>::max();
    result.lastTime = -std::numeric_limits<Real>::max();
    return result;
}



template <typename Real>
typename FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::Result
FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::operator()(
    std::array<Real, 2> const& interval0,
    std::array<Real, 2> const& interval1)
{
    Result result;
    result.firstTime = std::numeric_limits<Real>::max();
    result.lastTime = -std::numeric_limits<Real>::max();

    if (interval0[1] < interval1[0] || interval0[0] > interval1[1])
    {
        result.numIntersections = 0;
        result.overlap[0] = std::numeric_limits<Real>::max();
        result.overlap[1] = -std::numeric_limits<Real>::max();
    }
    else if (interval0[1] > interval1[0])
    {
        if (interval0[0] < interval1[1])
        {
            result.numIntersections = 2;
            result.overlap[0] =
                (interval0[0] < interval1[0] ? interval1[0] : interval0[0]);
            result.overlap[1] =
                (interval0[1] > interval1[1] ? interval1[1] : interval0[1]);
            if (result.overlap[0] == result.overlap[1])
            {
                result.numIntersections = 1;
            }
        }
        else  // interval0[0] == interval1[1]
        {
            result.numIntersections = 1;
            result.overlap[0] = interval0[0];
            result.overlap[1] = result.overlap[0];
        }
    }
    else  // interval0[1] == interval1[0]
    {
        result.numIntersections = 1;
        result.overlap[0] = interval0[1];
        result.overlap[1] = result.overlap[0];
    }

    result.intersect = (result.numIntersections > 0);
    return result;
}

template <typename Real>
typename FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::Result
FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>>::operator()(
    Real maxTime, std::array<Real, 2> const& interval0, Real speed0,
    std::array<Real, 2> const& interval1, Real speed1)
{
    Result result;

    if (interval0[1] < interval1[0])
    {
        // interval0 initially to the left of interval1.
        Real diffSpeed = speed0 - speed1;
        if (diffSpeed > (Real)0)
        {
            // The intervals must move towards each other.  'intersect' is
            // true when the intervals will intersect by maxTime.
            Real diffPos = interval1[0] - interval0[1];
            Real invDiffSpeed = ((Real)1) / diffSpeed;
            result.intersect = (diffPos <= maxTime*diffSpeed);
            result.numIntersections = 1;
            result.firstTime = diffPos*invDiffSpeed;
            result.lastTime = (interval1[1] - interval0[0])*invDiffSpeed;
            result.overlap[0] = interval0[0] + result.firstTime*speed0;
            result.overlap[1] = result.overlap[0];
            return result;
        }
    }
    else if (interval0[0] > interval1[1])
    {
        // interval0 initially to the right of interval1.
        Real diffSpeed = speed1 - speed0;
        if (diffSpeed > (Real)0)
        {
            // The intervals must move towards each other.  'intersect' is
            // true when the intervals will intersect by maxTime.
            Real diffPos = interval0[0] - interval1[1];
            Real invDiffSpeed = ((Real)1) / diffSpeed;
            result.intersect = (diffPos <= maxTime*diffSpeed);
            result.numIntersections = 1;
            result.firstTime = diffPos*invDiffSpeed;
            result.lastTime = (interval0[1] - interval1[0])*invDiffSpeed;
            result.overlap[0] = interval1[1] + result.firstTime*speed1;
            result.overlap[1] = result.overlap[0];
            return result;
        }
    }
    else
    {
        // The intervals are initially intersecting.
        result.intersect = true;
        result.firstTime = (Real)0;
        if (speed1 > speed0)
        {
            result.lastTime = (interval0[1] - interval1[0]) / (speed1 - speed0);
        }
        else if (speed1 < speed0)
        {
            result.lastTime = (interval1[1] - interval0[0]) / (speed0 - speed1);
        }
        else
        {
            result.lastTime = std::numeric_limits<Real>::max();
        }

        if (interval0[1] > interval1[0])
        {
            if (interval0[0] < interval1[1])
            {
                result.numIntersections = 2;
                result.overlap[0] = (interval0[0] < interval1[0] ?
                    interval1[0] : interval0[0]);
                result.overlap[1] = (interval0[1] > interval1[1] ?
                    interval1[1] : interval0[1]);
            }
            else  // interval0[0] == interval1[1]
            {
                result.numIntersections = 1;
                result.overlap[0] = interval0[0];
                result.overlap[1] = result.overlap[0];
            }
        }
        else  // interval0[1] == interval1[0]
        {
            result.numIntersections = 1;
            result.overlap[0] = interval0[1];
            result.overlap[1] = result.overlap[0];
        }
        return result;
    }

    result.intersect = false;
    result.numIntersections = 0;
    result.overlap[0] = std::numeric_limits<Real>::max();
    result.overlap[1] = -std::numeric_limits<Real>::max();
    result.firstTime = std::numeric_limits<Real>::max();
    result.lastTime = -std::numeric_limits<Real>::max();
    return result;
}


}
