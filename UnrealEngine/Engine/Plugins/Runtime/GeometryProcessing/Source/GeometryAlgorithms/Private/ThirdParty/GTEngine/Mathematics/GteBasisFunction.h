// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.3 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteArray2.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <cmath>
#include <cstring>

namespace gte
{

template <typename Real>
struct UniqueKnot
{
    Real t;
    int multiplicity;
};

template <typename Real>
struct BasisFunctionInput
{
    // The members are uninitialized.
    BasisFunctionInput();

    // Construct an open uniform curve with t in [0,1].
    BasisFunctionInput(int inNumControls, int inDegree);

    int numControls;
    int degree;
    bool uniform;
    bool periodic;
    int numUniqueKnots;
    std::vector<UniqueKnot<Real>> uniqueKnots;
};

template <typename Real>
class BasisFunction
{
public:
    // Let n be the number of control points. Let d be the degree, where
    // 1 <= d <= n-1.  The number of knots is k = n + d + 1.  The knots are
    // t[i] for 0 <= i < k and must be nondecreasing, t[i] <= t[i+1], but a
    // knot value can be repeated.  Let s be the number of distinct knots.
    // Let the distinct knots be u[j] for 0 <= j < s, so u[j] < u[j+1] for
    // all j.  The set of u[j] is called a 'breakpoint sequence'.  Let
    // m[j] >= 1 be the multiplicity; that is, if t[i] is the first occurrence
    // of u[j], then t[i+r] = t[i] for 1 <= r < m[j].  The multiplicities have
    // the constraints m[0] <= d+1, m[s-1] <= d+1, and m[j] <= d for
    // 1 <= j <= s-2.  Also, k = sum_{j=0}^{s-1} m[j], which says the
    // multiplicities account for all k knots.
    //
    // Given a knot vector (t[0],...,t[n+d]), the domain of the corresponding
    // B-spline curve is the interval [t[d],t[n]].
    //
    // The corresponding B-spline or NURBS curve is characterized as follows.
    // See "Geometric Modeling with Splines: An Introduction" by Elaine Cohen,
    // Richard F. Riesenfeld, and Gershon Elber, AK Peters, 2001, Natick MA.
    // The curve is 'open' when m[0] = m[s-1] = d+1; otherwise, it is
    // 'floating'.  An open curve is uniform when the knots t[d] through
    // t[n] are equally spaced; that is, t[i+1] - t[i] are a common value
    // for d <= i <= n-1.  By implication, s = n-d+1 and m[j] = 1 for
    // 1 <= j <= s-2.  An open curve that does not satisfy these conditions
    // is said to be nonuniform.  A floating curve is uniform when
    // m[j] = 1 for 0 <= j <= s-1 and t[i+1] - t[i] are a common value for
    // 0 <= i <= k-2; otherwise, the floating curve is nonuniform.
    //
    // A special case of a floating curve is a periodic curve.  The intent
    // is that the curve is closed, so the first and last control points
    // should be the same, which ensures C^{0} continuity.  Higher-order
    // continuity is obtained by repeating more control points.  If the
    // control points are P[0] through P[n-1], append the points P[0]
    // through P[d-1] to ensure C^{d-1} continuity.  Additionally the knots
    // must be chosen properly.  You may choose t[d] through t[n] as you
    // wish.  The other knots are defined by
    //   t[i] - t[i-1] = t[n-d+i] - t[n-d+i-1]
    //   t[n+i] - t[n+i-1] = t[d+i] - t[d+i-1]
    // for 1 <= i <= d.

    // Construction and destruction.  The determination that the curve is
    // open or floating is based on the multiplicities.  The 'uniform' input
    // is used to avoid misclassifications due to floating-point rounding
    // errors.  Specifically, the breakpoints might be equally spaced
    // (uniform) as real numbers, but the floating-point representations can
    // have rounding errors that cause the knot differences not to be exactly
    // the same constant.  A periodic curve can have uniform or nonuniform
    // knots.  This object makes copies of the input arrays.

    ~BasisFunction();
    BasisFunction();
    BasisFunction(BasisFunctionInput<Real> const& input);

    // No copying is allowed.
    BasisFunction(BasisFunction const&) = delete;
    BasisFunction& operator=(BasisFunction const&) = delete;

    // Support for explicit creation in classes that have std::array
    // members involving BasisFunction.  This is a call-once function.
    void Create(BasisFunctionInput<Real> const& input);

    // The inputs have complicated validation tests.  You should create a
    // BasisFunction object as shown:
    //     BasisFunction<Real> function(parameters);
    //     if (!function) { <constructor failed, respond accordingly>; }
    inline operator bool() const;

