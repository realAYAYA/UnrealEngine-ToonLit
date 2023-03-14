// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/28)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteContCircle2.h>
#include <ThirdParty/GTEngine/Mathematics/GteHypersphere.h>
#include <ThirdParty/GTEngine/Mathematics/GteLinearSystem.h>
#include <algorithm>
#include <random>
#include <utility>

// Compute the minimum area circle containing the input set of points.  The
// algorithm randomly permutes the input points so that the construction
// occurs in 'expected' O(N) time.  All internal minimal circle calculations
// store the squared radius in the radius member of Circle2.  Only at the
// end is a sqrt computed.
//
// The most robust choice for ComputeType is BSRational<T> for exact rational
// arithmetic.  As long as this code is a correct implementation of the theory
// (which I hope it is), you will obtain the minimum-area circle containing
// the points.
//
// Instead, if you choose ComputeType to be float or double, floating-point
// rounding errors can cause the UpdateSupport{2,3} functions to fail.
// The failure is trapped in those functions and a simple bounding circle is
// computed using GetContainer in file GteContCircle2.h.  This circle is
// generally not the minimum-area circle containing the points.  The
// minimum-area algorithm is terminated immediately.  The circle is returned
// as well as a bool value of 'true' when the circle is minimum area or
// 'false' when the failure is trapped.  When 'false' is returned, you can
// try another call to the operator()(...) function.  The random shuffle
// that occurs is highly likely to be different from the previous shuffle,
// and there is a chance that the algorithm can succeed just because of the
// different ordering of points.

namespace gte
{
    template <typename InputType, typename ComputeType>
    class MinimumAreaCircle2
    {
    public:
        bool operator()(int numPoints, Vector2<InputType> const* points, Circle2<InputType>& minimal)
        {
            if (numPoints >= 1 && points)
            {
                // Function array to avoid switch statement in the main loop.
                std::function<UpdateResult(int)> update[4];
                update[1] = [this](int i) { return UpdateSupport1(i); };
                update[2] = [this](int i) { return UpdateSupport2(i); };
                update[3] = [this](int i) { return UpdateSupport3(i); };

                // Process only the unique points.
                std::vector<int> permuted(numPoints);
                for (int i = 0; i < numPoints; ++i)
                {
                    permuted[i] = i;
                }
                std::sort(permuted.begin(), permuted.end(),
                    [points](int i0, int i1) { return points[i0] < points[i1]; });
                auto end = std::unique(permuted.begin(), permuted.end(),
                    [points](int i0, int i1) { return points[i0] == points[i1]; });
                permuted.erase(end, permuted.end());
                numPoints = static_cast<int>(permuted.size());

                // Create a random permutation of the points.
                std::shuffle(permuted.begin(), permuted.end(), mDRE);

                // Convert to the compute type, which is a simple copy when
                // ComputeType is the same as InputType.
                mComputePoints.resize(numPoints);
                for (int i = 0; i < numPoints; ++i)
                {
                    for (int j = 0; j < 2; ++j)
                    {
                        mComputePoints[i][j] = points[permuted[i]][j];
                    }
                }

                // Start with the first point.
                Circle2<ComputeType> ctMinimal = ExactCircle1(0);
                mNumSupport = 1;
                mSupport[0] = 0;

                // The loop restarts from the beginning of the point list each
                // time the circle needs updating.  Linus Kallberg (Computer
                // Science at Malardalen University in Sweden) discovered that
                // performance is better when the remaining points in the
                // array are processed before restarting.  The points
                // processed before the point that caused the update are
                // likely to be enclosed by the new circle (or near the circle
                // boundary) because they were enclosed by the previous
                // circle.  The chances are better that points after the
                // current one will cause growth of the bounding circle.
                for (int i = 1 % numPoints, n = 0; i != n; i = (i + 1) % numPoints)
                {
                    if (!SupportContains(i))
                    {
                        if (!Contains(i, ctMinimal))
                        {
                            auto result = update[mNumSupport](i);
                            if (result.second == true)
                            {
                                if (result.first.radius > ctMinimal.radius)
                                {
                                    ctMinimal = result.first;
                                    n = i;
                                }
                            }
                            else
                            {
                                // This case can happen when ComputeType is
                                // float or double.  See the comments at the
                                // beginning of this file.
                                LogWarning("ComputeType is not exact and failure occurred. Returning non-minimal circle.");
                                GetContainer(numPoints, points, minimal);
                                mNumSupport = 0;
                                mSupport.fill(0);
                                return false;
                            }
                        }
                    }
                }

                for (int j = 0; j < 2; ++j)
                {
                    minimal.center[j] = static_cast<InputType>(ctMinimal.center[j]);
                }
                minimal.radius = static_cast<InputType>(ctMinimal.radius);
                minimal.radius = std::sqrt(minimal.radius);

                for (int i = 0; i < mNumSupport; ++i)
                {
                    mSupport[i] = permuted[mSupport[i]];
                }
                return true;
            }
            else
            {
                LogError("Input must contain points.");
                minimal.center = Vector2<InputType>::Zero();
                minimal.radius = std::numeric_limits<InputType>::max();
                return false;
            }
        }

