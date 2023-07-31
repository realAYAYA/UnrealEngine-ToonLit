// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntegration.h>
#include <ThirdParty/GTEngine/Mathematics/GteRootsBisection.h>
#include <algorithm>
#include <vector>

namespace gte
{

template <int N, typename Real>
class ParametricCurve
{
protected:
    // Abstract base class for a parameterized curve X(t), where t is the
    // parameter in [tmin,tmax] and X is an N-tuple position.  The first
    // constructor is for single-segment curves.  The second constructor is
    // for multiple-segment curves. The times must be strictly increasing.
    ParametricCurve(Real tmin, Real tmax);
    ParametricCurve(int numSegments, Real const* times);
public:
    virtual ~ParametricCurve();

    // To validate construction, create an object as shown:
    //     DerivedClassCurve<N, Real> curve(parameters);
    //     if (!curve) { <constructor failed, handle accordingly>; }
    inline operator bool() const;

    // Member access.
    inline Real GetTMin() const;
    inline Real GetTMax() const;
    inline int GetNumSegments() const;
    Real const* GetTimes() const;

    // This function applies only when the first constructor is used (two
    // times rather than a sequence of three or more times).
    void SetTimeInterval(Real tmin, Real tmax);

    // Parameters used in GetLength(...), GetTotalLength(), and GetTime(...).
    void SetRombergOrder(int order);  // default = 8
    void SetMaxBisections(unsigned int maxBisections);  // default = 1024

    // Evaluation of the curve.  The function supports derivative calculation
    // through order 3; that is, maxOrder <= 3 is required.  If you want
    // only the position, pass in maxOrder of 0.  If you want the position and
    // first derivative, pass in maxOrder of 1, and so on.  The output
    // 'values' are ordered as: position, first derivative, second derivative,
    // third derivative.
    virtual void Evaluate(Real t, unsigned int maxOrder,
        Vector<N, Real> values[4]) const = 0;

    // Differential geometric quantities.
    Vector<N, Real> GetPosition(Real t) const;
    Vector<N, Real> GetTangent(Real t) const;
    Real GetSpeed(Real t) const;
    Real GetLength(Real t0, Real t1) const;
    Real GetTotalLength() const;

    // Inverse mapping of s = Length(t) given by t = Length^{-1}(s).  The
    // inverse length function is generally a function that cannot be written
    // in closed form, in which case it is not directly computable.  Instead,
    // we can specify s and estimate the root t for F(t) = Length(t) - s.  The
    // derivative is F'(t) = Speed(t) >= 0, so F(t) is nondecreasing.  To be
    // robust, we use bisection to locate the root, although it is possible to
    // use a hybrid of Newton's method and bisection.
    Real GetTime(Real length) const;

    // Compute a subset of curve points according to the specified attribute.
    // The input 'numPoints' must be two or larger.
    void SubdivideByTime(int numPoints, Vector<N, Real>* points) const;
    void SubdivideByLength(int numPoints, Vector<N, Real>* points) const;

protected:
    enum
    {
        DEFAULT_ROMBERG_ORDER = 8,
        DEFAULT_MAX_BISECTIONS = 1024
    };

    std::vector<Real> mTime;
    mutable std::vector<Real> mSegmentLength;
    mutable std::vector<Real> mAccumulatedLength;
    int mRombergOrder;
    unsigned int mMaxBisections;