    // Member access.
    inline int GetNumControls() const;
    inline int GetDegree() const;
    inline int GetNumUniqueKnots() const;
    inline int GetNumKnots() const;
    inline Real GetMinDomain() const;
    inline Real GetMaxDomain() const;
    inline bool IsOpen() const;
    inline bool IsUniform() const;
    inline bool IsPeriodic() const;
    inline UniqueKnot<Real> const* GetUniqueKnots() const;
    inline Real const* GetKnots() const;

    // Evaluation of the basis function and its derivatives through order 3.
    // For the function value only, pass order 0.  For the function and first
    // derivative, pass order 1, and so on.
    void Evaluate(Real t, unsigned int order, int& minIndex, int& maxIndex)
        const;

    // Access the results of the call to Evaluate(...).  The index i must
    // satisfy minIndex <= i <= maxIndex.  If it is not, the function returns
    // zero.  The separation of evaluation and access is based on local
    // control of the basis function; that is, only the accessible values are
    // (potentially) not zero.
    Real GetValue(unsigned int order, int i) const;

private:
    // Determine the index i for which knot[i] <= t < knot[i+1].  The t-value
    // is modified (wrapped for periodic splines, clamped for nonperiodic
    // splines).
    int GetIndex(Real& t) const;

    // Constructor inputs and values derived from them.
    int mNumControls;
    int mDegree;
    Real mTMin, mTMax, mTLength;
    bool mOpen;
    bool mUniform;
    bool mPeriodic;
    bool mConstructed;
    std::vector<UniqueKnot<Real>> mUniqueKnots;
    std::vector<Real> mKnots;

    // Lookup information for the GetIndex() function.  The first element of
    // the pair is a unique knot value.  The second element is the index in
    // mKnots[] for the last occurrence of that knot value.  NOTE:  This is
    // a heavily used function during Evaluate calls.  This member was
    // initially set to be std::vector<*>, but in debug mode the range-based
    // for-loop in GetIndex was taking an extremely long time due to checked
    // calls involving iterators.  We modified this to a regular array to
    // ensure that debug performance is better.
    std::vector<std::pair<Real, int>> mKeys;

