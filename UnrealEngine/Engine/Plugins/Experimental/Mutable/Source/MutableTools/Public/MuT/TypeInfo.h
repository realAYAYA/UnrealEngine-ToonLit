// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../Private/MuR/ParametersPrivate.h"
#include "HAL/PlatformMath.h"
#include "MuR/Image.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/Parameters.h"

// This header file provides additional information about data types defined in the runtime, that
// can be useful only for the tools. For instance, it provides strings for some enumeration values.

namespace mu
{

	class MUTABLETOOLS_API TypeInfo
	{
	public:

		static const char* s_imageFormatName[size_t(EImageFormat::IF_COUNT)];

		static const char* s_meshBufferSemanticName[ MBS_COUNT ];

		static const char* s_meshBufferFormatName[ MBF_COUNT ];

		static const char* s_projectorTypeName [ static_cast<uint32>(mu::PROJECTOR_TYPE::COUNT) ];

		static const char* s_curveInterpolationModeName [ static_cast<uint8_t>(mu::CurveKeyFrame::InterpMode::Count) ];

		static const char* s_curveTangentModeName [ static_cast<uint8_t> (mu::CurveKeyFrame::TangentMode::Count) ];

		static const char* s_curveTangentWeightModeName [ static_cast<uint8_t> (mu::CurveKeyFrame::TangentWeightMode::Count) ];
	};

}
