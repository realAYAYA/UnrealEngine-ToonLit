// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>

namespace gte
{

template <int N, bool Ordered>
class FeatureKey
{
protected:
    // Abstract base class with V[] uninitialized.  The derived classes must
    // set the V[] values accordingly.
    //
    // An ordered feature key has V[0] = min(V[]) with (V[0],V[1],...,V[N-1]) a
    // permutation of N inputs with an even number of transpositions.
    //
    // An unordered feature key has V[0] < V[1] < ... < V[N-1].
    //
    // Note that the word 'order' is about the geometry of the feature, not
    // the comparison order for any sorting.
    FeatureKey();

public:
    bool operator<(FeatureKey const& key) const;
    bool operator==(FeatureKey const& key) const;

    int V[N];
};


template <int N, bool Ordered>
FeatureKey<N, Ordered>::FeatureKey()
{
}

template <int N, bool Ordered>
bool FeatureKey<N, Ordered>::operator<(FeatureKey const& key) const
{
    for (int i = N - 1; i >= 0; --i)
    {
        if (V[i] < key.V[i])
        {
            return true;
        }

        if (V[i] > key.V[i])
        {
            return false;
        }
    }
    return false;
}

template <int N, bool Ordered>
bool FeatureKey<N, Ordered>::operator==(FeatureKey const& key) const
{
    for (int i = 0; i < N; ++i)
    {
        if (V[i] != key.V[i])
        {
            return false;
        }
    }
    return true;
}


}
