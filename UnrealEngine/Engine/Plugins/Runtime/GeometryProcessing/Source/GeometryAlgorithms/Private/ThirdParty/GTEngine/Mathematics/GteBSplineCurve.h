// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/02/17)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteBasisFunction.h>
#include <ThirdParty/GTEngine/Mathematics/GteParametricCurve.h>

namespace gte
{

template <int N, typename Real>
class BSplineCurve : public ParametricCurve<N, Real>
{
public:
    // Construction.  If the input controls is non-null, a copy is made of
    // the controls.  To defer setting the control points, pass a null pointer
    // and later access the control points via GetControls() or SetControl()
    // member functions.  The domain is t in [t[d],t[n]], where t[d] and t[n]
    // are knots with d the degree and n the number of control points.  To
    // validate construction, create an object as shown:
    //     BSplineCurve<N, Real> curve(parameters);
    //     if (!curve) { <constructor failed, handle accordingly>; }
    BSplineCurve(BasisFunctionInput<Real> const& input,
        Vector<N, Real> const* controls);

    // Member access.
    inline BasisFunction<Real> const& GetBasisFunction() const;
    inline int GetNumControls() const;
    inline Vector<N, Real> const* GetControls() const;
    inline Vector<N, Real>* GetControls();
    void SetControl(int i, Vector<N, Real> const& control);
    Vector<N, Real> const& GetControl(int i) const;

    // Evaluation of the curve.  The function supports derivative calculation
    // through order 3; that is, maxOrder <= 3 is required.  If you want
    // only the position, pass in maxOrder of 0.  If you want the position and
    // first derivative, pass in maxOrder of 1, and so on.  The output
    // 'values' are ordered as: position, first derivative, second derivative,
    // third derivative.
    virtual void Evaluate(Real t, unsigned int maxOrder,
        Vector<N, Real> values[4]) const;

private:
    // Support for Evaluate(...).
    Vector<N, Real> Compute(unsigned int order, int imin, int imax) const;

    BasisFunction<Real> mBasisFunction;
    std::vector<Vector<N, Real>> mControls;
};


template <int N, typename Real>
BSplineCurve<N, Real>::BSplineCurve(BasisFunctionInput<Real> const& input,
    Vector<N, Real> const* controls)
    :
    ParametricCurve<N, Real>((Real)0, (Real)1),
    mBasisFunction(input)
{
    if (!mBasisFunction)
    {
        // Errors were already generated during construction of the
        // basis function.
        return;
    }

    // The mBasisFunction stores the domain but so does ParametricCurve.
    this->mTime.front() = mBasisFunction.GetMinDomain();
    this->mTime.back() = mBasisFunction.GetMaxDomain();

    // The replication of control points for periodic splines is avoided
    // by wrapping the i-loop index in Evaluate.
    mControls.resize(input.numControls);
    if (controls)
    {
        std::copy(controls, controls + input.numControls, mControls.begin());
    }
    else
    {
        Vector<N, Real> zero{ (Real)0 };
        std::fill(mControls.begin(), mControls.end(), zero);
    }
    this->mConstructed = true;
}

template <int N, typename Real>
BasisFunction<Real> const& BSplineCurve<N, Real>::GetBasisFunction() const
{
    return mBasisFunction;
}

template <int N, typename Real>
int BSplineCurve<N, Real>::GetNumControls() const
{
    return static_cast<int>(mControls.size());
}

template <int N, typename Real>
Vector<N, Real> const* BSplineCurve<N, Real>::GetControls() const
{
    return mControls.data();
}

template <int N, typename Real>
Vector<N, Real>* BSplineCurve<N, Real>::GetControls()
{
    return mControls.data();
}

template <int N, typename Real>
void BSplineCurve<N, Real>::SetControl(int i, Vector<N, Real> const& control)
{
    if (0 <= i && i < GetNumControls())
    {
        mControls[i] = control;
    }
}

template <int N, typename Real>
Vector<N, Real> const& BSplineCurve<N, Real>::GetControl(int i) const
{
    if (0 <= i && i < GetNumControls())
    {
        return mControls[i];
    }
    else
    {
        return mControls[0];
    }
}

template <int N, typename Real>
void BSplineCurve<N, Real>::Evaluate(Real t, unsigned int maxOrder,
    Vector<N, Real> values[4]) const
{
    if (!this->mConstructed)
    {
        // Errors were already generated during construction.
        for (unsigned int order = 0; order < 4; ++order)
        {
            values[order].MakeZero();
        }
        return;
    }

    int imin, imax;
    mBasisFunction.Evaluate(t, maxOrder, imin, imax);

    // Compute position.
    values[0] = Compute(0, imin, imax);
    if (maxOrder >= 1)
    {
        // Compute first derivative.
        values[1] = Compute(1, imin, imax);
        if (maxOrder >= 2)
        {
            // Compute second derivative.
            values[2] = Compute(2, imin, imax);
            if (maxOrder == 3)
            {
                values[3] = Compute(3, imin, imax);
            }
            else
            {
                values[3].MakeZero();
            }
        }
    }
}

template <int N, typename Real>
Vector<N, Real> BSplineCurve<N, Real>::Compute(unsigned int order, int imin,
    int imax) const
{
    // The j-index introduces a tiny amount of overhead in order to handle
    // both aperiodic and periodic splines.  For aperiodic splines, j = i
    // always.

    int numControls = GetNumControls();
    Vector<N, Real> result;
    result.MakeZero();
    for (int i = imin; i <= imax; ++i)
    {
        Real tmp = mBasisFunction.GetValue(order, i);
        int j = (i >= numControls ? i - numControls : i);
        result += tmp * mControls[j];
    }
    return result;
}

}
