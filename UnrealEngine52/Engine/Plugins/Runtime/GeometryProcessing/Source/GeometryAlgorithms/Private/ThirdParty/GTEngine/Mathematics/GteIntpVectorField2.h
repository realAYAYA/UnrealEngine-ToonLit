// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDelaunay2Mesh.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntpQuadraticNonuniform2.h>
#include <memory>

// Given points (x0[i],y0[i]) which are mapped to (x1[i],y1[i]) for
// 0 <= i < N, interpolate positions (xIn,yIn) to (xOut,yOut).

namespace gte
{

template <typename InputType, typename ComputeType, typename RationalType>
class IntpVectorField2
{
public:
    // Construction and destruction.
    ~IntpVectorField2();
    IntpVectorField2(int numPoints, Vector2<InputType> const* domain,
        Vector2<InputType> const* range);

    // The return value is 'true' if and only if (xIn,yIn) is in the convex
    // hull of the input domain points, in which case the interpolation is
    // valid.
    bool operator()(Vector2<InputType> const& input,
        Vector2<InputType>& output) const;

protected:
    typedef Delaunay2Mesh<InputType, ComputeType, RationalType> TriangleMesh;
    Delaunay2<InputType, ComputeType> mDelaunay;
    TriangleMesh mMesh;

    std::vector<InputType> mXRange;
    std::vector<InputType> mYRange;
    std::unique_ptr<IntpQuadraticNonuniform2<InputType, TriangleMesh>> mXInterp;
    std::unique_ptr<IntpQuadraticNonuniform2<InputType, TriangleMesh>> mYInterp;
};


template <typename InputType, typename ComputeType, typename RationalType>
IntpVectorField2<InputType, ComputeType, RationalType>::~IntpVectorField2()
{
}

template <typename InputType, typename ComputeType, typename RationalType>
IntpVectorField2<InputType, ComputeType, RationalType>::IntpVectorField2(
    int numPoints, Vector2<InputType> const* domain,
    Vector2<InputType> const* range)
    :
    mMesh(mDelaunay)
{
    // Repackage the output vectors into individual components.  This is
    // required because of the format that the quadratic interpolator expects
    // for its input data.
    mXRange.resize(numPoints);
    mYRange.resize(numPoints);
    for (int i = 0; i < numPoints; ++i)
    {
        mXRange[i] = range[i][0];
        mYRange[i] = range[i][1];
    }

    // Common triangulator for the interpolators.
    mDelaunay(numPoints, &domain[0], (ComputeType)0);

    // Create interpolator for x-coordinate of vector field.
    mXInterp = std::make_unique<IntpQuadraticNonuniform2<InputType, TriangleMesh>>(
        mMesh, &mXRange[0], (InputType)1);

    // Create interpolator for y-coordinate of vector field, but share the
    // already created triangulation for the x-interpolator.
    mYInterp = std::make_unique<IntpQuadraticNonuniform2<InputType, TriangleMesh>>(
        mMesh, &mYRange[0], (InputType)1);
}

template <typename InputType, typename ComputeType, typename RationalType>
bool IntpVectorField2<InputType, ComputeType, RationalType>::operator()(
    Vector2<InputType> const& input, Vector2<InputType>& output) const
{
    InputType xDeriv, yDeriv;
    return (*mXInterp)(input, output[0], xDeriv, yDeriv)
        && (*mYInterp)(input, output[1], xDeriv, yDeriv);
}


}