    // Storage for the basis functions and their first three derivatives.
    mutable Array2<Real> mValue[4];  // mValue[i] is array[d+1][n+d]
};


template <typename Real>
BasisFunctionInput<Real>::BasisFunctionInput()
{
}

template <typename Real>
BasisFunctionInput<Real>::BasisFunctionInput(int inNumControls, int inDegree)
    :
    numControls(inNumControls),
    degree(inDegree),
    uniform(true),
    periodic(false),
    numUniqueKnots(numControls - degree + 1),
    uniqueKnots(numUniqueKnots)
{
    uniqueKnots.front().t = (Real)0;
    uniqueKnots.front().multiplicity = degree + 1;
    for (int i = 1; i <= numUniqueKnots - 2; ++i)
    {
        uniqueKnots[i].t = i / (numUniqueKnots - (Real)1);
        uniqueKnots[i].multiplicity = 1;
    }
    uniqueKnots.back().t = (Real)1;
    uniqueKnots.back().multiplicity = degree + 1;
}

template <typename Real>
BasisFunction<Real>::~BasisFunction()
{
}

template <typename Real>
BasisFunction<Real>::BasisFunction()
    :
    mNumControls(0),
    mDegree(0),
    mTMin((Real)0),
    mTMax((Real)0),
    mTLength((Real)0),
    mOpen(false),
    mUniform(false),
    mPeriodic(false),
    mConstructed(false)
{
}

template <typename Real>
BasisFunction<Real>::BasisFunction(BasisFunctionInput<Real> const& input)
    :
    mConstructed(false)
{
    Create(input);
}


template <typename Real>
void BasisFunction<Real>::Create(BasisFunctionInput<Real> const& input)
{
    if (mConstructed)
    {
        LogError("Object already created.");
        return;
    }

    mNumControls = (input.periodic ? input.numControls + input.degree : input.numControls);
    mDegree = input.degree;
    mTMin = (Real)0;
    mTMax = (Real)0;
    mTLength = (Real)0;
    mOpen = false;
    mUniform = input.uniform;
    mPeriodic = input.periodic;
    for (int i = 0; i < 4; ++i)
    {
        mValue[i] = Array2<Real>();
    }

    if (input.numControls < 2)
    {
        LogError("Invalid number of control points.");
        return;
    }

    if (input.degree < 1 || input.degree >= input.numControls)
    {
        LogError("Invalid degree.");
        return;
    }

    if (input.numUniqueKnots < 2)
    {
        LogError("Invalid number of unique knots.");
        return;
    }

    mUniqueKnots.resize(input.numUniqueKnots);
    std::copy(input.uniqueKnots.begin(), input.uniqueKnots.begin() + input.numUniqueKnots,
        mUniqueKnots.begin());

    Real u = mUniqueKnots.front().t;
    for (int i = 1; i < input.numUniqueKnots - 1; ++i)
    {
        Real uNext = mUniqueKnots[i].t;
        if (u >= uNext)
        {
            LogError("Unique knots are not strictly increasing.");
            return;
        }
        u = uNext;
    }

    int mult0 = mUniqueKnots.front().multiplicity;
    if (mult0 < 1 || mult0 > mDegree + 1)
    {
        LogError("Invalid first multiplicity.");
        return;
    }

    int mult1 = mUniqueKnots.back().multiplicity;
    if (mult1 < 1 || mult1 > mDegree + 1)
    {
        LogError("Invalid last multiplicity.");
        return;
    }

    for (int i = 1; i <= input.numUniqueKnots - 2; ++i)
    {
        int mult = mUniqueKnots[i].multiplicity;
        if (mult < 1 || mult > mDegree)
        {
            LogError("Invalid interior multiplicity.");
            return;
        }
    }

    mOpen = (mult0 == mult1 && mult0 == mDegree + 1);

    mKnots.resize(mNumControls + mDegree + 1);
    mKeys.resize(input.numUniqueKnots);
    int sum = 0;
    for (int i = 0, j = 0; i < input.numUniqueKnots; ++i)
    {
        Real tCommon = mUniqueKnots[i].t;
        int mult = mUniqueKnots[i].multiplicity;
        for (int k = 0; k < mult; ++k, ++j)
        {
            mKnots[j] = tCommon;
        }

        mKeys[i].first = tCommon;
        mKeys[i].second = sum - 1;
        sum += mult;
    }

    mTMin = mKnots[mDegree];
    mTMax = mKnots[mNumControls];
    mTLength = mTMax - mTMin;

    size_t numRows = mDegree + 1;
    size_t numCols = mNumControls + mDegree;
    size_t numBytes = numRows * numCols * sizeof(Real);
    for (int i = 0; i < 4; ++i)
    {
        mValue[i] = Array2<Real>(numCols, numRows);
        memset(mValue[i][0], 0, numBytes);
    }

    mConstructed = true;
}

template <typename Real> inline
BasisFunction<Real>::operator bool() const
{
    return mConstructed;
}

template <typename Real> inline
int BasisFunction<Real>::GetNumControls() const
{
    return mNumControls;
}

template <typename Real> inline
int BasisFunction<Real>::GetDegree() const
{
    return mDegree;
}

template <typename Real> inline
int BasisFunction<Real>::GetNumUniqueKnots() const
{
    return static_cast<int>(mUniqueKnots.size());
}

template <typename Real> inline
int BasisFunction<Real>::GetNumKnots() const
{
    return static_cast<int>(mKnots.size());
}

template <typename Real> inline
Real BasisFunction<Real>::GetMinDomain() const
{
    return mTMin;
}

template <typename Real> inline
Real BasisFunction<Real>::GetMaxDomain() const
{
    return mTMax;
}

template <typename Real> inline
bool BasisFunction<Real>::IsOpen() const
{
    return mOpen;
}

template <typename Real> inline
bool BasisFunction<Real>::IsUniform() const
{
    return mUniform;
}

template <typename Real> inline
bool BasisFunction<Real>::IsPeriodic() const
{
    return mPeriodic;
}

template <typename Real> inline
UniqueKnot<Real> const* BasisFunction<Real>::GetUniqueKnots() const
{
    return &mUniqueKnots[0];
}

template <typename Real> inline
Real const* BasisFunction<Real>::GetKnots() const
{
    return &mKnots[0];
}

template <typename Real>
void BasisFunction<Real>::Evaluate(Real t, unsigned int order, int& minIndex,
    int& maxIndex) const
{
    if (!mConstructed)
    {
        // Errors were already generated during construction.  Return an index
        // range that leads to zero-valued positions and derivatives.
        minIndex = -1;
        maxIndex = -1;
        return;
    }

    if (order > 3)
    {
        LogError("Only derivatives through order 3 are supported.");
        minIndex = 0;
        maxIndex = 0;
        return;
    }

    int i = GetIndex(t);
    mValue[0][0][i] = (Real)1;

    if (order >= 1)
    {
        mValue[1][0][i] = (Real)0;
        if (order >= 2)
        {
            mValue[2][0][i] = (Real)0;
            if (order >= 3)
            {
                mValue[3][0][i] = (Real)0;
            }
        }
    }

    Real n0 = t - mKnots[i], n1 = mKnots[i + 1] - t;
    Real e0, e1, d0, d1, invD0, invD1;
    int j;
    for (j = 1; j <= mDegree; j++)
    {
        d0 = mKnots[i + j] - mKnots[i];
        d1 = mKnots[i + 1] - mKnots[i - j + 1];
        invD0 = (d0 > (Real)0 ? (Real)1 / d0 : (Real)0);
        invD1 = (d1 > (Real)0 ? (Real)1 / d1 : (Real)0);

        e0 = n0*mValue[0][j - 1][i];
        mValue[0][j][i] = e0*invD0;
        e1 = n1*mValue[0][j - 1][i - j + 1];
        mValue[0][j][i - j] = e1*invD1;

        if (order >= 1)
        {
            e0 = n0 * mValue[1][j - 1][i] + mValue[0][j - 1][i];
            mValue[1][j][i] = e0 * invD0;
            e1 = n1 * mValue[1][j - 1][i - j + 1] - mValue[0][j - 1][i - j + 1];
            mValue[1][j][i - j] = e1 * invD1;

            if (order >= 2)
            {
                e0 = n0 * mValue[2][j - 1][i] + ((Real)2) * mValue[1][j - 1][i];
                mValue[2][j][i] = e0 * invD0;
                e1 = n1 * mValue[2][j - 1][i - j + 1] - ((Real)2) * mValue[1][j - 1][i - j + 1];
                mValue[2][j][i - j] = e1 * invD1;

                if (order >= 3)
                {
                    e0 = n0 * mValue[3][j - 1][i] + ((Real)3) * mValue[2][j - 1][i];
                    mValue[3][j][i] = e0 * invD0;
                    e1 = n1 * mValue[3][j - 1][i - j + 1] - ((Real)3) * mValue[2][j - 1][i - j + 1];
                    mValue[3][j][i - j] = e1 * invD1;
                }
            }
        }
    }

    for (j = 2; j <= mDegree; ++j)
    {
        for (int k = i - j + 1; k < i; ++k)
        {
            n0 = t - mKnots[k];
            n1 = mKnots[k + j + 1] - t;
            d0 = mKnots[k + j] - mKnots[k];
            d1 = mKnots[k + j + 1] - mKnots[k + 1];
            invD0 = (d0 >(Real)0 ? (Real)1 / d0 : (Real)0);
            invD1 = (d1 > (Real)0 ? (Real)1 / d1 : (Real)0);

            e0 = n0*mValue[0][j - 1][k];
            e1 = n1*mValue[0][j - 1][k + 1];
            mValue[0][j][k] = e0 * invD0 + e1 * invD1;

            if (order >= 1)
            {
                e0 = n0 * mValue[1][j - 1][k] + mValue[0][j - 1][k];
                e1 = n1 * mValue[1][j - 1][k + 1] - mValue[0][j - 1][k + 1];
                mValue[1][j][k] = e0 * invD0 + e1 * invD1;

                if (order >= 2)
                {
                    e0 = n0 * mValue[2][j - 1][k] + ((Real)2) * mValue[1][j - 1][k];
                    e1 = n1 * mValue[2][j - 1][k + 1] - ((Real)2) * mValue[1][j - 1][k + 1];
                    mValue[2][j][k] = e0 * invD0 + e1 * invD1;

                    if (order >= 3)
                    {
                        e0 = n0 * mValue[3][j - 1][k] + ((Real)3) * mValue[2][j - 1][k];
                        e1 = n1 * mValue[3][j - 1][k + 1] - ((Real)3) * mValue[2][j - 1][k + 1];
                        mValue[3][j][k] = e0 * invD0 + e1 * invD1;
                    }
                }
            }
        }
    }

    minIndex = i - mDegree;
    maxIndex = i;
}

template <typename Real>
Real BasisFunction<Real>::GetValue(unsigned int order, int i) const
{
    if (!mConstructed)
    {
        // Errors were already generated during construction.  Return a value
        // that leads to zero-valued positions and derivatives.
        return (Real)0;
    }

    if (order < 4)
    {
        if (0 <= i && i < mNumControls + mDegree)
        {
            return mValue[order][mDegree][i];
        }
    }

    LogError("Invalid input.");
    return (Real)0;
}

template <typename Real>
int BasisFunction<Real>::GetIndex(Real& t) const
{
    // Find the index i for which knot[i] <= t < knot[i+1].
    if (mPeriodic)
    {
        // Wrap to [tmin,tmax].
        Real r = std::fmod(t - mTMin, mTLength);
        if (r < (Real)0)
        {
            r += mTLength;
        }
        t = mTMin + r;
    }

    // Clamp to [tmin,tmax].  For the periodic case, this handles small
    // numerical rounding errors near the domain endpoints.
    if (t <= mTMin)
    {
        t = mTMin;
        return mDegree;
    }
    if (t >= mTMax)
    {
        t = mTMax;
        return mNumControls - 1;
    }

    // At this point, tmin < t < tmax.
    for (auto const& key : mKeys)
    {
        if (t < key.first)
        {
            return key.second;
        }
    }

    // We should not reach this code.
    LogError("Unexpected condition.");
    t = mTMin;
    return mDegree;
}

}