    bool mConstructed;
};


template <int N, typename Real>
ParametricCurve<N, Real>::ParametricCurve(Real tmin, Real tmax)
    :
    mTime(2),
    mSegmentLength(1),
    mAccumulatedLength(1),
    mRombergOrder(DEFAULT_ROMBERG_ORDER),
    mMaxBisections(DEFAULT_MAX_BISECTIONS),
    mConstructed(false)
{
    mTime[0] = tmin;
    mTime[1] = tmax;
    mSegmentLength[0] = (Real)0;
    mAccumulatedLength[0] = (Real)0;
}

template <int N, typename Real>
ParametricCurve<N, Real>::ParametricCurve(int numSegments, Real const* times)
    :
    mTime(numSegments + 1),
    mSegmentLength(numSegments),
    mAccumulatedLength(numSegments),
    mRombergOrder(DEFAULT_ROMBERG_ORDER),
    mMaxBisections(DEFAULT_MAX_BISECTIONS),
    mConstructed(false)
{
    std::copy(times, times + numSegments + 1, mTime.begin());
    mSegmentLength[0] = (Real)0;
    mAccumulatedLength[0] = (Real)0;
}

template <int N, typename Real>
ParametricCurve<N, Real>::~ParametricCurve()
{
}

template <int N, typename Real> inline
ParametricCurve<N, Real>::operator bool() const
{
    return mConstructed;
}

template <int N, typename Real> inline
Real ParametricCurve<N, Real>::GetTMin() const
{
    return mTime.front();
}

template <int N, typename Real> inline
Real ParametricCurve<N, Real>::GetTMax() const
{
    return mTime.back();
}

template <int N, typename Real> inline
int ParametricCurve<N, Real>::GetNumSegments() const
{
    return static_cast<int>(mSegmentLength.size());
}

template <int N, typename Real> inline
Real const* ParametricCurve<N, Real>::GetTimes() const
{
    return &mTime[0];
}

template <int N, typename Real>
void ParametricCurve<N, Real>::SetTimeInterval(Real tmin, Real tmax)
{
    if (mTime.size() == 2)
    {
        mTime[0] = tmin;
        mTime[1] = tmax;
    }
}

template <int N, typename Real>
void ParametricCurve<N, Real>::SetRombergOrder(int order)
{
    mRombergOrder = std::max(order, 1);
}

template <int N, typename Real>
void ParametricCurve<N, Real>::SetMaxBisections(unsigned int maxBisections)
{
    mMaxBisections = std::max(maxBisections, 1u);
}

template <int N, typename Real>
Vector<N, Real> ParametricCurve<N, Real>::GetPosition(Real t) const
{
    Vector<N, Real> values[4];
    Evaluate(t, 0, values);
    return values[0];
}

template <int N, typename Real>
Vector<N, Real> ParametricCurve<N, Real>::GetTangent(Real t) const
{
    Vector<N, Real> values[4];
    Evaluate(t, 1, values);
    Normalize(values[1]);
    return values[1];
}

template <int N, typename Real>
Real ParametricCurve<N, Real>::GetSpeed(Real t) const
{
    Vector<N, Real> values[4];
    Evaluate(t, 1, values);
    return Length(values[1]);
}

template <int N, typename Real>
Real ParametricCurve<N, Real>::GetLength(Real t0, Real t1) const
{
    std::function<Real(Real)> speed = [this](Real t)
    {
        return GetSpeed(t);
    };

    if (mSegmentLength[0] == (Real)0)
    {
        // Lazy initialization of lengths of segments.
        int const numSegments = static_cast<int>(mSegmentLength.size());
        Real accumulated = (Real)0;
        for (int i = 0; i < numSegments; ++i)
        {
            mSegmentLength[i] = Integration<Real>::Romberg(mRombergOrder,
                mTime[i], mTime[i + 1], speed);
            accumulated += mSegmentLength[i];
            mAccumulatedLength[i] = accumulated;
        }
    }

    t0 = std::max(t0, GetTMin());
    t1 = std::min(t1, GetTMax());
    auto iter0 = std::lower_bound(mTime.begin(), mTime.end(), t0);
    int index0 = static_cast<int>(iter0 - mTime.begin());
    auto iter1 = std::lower_bound(mTime.begin(), mTime.end(), t1);
    int index1 = static_cast<int>(iter1 - mTime.begin());

    Real length;
    if (index0 < index1)
    {
        length = (Real)0;
        if (t0 < *iter0)
        {
            length += Integration<Real>::Romberg(mRombergOrder, t0,
                mTime[index0], speed);
        }

        int isup;
        if (t1 < *iter1)
        {
            length += Integration<Real>::Romberg(mRombergOrder,
                mTime[index1 - 1], t1, speed);
            isup = index1 - 1;
        }
        else
        {
            isup = index1;
        }
        for (int i = index0; i < isup; ++i)
        {
            length += mSegmentLength[i];
        }
    }
    else
    {
        length = Integration<Real>::Romberg(mRombergOrder, t0, t1, speed);
    }
    return length;
}

template <int N, typename Real>
Real ParametricCurve<N, Real>::GetTotalLength() const
{
    if (mAccumulatedLength.back() == (Real)0)
    {
        // Lazy evaluation of the accumulated length array.
        return GetLength(mTime.front(), mTime.back());
    }

    return mAccumulatedLength.back();
}

template <int N, typename Real>
Real ParametricCurve<N, Real>::GetTime(Real length) const
{
    if (length > (Real)0)
    {
        if (length < GetTotalLength())
        {
            std::function<Real(Real)> F = [this, &length](Real t)
            {
                return Integration<Real>::Romberg(mRombergOrder,
                    mTime.front(), t, [this](Real z){ return GetSpeed(z); })
                    - length;
            };

            // We know that F(tmin) < 0 and F(tmax) > 0, which allows us to
            // use bisection.  Rather than bisect the entire interval, let's
            // narrow it down with a reasonable initial guess.
            Real ratio = length / GetTotalLength();
            Real omratio = (Real)1 - ratio;
            Real tmid = omratio * mTime.front() + ratio * mTime.back();
            Real fmid = F(tmid);
            if (fmid > (Real)0)
            {
                RootsBisection<Real>::Find(F, mTime.front(), tmid, (Real)-1,
                    (Real)1, mMaxBisections, tmid);
            }
            else if (fmid < (Real)0)
            {
                RootsBisection<Real>::Find(F, tmid, mTime.back(), (Real)-1,
                    (Real)1, mMaxBisections, tmid);
            }
            return tmid;
        }
        else
        {
            return mTime.back();
        }
    }
    else
    {
        return mTime.front();
    }
}

template <int N, typename Real>
void ParametricCurve<N, Real>::SubdivideByTime(int numPoints,
    Vector<N, Real>* points) const
{
    Real delta = (mTime.back() - mTime.front()) / (Real)(numPoints - 1);
    for (int i = 0; i < numPoints; ++i)
    {
        Real t = mTime.front() + delta * i;
        points[i] = GetPosition(t);
    }
}

template <int N, typename Real>
void ParametricCurve<N, Real>::SubdivideByLength(int numPoints,
    Vector<N, Real>* points) const
{
    Real delta = GetTotalLength() / (Real)(numPoints - 1);
    for (int i = 0; i < numPoints; ++i)
    {
        Real length = delta * i;
        Real t = GetTime(length);
        points[i] = GetPosition(t);
    }
}


}
