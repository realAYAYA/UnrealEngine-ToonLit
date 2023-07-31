// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/05/22)

#include <ThirdParty/GTEngine/Mathematics/GteTetrahedronKey.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
#include <algorithm>

namespace
{
    void Permute(int u0, int u1, int u2, int V[4])
    {
        // Once V[0] is determined, create a permutation (V[1],V[2],V[3]) so
        // that (V[0],V[1],V[2],V[3]) is a positive permutation of
        // (v0,v1,v2,v3).
        if (u0 < u1)
        {
            if (u0 < u2)
            {
                // u0 is minimum
                V[1] = u0;
                V[2] = u1;
                V[3] = u2;
            }
            else
            {
                // u2 is minimum
                V[1] = u2;
                V[2] = u0;
                V[3] = u1;
            }
        }
        else
        {
            if (u1 < u2)
            {
                // u1 is minimum
                V[1] = u1;
                V[2] = u2;
                V[3] = u0;
            }
            else
            {
                // u2 is minimum
                V[1] = u2;
                V[2] = u0;
                V[3] = u1;
            }
        }
    }
}

namespace gte
{
    // TetrahedronKey<true>

    template<> std::array<int, 3> const TetrahedronKey<true>::oppositeFace[4] =
    {
		{{ 1, 2, 3 }},
		{{ 0, 3, 2 }},
		{{ 0, 1, 3 }},
		{{ 0, 2, 1 }}
    };

    template<>
    TetrahedronKey<true>::TetrahedronKey()
    {
        V[0] = -1;
        V[1] = -1;
        V[2] = -1;
        V[3] = -1;
    }

    template<>
    TetrahedronKey<true>::TetrahedronKey(int v0, int v1, int v2, int v3)
    {
        int imin = 0;
        V[0] = v0;
        if (v1 < V[0])
        {
            V[0] = v1;
            imin = 1;
        }
        if (v2 < V[0])
        {
            V[0] = v2;
            imin = 2;
        }
        if (v3 < V[0])
        {
            V[0] = v3;
            imin = 3;
        }

        if (imin == 0)
        {
            Permute(v1, v2, v3, V);
        }
        else if (imin == 1)
        {
            Permute(v0, v3, v2, V);
        }
        else if (imin == 2)
        {
            Permute(v0, v1, v3, V);
        }
        else  // imin == 3
        {
            Permute(v0, v2, v1, V);
        }
    }


    // TetrahedronKey<false>

    template<> std::array<int, 3> const TetrahedronKey<false>::oppositeFace[4] =
    {
		{{ 1, 2, 3 }},
		{{ 0, 3, 2 }},
		{{ 0, 1, 3 }},
		{{ 0, 2, 1 }}
    };

    template<>
    TetrahedronKey<false>::TetrahedronKey()
    {
        V[0] = -1;
        V[1] = -1;
        V[2] = -1;
        V[3] = -1;
    }

    template<>
    TetrahedronKey<false>::TetrahedronKey(int v0, int v1, int v2, int v3)
    {
        V[0] = v0;
        V[1] = v1;
        V[2] = v2;
        V[3] = v3;
        std::sort(&V[0], &V[4]);
    }
}
