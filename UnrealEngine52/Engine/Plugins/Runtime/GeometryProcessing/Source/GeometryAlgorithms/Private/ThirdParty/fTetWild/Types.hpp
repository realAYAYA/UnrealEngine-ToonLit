// This file is part of fTetWild, a software for generating tetrahedral meshes.
//
// Copyright (C) 2019 Teseo Schneider <teseo.schneider@nyu.edu>
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
//

#pragma once

#include "HAL/Platform.h"
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 6011)
#pragma warning(disable : 6387)
#pragma warning(disable : 6313)
#pragma warning(disable : 6294)
#endif
THIRD_PARTY_INCLUDES_START
#include <Eigen/Dense>
THIRD_PARTY_INCLUDES_END
#if defined(_MSC_VER)
#pragma warning(pop)
#endif


namespace floatTetWild {
#ifdef FLOAT_TETWILD_USE_FLOAT
    typedef float Scalar;
#define SCALAR_ZERO 1e-6
#define SCALAR_ZERO_2 1e-12
#define SCALAR_ZERO_3 1e-18
#else
    typedef double Scalar;
#define SCALAR_ZERO 1e-8
#define SCALAR_ZERO_2 1e-16
#define SCALAR_ZERO_3 1e-24
#endif

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> MatrixXs;

    typedef Eigen::Matrix<Scalar, 3, 3> Matrix3;

    typedef Eigen::Matrix<Scalar, 3, 1> Vector3;
    typedef Eigen::Matrix<Scalar, 2, 1> Vector2;


    typedef Eigen::Matrix<int, 4, 1> Vector4i;
    typedef Eigen::Matrix<int, 3, 1> Vector3i;
    typedef Eigen::Matrix<int, 2, 1> Vector2i;
}
