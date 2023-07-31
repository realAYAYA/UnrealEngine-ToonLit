// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2017/07/26)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteVector.h>
#include <algorithm>
#include <limits>
#include <vector>

namespace gte
{

// Use a kd-tree for sorting used in a query for finding nearest neighbors
// of a point in a space of the specified dimension N.  The split order is
// always 0,1,2,...,N-1.  The number of sites at a leaf node is controlled
// by 'maxLeafSize' and the maximum level of the tree is controlled by
// 'maxLevels'.  The points are of type Vector<N,Real>.  The 'Site' is a
// structure of information that minimally implements the function
// 'Vector<N,Real> GetPosition () const'.  The Site template parameter
// allows the query to be applied even when it has more local information
// than just point location.
template <int N, typename Real, typename Site>
class NearestNeighborQuery
{
public:
    // Construction.
    NearestNeighborQuery(std::vector<Site> const& sites, int maxLeafSize, int maxLevel);

    // Member access.
    inline int GetMaxLeafSize () const;
    inline int GetMaxLevel () const;

    // Compute up to MaxNeighbors nearest neighbors within the specified
    // radius of the point.  The returned integer is the number of neighbors
    // found, possibly zero.  The neighbors array stores indices into the
    // array passed to the constructor.
    template <int MaxNeighbors>
    int FindNeighbors(Vector<N,Real> const& point, Real radius,
        std::array<int, MaxNeighbors>& neighbors) const;

private:
    typedef std::pair<Vector<N, Real>, int> SortedPoint;

    struct Node
    {
        Real split;
        int axis;
        int numSites;
        int siteOffset;
        int left;
        int right;
    };

    // Populate the node so that it contains the points split along the
    // coordinate axes.
    void Build(int numSites, int siteOffset, int nodeIndex, int level);

    int mMaxLeafSize;
    int mMaxLevel;
    std::vector<SortedPoint> mSortedPoints;
    std::vector<Node> mNodes;
};


template <int N, typename Real, typename Site>
NearestNeighborQuery<N, Real, Site>::NearestNeighborQuery(
    std::vector<Site> const& sites, int maxLeafSize, int maxLevel)
    :
    mMaxLeafSize(maxLeafSize),
    mMaxLevel(maxLevel),
    mSortedPoints(sites.size())
{
    int const numSites = static_cast<int>(sites.size());
    for (int i = 0; i < numSites; ++i)
    {
        mSortedPoints[i] = std::make_pair(sites[i].GetPosition(), i);
    }

    mNodes.push_back(Node());
    Build(numSites, 0, 0, 0);
}

template <int N, typename Real, typename Site>
inline int NearestNeighborQuery<N, Real, Site>::GetMaxLeafSize() const
{
    return mMaxLeafSize;
}

template <int N, typename Real, typename Site>
inline int NearestNeighborQuery<N, Real, Site>::GetMaxLevel() const
{
    return mMaxLevel;
}

template <int N, typename Real, typename Site>
template <int MaxNeighbors>
int NearestNeighborQuery<N, Real, Site>::FindNeighbors(Vector<N, Real> const& point,
    Real radius, std::array<int, MaxNeighbors>& neighbors) const
{
    Real sqrRadius = radius * radius;
    int numNeighbors = 0;
    std::array<int, MaxNeighbors + 1> localNeighbors;
    std::array<Real, MaxNeighbors + 1> neighborSqrLength;
    for (int i = 0; i <= MaxNeighbors; ++i)
    {
        localNeighbors[i] = -1;
        neighborSqrLength[i] = std::numeric_limits<Real>::max();
    }

    // The kd-tree construction is recursive, simulated here by using a stack.
    // The maximum depth is limited to 32, because the number of sites is
    // limited to 2^{32} (the number of 32-bit integer indices).
    std::array<int, 32> stack;
    int top = 0;
    stack[0] = 0;

    while (top >= 0)
    {
        Node node = mNodes[stack[top--]];

        if (node.siteOffset != -1)
        {
            for (int i = 0, j = node.siteOffset; i < node.numSites; ++i, ++j)
            {
                Vector<N, Real> diff = mSortedPoints[j].first - point;
                Real sqrLength = Dot(diff, diff);
                if (sqrLength <= sqrRadius)
                {
                    // Maintain the nearest neighbors.
                    int k;
                    for (k = 0; k < numNeighbors; ++k)
                    {
                        if (sqrLength <= neighborSqrLength[k])
                        {
                            for (int n = numNeighbors; n > k; --n)
                            {
                                localNeighbors[n] = localNeighbors[n - 1];
                                neighborSqrLength[n] = neighborSqrLength[n - 1];
                            }
                            break;
                        }
                    }
                    if (k < MaxNeighbors)
                    {
                        localNeighbors[k] = mSortedPoints[j].second;
                        neighborSqrLength[k] = sqrLength;
                    }
                    if (numNeighbors < MaxNeighbors)
                    {
                        ++numNeighbors;
                    }
                }
            }
        }

        if (node.left != -1 && point[node.axis] - radius <= node.split)
        {
            stack[++top] = node.left;
        }

        if (node.right != -1 && point[node.axis] + radius >= node.split)
        {
            stack[++top] = node.right;
        }
    }

    for (int i = 0; i < numNeighbors; ++i)
    {
        neighbors[i] = localNeighbors[i];
    }

    return numNeighbors;
}

template <int N, typename Real, typename Site>
void NearestNeighborQuery<N, Real, Site>::Build(int numSites, int siteOffset,
    int nodeIndex, int level)
{
    LogAssert(siteOffset != -1, "Invalid site offset.");
    LogAssert(nodeIndex != -1, "Invalid node index.");
    LogAssert(numSites > 0, "Empty point list.");

    Node& node = mNodes[nodeIndex];
    node.numSites = numSites;

    if (numSites > mMaxLeafSize && level <= mMaxLevel)
    {
        int halfNumSites = numSites / 2;

        // The point set is too large for a leaf node, so split it at the
        // median.  The O(m log m) sort is not needed; rather, we locate the
        // median using an order statistic construction that is expected
        // time O(m).
        int const axis = level % N;
        auto sorter = [axis](SortedPoint const& p0, SortedPoint const& p1)
        {
            return p0.first[axis] < p1.first[axis];
        };

        auto begin = mSortedPoints.begin() + siteOffset;
        auto mid = mSortedPoints.begin() + siteOffset + halfNumSites;
        auto end = mSortedPoints.begin() + siteOffset + numSites;
        std::nth_element(begin, mid, end, sorter);

        // Get the median position.
        node.split = mSortedPoints[siteOffset + halfNumSites].first[axis];
        node.axis = axis;
        node.siteOffset = -1;

        // Apply a divide-and-conquer step.
        int left = (int)mNodes.size(), right = left + 1;
        node.left = left;
        node.right = right;
        mNodes.push_back(Node());
        mNodes.push_back(Node());

        int nextLevel = level + 1;
        Build(halfNumSites, siteOffset, left, nextLevel);
        Build(numSites - halfNumSites, siteOffset + halfNumSites, right, nextLevel);
    }
    else
    {
        // The number of points is small enough, so make this node a leaf.
        node.split = std::numeric_limits<Real>::max();
        node.axis = -1;
        node.siteOffset = siteOffset;
        node.left = -1;
        node.right = -1;
    }
}

}