        // Member access.
        inline int GetNumSupport() const
        {
            return mNumSupport;
        }

        inline std::array<int, 3> const& GetSupport() const
        {
            return mSupport;
        }

    private:
        // Test whether point P is inside circle C using squared distance and
        // squared radius.
        bool Contains(int i, Circle2<ComputeType> const& circle) const
        {
            // NOTE: In this algorithm, circle.radius is the *squared radius*
            // until the function returns at which time a square root is
            // applied.
            Vector2<ComputeType> diff = mComputePoints[i] - circle.center;
            return Dot(diff, diff) <= circle.radius;
        }

        Circle2<ComputeType> ExactCircle1(int i0) const
        {
            Circle2<ComputeType> minimal;
            minimal.center = mComputePoints[i0];
            minimal.radius = (ComputeType)0;
            return minimal;
        }

        Circle2<ComputeType> ExactCircle2(int i0, int i1) const
        {
            Vector2<ComputeType> const& P0 = mComputePoints[i0];
            Vector2<ComputeType> const& P1 = mComputePoints[i1];
            Vector2<ComputeType> diff = P1 - P0;
            Circle2<ComputeType> minimal;
            minimal.center = ((ComputeType)0.5)*(P0 + P1);
            minimal.radius = ((ComputeType)0.25)*Dot(diff, diff);
            return minimal;
        }

        Circle2<ComputeType> ExactCircle3(int i0, int i1, int i2) const
        {
            // Compute the 2D circle containing P0, P1, and P2.  The center in
            // barycentric coordinates is C = x0*P0 + x1*P1 + x2*P2, where
            // x0 + x1 + x2 = 1.  The center is equidistant from the three
            // points, so |C - P0| = |C - P1| = |C - P2| = R, where R is the
            // radius of the circle.  From these conditions,
            //   C - P0 = x0*E0 + x1*E1 - E0
            //   C - P1 = x0*E0 + x1*E1 - E1
            //   C - P2 = x0*E0 + x1*E1
            // where E0 = P0 - P2 and E1 = P1 - P2, which leads to
            //   r^2 = |x0*E0 + x1*E1|^2 - 2*Dot(E0, x0*E0 + x1*E1) + |E0|^2
            //   r^2 = |x0*E0 + x1*E1|^2 - 2*Dot(E1, x0*E0 + x1*E1) + |E1|^2
            //   r^2 = |x0*E0 + x1*E1|^2
            // Subtracting the last equation from the first two and writing
            // the equations as a linear system,
            //
            // +-                     -++   -+       +-          -+
            // | Dot(E0,E0) Dot(E0,E1) || x0 | = 0.5 | Dot(E0,E0) |
            // | Dot(E1,E0) Dot(E1,E1) || x1 |       | Dot(E1,E1) |
            // +-                     -++   -+       +-          -+
            //
            // The following code solves this system for x0 and x1 and then
            // evaluates the third equation in r^2 to obtain r.

            Vector2<ComputeType> const& P0 = mComputePoints[i0];
            Vector2<ComputeType> const& P1 = mComputePoints[i1];
            Vector2<ComputeType> const& P2 = mComputePoints[i2];

            Vector2<ComputeType> E0 = P0 - P2;
            Vector2<ComputeType> E1 = P1 - P2;

            Matrix2x2<ComputeType> A;
            A(0, 0) = Dot(E0, E0);
            A(0, 1) = Dot(E0, E1);
            A(1, 0) = A(0, 1);
            A(1, 1) = Dot(E1, E1);

            ComputeType const half = (ComputeType)0.5;
            Vector2<ComputeType> B{ half * A(0, 0), half* A(1, 1) };

            Circle2<ComputeType> minimal;
            Vector2<ComputeType> X;
            if (LinearSystem<ComputeType>::Solve(A, B, X))
            {
                ComputeType x2 = (ComputeType)1 - X[0] - X[1];
                minimal.center = X[0] * P0 + X[1] * P1 + x2 * P2;
                Vector2<ComputeType> tmp = X[0] * E0 + X[1] * E1;
                minimal.radius = Dot(tmp, tmp);
            }
            else
            {
                minimal.center = Vector2<ComputeType>::Zero();
                minimal.radius = (ComputeType)std::numeric_limits<InputType>::max();
            }

            return minimal;
        }

        typedef std::pair<Circle2<ComputeType>, bool> UpdateResult;

        UpdateResult UpdateSupport1(int i)
        {
            Circle2<ComputeType> minimal = ExactCircle2(mSupport[0], i);
            mNumSupport = 2;
            mSupport[1] = i;
            return std::make_pair(minimal, true);
        }

