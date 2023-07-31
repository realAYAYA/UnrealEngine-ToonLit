// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/TypeInfo.h"

#include "MuR/ParametersPrivate.h"
#include "MuR/Types.h"

namespace mu
{

	const char* TypeInfo::s_imageFormatName[size_t(EImageFormat::IF_COUNT)] =
	{
		"None",
		"RGB U888",
		"RGBA U8888",
		"L U8",

		"PVRTC2",
		"PVRTC4",
		"ETC1",
		"ETC2",

		"L U8 RLE",
		"RGB U888 RLE",
		"RGBA U8888 RLE",
		"L U1 RLE",

		"BC1",
		"BC2",
		"BC3",
		"BC4",
		"BC5",
		"BC6",
		"BC7",

		"BGRA U8888",

		"ASTC_4x4_RGB_LDR",
		"ASTC_4x4_RGBA_LDR",
		"ASTC_4x4_RG_LDR",

	};


	const char* TypeInfo::s_meshBufferSemanticName[MBS_COUNT] =
	{
		"None",

		"VertexIndex",

		"Position",
		"Normal",
		"Tangent",
		"Binormal",
		"Tex Coords",
		"Colour",
		"Bone Weights",
		"Bone Indices",

		"Layout Block",

		"Chart",

		"Other",

		"Tangent Space Sign",

		"TriangleIndex",
		"BarycentricCoords",
		"Distance"

	};

	const char* TypeInfo::s_meshBufferFormatName[MBF_COUNT] =
	{
		"None",
		"Float16",
		"Float32",

		"UInt8",
		"UInt16",
		"UInt32",
		"Int8",
		"Int16",
		"Int32",

		"NUInt8",
		"NUInt16",
		"NUInt32",
		"NInt8",
		"NInt16",
		"NInt32",

		"PackedDir8",
		"PackedDir8_WTangentSign",
		"PackedDirS8",
		"PackedDirS8_WTangentSign",

		"Float64",
	};

	const char* TypeInfo::s_projectorTypeName[static_cast<uint32>(mu::PROJECTOR_TYPE::COUNT)] =
	{
		"Planar",
		"Cylindrical",
		"Wrapping",
	};

	const char* TypeInfo::s_curveInterpolationModeName[static_cast<uint8_t>(mu::CurveKeyFrame::InterpMode::Count)] =
	{
		"Linear",
		"Constant",
		"Cubic",
		"None",
	};

	const char* TypeInfo::s_curveTangentModeName[static_cast<uint8_t> (mu::CurveKeyFrame::TangentMode::Count)] =
	{
		"Auto",
		"Manual",
		"Broken",
		"None",
	};

	const char* TypeInfo::s_curveTangentWeightModeName[static_cast<uint8_t> (mu::CurveKeyFrame::TangentWeightMode::Count)] =
	{
		"Auto",
		"Manual",
		"Broken",
		"None",
	};

}