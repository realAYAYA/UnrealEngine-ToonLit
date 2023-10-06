// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/02/17)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteBasisFunction.h>
#include <ThirdParty/GTEngine/Mathematics/GteParametricSurface.h>

namespace gte
{

template <int N, typename Real>
class BSplineSurface : public ParametricSurface<N, Real>
{
public:
    // Construction.  If the input controls is non-null, a copy is made of
    // the controls.  To defer setting the control points, pass a null pointer
    // and later access the control points via GetControls() or SetControl()
    // member functions.  The input 'controls' must be stored in row-major
    // order, control[i0 + numControls0*i1].  As a 2D array, this corresponds
    // to control2D[i1][i0].  To validate construction, create an object as
    // shown:
    //     BSplineSurface<N,Real> surface(parameters);
    //     if (!surface) { <constructor failed, handle accordingly>; }
    BSplineSurface(BasisFunctionInput<Real> const input[2],
        Vector<N, Real> const* controls);

    // Member access.  The index 'dim' must be in {0,1}.
    inline BasisFunction<Real> const& GetBasisFunction(int dim) const;
    inline int GetNumControls(int dim) const;
    inline Vector<N, Real>* GetControls();
    inline Vector<N, Real> const* GetControls() const;
    void SetControl(int i0, int i1, Vector<N, Real> const& control);
    Vector<N, Real> const& GetControl(int i0, int i1) const;

    // Evaluation of the surface.  The function supports derivative
    // calculation through order 2; that is, maxOrder <= 2 is required.  If
    // you want only the position, pass in maxOrder of 0.  If you want the
    // position and first-order derivatives, pass in maxOrder of 1, and so on.
    // The output 'values' are ordered as: position X; first-order derivatives
    // dX/du, dX/dv; second-order derivatives d2X/du2, d2X/dudv, d2X/dv2.
    virtual void Evaluate(Real u, Real v, unsigned int maxOrder,
        Vector<N, Real> values[6]) const;

private:
    // Support for Evaluate(...).
    Vector<N, Real> Compute(unsigned int uOrder, unsigned int vOrder,
        int iumin, int iumax, int ivmin, int ivmax) const;

    std::array<BasisFunction<Real>, 2> mBasisFunction;
    std::array<int, 2> mNumControls;
    std::vector<Vector<N, Real>> mControls;
};


template <int N, typename Real>
BSplineSurface<N, Real>::BSplineSurface(BasisFunctionInput<Real> const input[2],
    Vector<N, Real> const* controls)
    :
    ParametricSurface<N, Real>((Real)0, (Real)1, (Real)0, (Real)1, true)
{
    for (int i = 0; i < 2; ++i)
    {
        mNumControls[i] = input[i].numControls;
        mBasisFunction[i].Create(input[i]);
        if (!mBasisFunction[i])
        {
            // Errors were already generated during construction of the
            // basis functions.
            return;
        }
    }

    // The mBasisFunction stores the domain but so does ParametricCurve.
    this->mUMin = mBasisFunction[0].GetMinDomain();
    this->mUMax = mBasisFunction[0].GetMaxDomain();
    this->mVMin = mBasisFunction[1].GetMinDomain();
    this->mVMax = mBasisFunction[1].GetMaxDomain();

    // The replication of control points for periodic splines is avoided
    // by wrapping the i-loop index in Evaluate.
    int numControls = mNumControls[0] * mNumControls[1];
    mControls.resize(numControls);
    if (controls)
    {
        std::copy(controls, controls + numControls, mControls.begin());
    }
    else
    {
        Vector<N, Real> zero{ (Real)0 };
        std::fill(mControls.begin(), mControls.end(), zero);
    }
    this->mConstructed = true;
}

template <int N, typename Real>
BasisFunction<Real> const& BSplineSurface<N, Real>::GetBasisFunction(int dim) const
{
    return mBasisFunction[dim];
}

template <int N, typename Real>
int BSplineSurface<N, Real>::GetNumControls(int dim) const
{
    return mNumControls[dim];
}

template <int N, typename Real>
Vector<N, Real> const* BSplineSurface<N, Real>::GetControls() const
{
    return mControls.data();
}

template <int N, typename Real>
Vector<N, Real>* BSplineSurface<N, Real>::GetControls()
{
    return mControls.data();
}

template <int N, typename Real>
void BSplineSurface<N, Real>::SetControl(int i0, int i1, Vector<N, Real> const& control)
{
    if (0 <= i0 && i0 < GetNumControls(0)
        && 0 <= i1 && i1 < GetNumControls(1))
    {
        mControls[i0 + mNumControls[0] * i1] = control;
    }
}

template <int N, typename Real>
Vector<N, Real> const& BSplineSurface<N, Real>::GetControl(int i0, int i1) const
{
    if (0 <= i0 && i0 < GetNumControls(0) && 0 <= i1 && i1 < GetNumControls(1))
    {
        return mControls[i0 + mNumControls[0] * i1];
    }
    else
    {
        return mControls[0];
    }
}

template <int N, typename Real>
void BSplineSurface<N, Real>::Evaluate(Real u, Real v, unsigned int maxOrder,
    Vector<N, Real> values[6]) const
{
    if (!this->mConstructed)
    {
        // Errors were already generated during construction.
        for (int i = 0; i < 6; ++i)
        {
            values[i].MakeZero();
        }
        return;
    }

    int iumin, iumax, ivmin, ivmax;
    mBasisFunction[0].Evaluate(u, maxOrder, iumin, iumax);
    mBasisFunction[1].Evaluate(v, maxOrder, ivmin, ivmax);

    // Compute position.
    values[0] = Compute(0, 0, iumin, iumax, ivmin, ivmax);
    if (maxOrder >= 1)
    {
        // Compute first-order derivatives.
        values[1] = Compute(1, 0, iumin, iumax, ivmin, ivmax);
        values[2] = Compute(0, 1, iumin, iumax, ivmin, ivmax);
        if (maxOrder >= 2)
        {
            // Compute second-order derivatives.
            values[3] = Compute(2, 0, iumin, iumax, ivmin, ivmax);
            values[4] = Compute(1, 1, iumin, iumax, ivmin, ivmax);
            values[5] = Compute(0, 2, iumin, iumax, ivmin, ivmax);
        }
    }
}

template <int N, typename Real>
Vector<N, Real> BSplineSurface<N, Real>::Compute(unsigned int uOrder,
    unsigned int vOrder, int iumin, int iumax, int ivmin, int ivmax) const
{
    // The j*-indices introduce a tiny amount of overhead in order to handle
    // both aperiodic and periodic splines.  For aperiodic splines, j* = i*
    // always.

    int const numControls0 = mNumControls[0];
    int const numControls1 = mNumControls[1];
    Vector<N, Real> result;
    result.MakeZero();
    for (int iv = ivmin; iv <= ivmax; ++iv)
    {
        Real tmpv = mBasisFunction[1].GetValue(vOrder, iv);
        int jv = (iv >= numControls1 ? iv - numControls1 : iv);
        for (int iu = iumin; iu <= iumax; ++iu)
        {
            Real tmpu = mBasisFunction[0].GetValue(uOrder, iu);
            int ju = (iu >= numControls0 ? iu - numControls0 : iu);
            result += (tmpu * tmpv) * mControls[ju + numControls0 * jv];
        }
    }
    return result;
}

}