        UpdateResult UpdateSupport2(int i)
        {
            // Permutations of type 2, used for calling ExactCircle2(...).
            int const numType2 = 2;
            int const type2[numType2][2] =
            {
                { 0, /*2*/ 1 },
                { 1, /*2*/ 0 }
            };

            // Permutations of type 3, used for calling ExactCircle3(...).
            int const numType3 = 1;  // {0, 1, 2}

            Circle2<ComputeType> circle[numType2 + numType3];
            ComputeType minRSqr = (ComputeType)std::numeric_limits<InputType>::max();
            int iCircle = 0, iMinRSqr = -1;
            int k0, k1;

            // Permutations of type 2.
            for (int j = 0; j < numType2; ++j, ++iCircle)
            {
                k0 = mSupport[type2[j][0]];
                circle[iCircle] = ExactCircle2(k0, i);
                if (circle[iCircle].radius < minRSqr)
                {
                    k1 = mSupport[type2[j][1]];
                    if (Contains(k1, circle[iCircle]))
                    {
                        minRSqr = circle[iCircle].radius;
                        iMinRSqr = iCircle;
                    }
                }
            }

            // Permutations of type 3.
            k0 = mSupport[0];
            k1 = mSupport[1];
            circle[iCircle] = ExactCircle3(k0, k1, i);
            if (circle[iCircle].radius < minRSqr)
            {
                minRSqr = circle[iCircle].radius;
                iMinRSqr = iCircle;
            }

            switch (iMinRSqr)
            {
            case 0:
                mSupport[1] = i;
                break;
            case 1:
                mSupport[0] = i;
                break;
            case 2:
                mNumSupport = 3;
                mSupport[2] = i;
                break;
            case -1:
                // For exact arithmetic, iMinRSqr >= 0, but for floating-point
                // arithmetic, round-off errors can lead to iMinRSqr == -1.
                // When this happens, use a simple bounding circle for the
                // result and terminate the minimum-area algorithm.
                return std::make_pair(Circle2<ComputeType>(), false);
            }

            return std::make_pair(circle[iMinRSqr], true);
        }

        UpdateResult UpdateSupport3(int i)
        {
            // Permutations of type 2, used for calling ExactCircle2(...).
            int const numType2 = 3;
            int const type2[numType2][3] =
            {
                { 0, /*3*/ 1, 2 },
                { 1, /*3*/ 0, 2 },
                { 2, /*3*/ 0, 1 }
            };

            // Permutations of type 2, used for calling ExactCircle3(...).
            int const numType3 = 3;
            int const type3[numType3][3] =
            {
                { 0, 1, /*3*/ 2 },
                { 0, 2, /*3*/ 1 },
                { 1, 2, /*3*/ 0 }
            };

            Circle2<ComputeType> circle[numType2 + numType3];
            ComputeType minRSqr = (ComputeType)std::numeric_limits<InputType>::max();
            int iCircle = 0, iMinRSqr = -1;
            int k0, k1, k2;

            // Permutations of type 2.
            for (int j = 0; j < numType2; ++j, ++iCircle)
            {
                k0 = mSupport[type2[j][0]];
                circle[iCircle] = ExactCircle2(k0, i);
                if (circle[iCircle].radius < minRSqr)
                {
                    k1 = mSupport[type2[j][1]];
                    k2 = mSupport[type2[j][2]];
                    if (Contains(k1, circle[iCircle]) && Contains(k2, circle[iCircle]))
                    {
                        minRSqr = circle[iCircle].radius;
                        iMinRSqr = iCircle;
                    }
                }
            }

            // Permutations of type 3.
            for (int j = 0; j < numType3; ++j, ++iCircle)
            {
                k0 = mSupport[type3[j][0]];
                k1 = mSupport[type3[j][1]];
                circle[iCircle] = ExactCircle3(k0, k1, i);
                if (circle[iCircle].radius < minRSqr)
                {
                    k2 = mSupport[type3[j][2]];
                    if (Contains(k2, circle[iCircle]))
                    {
                        minRSqr = circle[iCircle].radius;
                        iMinRSqr = iCircle;
                    }
                }
            }

            switch (iMinRSqr)
            {
            case 0:
                mNumSupport = 2;
                mSupport[1] = i;
                break;
            case 1:
                mNumSupport = 2;
                mSupport[0] = i;
                break;
            case 2:
                mNumSupport = 2;
                mSupport[0] = mSupport[2];
                mSupport[1] = i;
                break;
            case 3:
                mSupport[2] = i;
                break;
            case 4:
                mSupport[1] = i;
                break;
            case 5:
                mSupport[0] = i;
                break;
            case -1:
                // For exact arithmetic, iMinRSqr >= 0, but for floating-point
                // arithmetic, round-off errors can lead to iMinRSqr == -1.
                // When this happens, use a simple bounding circle for the
                // result and terminate the minimum-area algorithm.
                return std::make_pair(Circle2<ComputeType>(), false);
            }

            return std::make_pair(circle[iMinRSqr], true);
        }

        // Indices of points that support the current minimum area circle.
        bool SupportContains(int j) const
        {
            for (int i = 0; i < mNumSupport; ++i)
            {
                if (j == mSupport[i])
                {
                    return true;
                }
            }
            return false;
        }

        int mNumSupport;
        std::array<int, 3> mSupport;

        // Random permutation of the unique input points to produce expected
        // linear time for the algorithm.
        std::default_random_engine mDRE;
        std::vector<Vector2<ComputeType>> mComputePoints;
    };
}
