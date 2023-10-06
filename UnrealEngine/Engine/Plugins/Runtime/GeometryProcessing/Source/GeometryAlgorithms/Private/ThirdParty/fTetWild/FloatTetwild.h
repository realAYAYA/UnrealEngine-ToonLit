// This file is part of fTetWild, a software for generating tetrahedral meshes.
//
// Copyright (C) 2019 Yixin Hu <yixin.hu@nyu.edu>
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
//

#pragma once

#include "ThirdParty/fTetWild/Parameters.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/IntVector.h"

#include "Util/ProgressCancel.h"

namespace floatTetWild {

bool tetrahedralization(
	std::vector<Vector3>&  input_vertices,
	std::vector<Vector3i>& input_faces,
	std::vector<int>&      input_tags,
    Parameters&            params,
	bool                   bInvertOutputTets,
    TArray<FVector>&       OutVertices,
    TArray<FIntVector4>&   OutTets,
	TArray<FVector>*       OutSurfaceVertices = nullptr,
	TArray<FIntVector3>*   OutSurfaceTris = nullptr,
    int                    boolean_op    = -1,
    bool                   skip_simplify = false,
	FProgressCancel*       Progress = nullptr);

}
