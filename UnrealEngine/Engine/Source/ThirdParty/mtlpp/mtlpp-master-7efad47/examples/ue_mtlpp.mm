// Copyright Epic Games, Inc. All Rights Reserved.

#include "../src/mtlpp.hpp"
#include <stdio.h>

bool CompileAndLink(ns::String const& sourcePath, ns::String const& tmpOutput, ns::String const& LibOutput, mtlpp::CompilerOptions const& options)
{
	ns::AutoReleasedError AutoReleasedError;
	bool bOK = mtlpp::Compiler::Compile(sourcePath, tmpOutput, options, &AutoReleasedError);
	
	if (AutoReleasedError.GetPtr())
	{
		NSLog(@"%@: %@ %d %@ %@", bOK ? @"Warning" : @"AutoReleasedError", AutoReleasedError.GetDomain().GetPtr(), AutoReleasedError.GetCode(), AutoReleasedError.GetLocalizedDescription().GetPtr(), AutoReleasedError.GetLocalizedFailureReason().GetPtr());
	}
	
	if (bOK)
	{
		ns::Array<ns::String> Array = @[ tmpOutput.GetPtr() ];
		bOK = mtlpp::Compiler::Link(Array, LibOutput, options, &AutoReleasedError);
		
		if (AutoReleasedError.GetPtr())
		{
			NSLog(@"%@: %@ %d %@ %@", bOK ? @"Warning" : @"AutoReleasedError", AutoReleasedError.GetDomain().GetPtr(), AutoReleasedError.GetCode(), AutoReleasedError.GetLocalizedDescription().GetPtr(), AutoReleasedError.GetLocalizedFailureReason().GetPtr());
		}
	}
	
	return bOK;
}

enum ParseEntries
{
	Descriptor,
	Label,
	AlphaToCover,
	AlphaToOne,
	RasterOn,
	SampleCover,
	SampleMask,
	SampleCount,
	Topology,
	DepthFormat,
	StencilFormat,
	TessPartition,
	TessFactor,
	TessScaleOn,
	TessFactorFormat,
	TessControlPoint,
	TessStepFunc,
	TessWindOrder,
	Vertex,
	Fragment,
	ShaderName,
	VertexArray,
	BufferTemplate,
	StepFunc,
	Stride,
	AttributeTemplate,
	Offset,
	Format,
	ColorAttachments,
	ColorAttachTemplate,
	PixelFormat,
	Blending,
	SourceFactors,
	DestFactors,
	BlendOps,
	RGB,
	Alpha,
	WriteMask,
	ComputeFunction,
	StageInputDescriptor,
	IndexType,
	IndexBufferIndex,
	MAX
};

template<typename t>
bool Parse(char const* str, char const** tags, unsigned max, t& output)
{
	for (unsigned i = 0; i < max; i++)
	{
		if (strcmp(tags[i], str) == 0)
		{
			output = (t)i;
			return true;
		}
	}
	assert(false);
	return false;
}

template<typename t>
bool Parse(char const* str, char const** tags, t const* vals, unsigned max, t& output)
{
	for (unsigned i = 0; i < max; i++)
	{
		if (strcmp(tags[i], str) == 0)
		{
			output = vals[i];
			return true;
		}
	}
	assert(false);
	return false;
}

bool Parse(char const* str, mtlpp::PrimitiveTopologyClass& val)
{
	static char const* ParseTags[] =
	{
		"Unspecified",
		"Point",
		"Line",
		"Triangle",
	};
	return Parse(str, ParseTags, 4, val);
}
bool Parse(char const* str, mtlpp::PixelFormat& val)
{
	static char const* ParseTags[] =
	{
		"MTLPixelFormatInvalid",
		"MTLPixelFormatA8Unorm",
		"MTLPixelFormatR8Unorm",
		"MTLPixelFormatR8Unorm_sRGB",
		"MTLPixelFormatR8Snorm",
		"MTLPixelFormatR8Uint",
		"MTLPixelFormatR8Sint",
		"MTLPixelFormatR16Unorm",
		"MTLPixelFormatR16Snorm",
		"MTLPixelFormatR16Uint",
		"MTLPixelFormatR16Sint",
		"MTLPixelFormatR16Float",
		"MTLPixelFormatRG8Unorm",
		"MTLPixelFormatRG8Unorm_sRGB",
		"MTLPixelFormatRG8Snorm",
		"MTLPixelFormatRG8Uint",
		"MTLPixelFormatRG8Sint",
		"MTLPixelFormatB5G6R5Unorm",
		"MTLPixelFormatA1BGR5Unorm",
		"MTLPixelFormatABGR4Unorm",
		"MTLPixelFormatBGR5A1Unorm",
		"MTLPixelFormatR32Uint",
		"MTLPixelFormatR32Sint",
		"MTLPixelFormatR32Float",
		"MTLPixelFormatRG16Unorm",
		"MTLPixelFormatRG16Snorm",
		"MTLPixelFormatRG16Uint",
		"MTLPixelFormatRG16Sint",
		"MTLPixelFormatRG16Float",
		"MTLPixelFormatRGBA8Unorm",
		"MTLPixelFormatRGBA8Unorm_sRGB",
		"MTLPixelFormatRGBA8Snorm",
		"MTLPixelFormatRGBA8Uint",
		"MTLPixelFormatRGBA8Sint",
		"MTLPixelFormatBGRA8Unorm",
		"MTLPixelFormatBGRA8Unorm_sRGB",
		"MTLPixelFormatRGB10A2Unorm",
		"MTLPixelFormatRGB10A2Uint",
		"MTLPixelFormatRG11B10Float",
		"MTLPixelFormatRGB9E5Float",
		"MTLPixelFormatBGR10A2Unorm",
		"MTLPixelFormatBGR10_XR",
		"MTLPixelFormatBGR10_XR_sRGB",
		"MTLPixelFormatRG32Uint",
		"MTLPixelFormatRG32Sint",
		"MTLPixelFormatRG32Float",
		"MTLPixelFormatRGBA16Unorm",
		"MTLPixelFormatRGBA16Snorm",
		"MTLPixelFormatRGBA16Uint",
		"MTLPixelFormatRGBA16Sint",
		"MTLPixelFormatRGBA16Float",
		"MTLPixelFormatBGRA10_XR",
		"MTLPixelFormatBGRA10_XR_sRGB",
		"MTLPixelFormatRGBA32Uint",
		"MTLPixelFormatRGBA32Sint",
		"MTLPixelFormatRGBA32Float",
		"MTLPixelFormatBC1_RGBA",
		"MTLPixelFormatBC1_RGBA_sRGB",
		"MTLPixelFormatBC2_RGBA",
		"MTLPixelFormatBC2_RGBA_sRGB",
		"MTLPixelFormatBC3_RGBA",
		"MTLPixelFormatBC3_RGBA_sRGB",
		"MTLPixelFormatBC4_RUnorm",
		"MTLPixelFormatBC4_RSnorm",
		"MTLPixelFormatBC5_RGUnorm",
		"MTLPixelFormatBC5_RGSnorm",
		"MTLPixelFormatBC6H_RGBFloat",
		"MTLPixelFormatBC6H_RGBUfloat",
		"MTLPixelFormatBC7_RGBAUnorm",
		"MTLPixelFormatBC7_RGBAUnorm_sRGB",
		"MTLPixelFormatPVRTC_RGB_2BPP",
		"MTLPixelFormatPVRTC_RGB_2BPP_sRGB",
		"MTLPixelFormatPVRTC_RGB_4BPP",
		"MTLPixelFormatPVRTC_RGB_4BPP_sRGB",
		"MTLPixelFormatPVRTC_RGBA_2BPP",
		"MTLPixelFormatPVRTC_RGBA_2BPP_sRGB",
		"MTLPixelFormatPVRTC_RGBA_4BPP",
		"MTLPixelFormatPVRTC_RGBA_4BPP_sRGB",
		"MTLPixelFormatEAC_R11Unorm",
		"MTLPixelFormatEAC_R11Snorm",
		"MTLPixelFormatEAC_RG11Unorm",
		"MTLPixelFormatEAC_RG11Snorm",
		"MTLPixelFormatEAC_RGBA8",
		"MTLPixelFormatEAC_RGBA8_sRGB",
		"MTLPixelFormatETC2_RGB8",
		"MTLPixelFormatETC2_RGB8_sRGB",
		"MTLPixelFormatETC2_RGB8A1",
		"MTLPixelFormatETC2_RGB8A1_sRGB",
		"MTLPixelFormatASTC_4x4_sRGB",
		"MTLPixelFormatASTC_5x4_sRGB",
		"MTLPixelFormatASTC_5x5_sRGB",
		"MTLPixelFormatASTC_6x5_sRGB",
		"MTLPixelFormatASTC_6x6_sRGB",
		"MTLPixelFormatASTC_8x5_sRGB",
		"MTLPixelFormatASTC_8x6_sRGB",
		"MTLPixelFormatASTC_8x8_sRGB",
		"MTLPixelFormatASTC_10x5_sRGB",
		"MTLPixelFormatASTC_10x6_sRGB",
		"MTLPixelFormatASTC_10x8_sRGB",
		"MTLPixelFormatASTC_10x10_sRGB",
		"MTLPixelFormatASTC_12x10_sRGB",
		"MTLPixelFormatASTC_12x12_sRGB",
		"MTLPixelFormatASTC_4x4_LDR",
		"MTLPixelFormatASTC_5x4_LDR",
		"MTLPixelFormatASTC_5x5_LDR",
		"MTLPixelFormatASTC_6x5_LDR",
		"MTLPixelFormatASTC_6x6_LDR",
		"MTLPixelFormatASTC_8x5_LDR",
		"MTLPixelFormatASTC_8x6_LDR",
		"MTLPixelFormatASTC_8x8_LDR",
		"MTLPixelFormatASTC_10x5_LDR",
		"MTLPixelFormatASTC_10x6_LDR",
		"MTLPixelFormatASTC_10x8_LDR",
		"MTLPixelFormatASTC_10x10_LDR",
		"MTLPixelFormatASTC_12x10_LDR",
		"MTLPixelFormatASTC_12x12_LDR",
		"MTLPixelFormatASTC_4x4_HDR",
		"MTLPixelFormatASTC_5x4_HDR",
		"MTLPixelFormatASTC_5x5_HDR",
		"MTLPixelFormatASTC_6x5_HDR",
		"MTLPixelFormatASTC_6x6_HDR",
		"MTLPixelFormatASTC_8x5_HDR",
		"MTLPixelFormatASTC_8x6_HDR",
		"MTLPixelFormatASTC_8x8_HDR",
		"MTLPixelFormatASTC_10x5_HDR",
		"MTLPixelFormatASTC_10x6_HDR",
		"MTLPixelFormatASTC_10x8_HDR",
		"MTLPixelFormatASTC_10x10_HDR",
		"MTLPixelFormatASTC_12x10_HDR",
		"MTLPixelFormatASTC_12x12_HDR",
		"MTLPixelFormatGBGR422",
		"MTLPixelFormatBGRG422",
		"MTLPixelFormatDepth16Unorm",
		"MTLPixelFormatDepth32Float",
		"MTLPixelFormatStencil8",
		"MTLPixelFormatDepth24Unorm_Stencil8",
		"MTLPixelFormatDepth32Float_Stencil8",
		"MTLPixelFormatX32_Stencil8",
		"MTLPixelFormatX24_Stencil8",
	};
	static mtlpp::PixelFormat ParseVals[] =
	{
		mtlpp::PixelFormat::Invalid,
		
		mtlpp::PixelFormat::A8Unorm,
		
		mtlpp::PixelFormat::R8Unorm                            ,
		mtlpp::PixelFormat::R8Unorm_sRGB,
		
		mtlpp::PixelFormat::R8Snorm      ,
		mtlpp::PixelFormat::R8Uint       ,
		mtlpp::PixelFormat::R8Sint       ,
		
		mtlpp::PixelFormat::R16Unorm     ,
		mtlpp::PixelFormat::R16Snorm     ,
		mtlpp::PixelFormat::R16Uint      ,
		mtlpp::PixelFormat::R16Sint      ,
		mtlpp::PixelFormat::R16Float     ,
		
		mtlpp::PixelFormat::RG8Unorm      ,
		mtlpp::PixelFormat::RG8Unorm_sRGB ,
		mtlpp::PixelFormat::RG8Snorm      ,
		mtlpp::PixelFormat::RG8Uint       ,
		mtlpp::PixelFormat::RG8Sint       ,
		
		mtlpp::PixelFormat::B5G6R5Unorm ,
		mtlpp::PixelFormat::A1BGR5Unorm ,
		mtlpp::PixelFormat::ABGR4Unorm  ,
		mtlpp::PixelFormat::BGR5A1Unorm ,
		
		mtlpp::PixelFormat::R32Uint  ,
		mtlpp::PixelFormat::R32Sint  ,
		mtlpp::PixelFormat::R32Float ,
		
		mtlpp::PixelFormat::RG16Unorm  ,
		mtlpp::PixelFormat::RG16Snorm  ,
		mtlpp::PixelFormat::RG16Uint   ,
		mtlpp::PixelFormat::RG16Sint   ,
		mtlpp::PixelFormat::RG16Float  ,
		
		mtlpp::PixelFormat::RGBA8Unorm      ,
		mtlpp::PixelFormat::RGBA8Unorm_sRGB ,
		mtlpp::PixelFormat::RGBA8Snorm      ,
		mtlpp::PixelFormat::RGBA8Uint       ,
		mtlpp::PixelFormat::RGBA8Sint       ,
		
		mtlpp::PixelFormat::BGRA8Unorm      ,
		mtlpp::PixelFormat::BGRA8Unorm_sRGB ,
		
		mtlpp::PixelFormat::RGB10A2Unorm ,
		mtlpp::PixelFormat::RGB10A2Uint  ,
		
		mtlpp::PixelFormat::RG11B10Float ,
		mtlpp::PixelFormat::RGB9E5Float ,
		
		mtlpp::PixelFormat::BGR10A2Unorm  ,
		
		mtlpp::PixelFormat::BGR10_XR      ,
		mtlpp::PixelFormat::BGR10_XR_sRGB ,
		
		mtlpp::PixelFormat::RG32Uint  ,
		mtlpp::PixelFormat::RG32Sint  ,
		mtlpp::PixelFormat::RG32Float ,
		
		mtlpp::PixelFormat::RGBA16Unorm  ,
		mtlpp::PixelFormat::RGBA16Snorm  ,
		mtlpp::PixelFormat::RGBA16Uint   ,
		mtlpp::PixelFormat::RGBA16Sint   ,
		mtlpp::PixelFormat::RGBA16Float  ,
		
		mtlpp::PixelFormat::BGRA10_XR      ,
		mtlpp::PixelFormat::BGRA10_XR_sRGB ,
		
		mtlpp::PixelFormat::RGBA32Uint  ,
		mtlpp::PixelFormat::RGBA32Sint  ,
		mtlpp::PixelFormat::RGBA32Float ,
		
		mtlpp::PixelFormat::BC1_RGBA              ,
		mtlpp::PixelFormat::BC1_RGBA_sRGB         ,
		mtlpp::PixelFormat::BC2_RGBA              ,
		mtlpp::PixelFormat::BC2_RGBA_sRGB         ,
		mtlpp::PixelFormat::BC3_RGBA              ,
		mtlpp::PixelFormat::BC3_RGBA_sRGB         ,
		
		
		mtlpp::PixelFormat::BC4_RUnorm            ,
		mtlpp::PixelFormat::BC4_RSnorm            ,
		mtlpp::PixelFormat::BC5_RGUnorm           ,
		mtlpp::PixelFormat::BC5_RGSnorm           ,
		
		
		mtlpp::PixelFormat::BC6H_RGBFloat         ,
		mtlpp::PixelFormat::BC6H_RGBUfloat        ,
		mtlpp::PixelFormat::BC7_RGBAUnorm         ,
		mtlpp::PixelFormat::BC7_RGBAUnorm_sRGB    ,
		
		
		mtlpp::PixelFormat::PVRTC_RGB_2BPP        ,
		mtlpp::PixelFormat::PVRTC_RGB_2BPP_sRGB   ,
		mtlpp::PixelFormat::PVRTC_RGB_4BPP        ,
		mtlpp::PixelFormat::PVRTC_RGB_4BPP_sRGB   ,
		mtlpp::PixelFormat::PVRTC_RGBA_2BPP       ,
		mtlpp::PixelFormat::PVRTC_RGBA_2BPP_sRGB  ,
		mtlpp::PixelFormat::PVRTC_RGBA_4BPP       ,
		mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB  ,
		
		
		mtlpp::PixelFormat::EAC_R11Unorm          ,
		mtlpp::PixelFormat::EAC_R11Snorm          ,
		mtlpp::PixelFormat::EAC_RG11Unorm         ,
		mtlpp::PixelFormat::EAC_RG11Snorm         ,
		mtlpp::PixelFormat::EAC_RGBA8             ,
		mtlpp::PixelFormat::EAC_RGBA8_sRGB        ,
		
		mtlpp::PixelFormat::ETC2_RGB8             ,
		mtlpp::PixelFormat::ETC2_RGB8_sRGB        ,
		mtlpp::PixelFormat::ETC2_RGB8A1           ,
		mtlpp::PixelFormat::ETC2_RGB8A1_sRGB      ,
		
		
		mtlpp::PixelFormat::ASTC_4x4_sRGB         ,
		mtlpp::PixelFormat::ASTC_5x4_sRGB         ,
		mtlpp::PixelFormat::ASTC_5x5_sRGB         ,
		mtlpp::PixelFormat::ASTC_6x5_sRGB         ,
		mtlpp::PixelFormat::ASTC_6x6_sRGB         ,
		mtlpp::PixelFormat::ASTC_8x5_sRGB         ,
		mtlpp::PixelFormat::ASTC_8x6_sRGB         ,
		mtlpp::PixelFormat::ASTC_8x8_sRGB         ,
		mtlpp::PixelFormat::ASTC_10x5_sRGB        ,
		mtlpp::PixelFormat::ASTC_10x6_sRGB        ,
		mtlpp::PixelFormat::ASTC_10x8_sRGB        ,
		mtlpp::PixelFormat::ASTC_10x10_sRGB       ,
		mtlpp::PixelFormat::ASTC_12x10_sRGB       ,
		mtlpp::PixelFormat::ASTC_12x12_sRGB       ,
		
		mtlpp::PixelFormat::ASTC_4x4_LDR          ,
		mtlpp::PixelFormat::ASTC_5x4_LDR          ,
		mtlpp::PixelFormat::ASTC_5x5_LDR          ,
		mtlpp::PixelFormat::ASTC_6x5_LDR          ,
		mtlpp::PixelFormat::ASTC_6x6_LDR          ,
		mtlpp::PixelFormat::ASTC_8x5_LDR          ,
		mtlpp::PixelFormat::ASTC_8x6_LDR          ,
		mtlpp::PixelFormat::ASTC_8x8_LDR          ,
		mtlpp::PixelFormat::ASTC_10x5_LDR         ,
		mtlpp::PixelFormat::ASTC_10x6_LDR         ,
		mtlpp::PixelFormat::ASTC_10x8_LDR         ,
		mtlpp::PixelFormat::ASTC_10x10_LDR        ,
		mtlpp::PixelFormat::ASTC_12x10_LDR        ,
		mtlpp::PixelFormat::ASTC_12x12_LDR        ,
		
		mtlpp::PixelFormat::ASTC_4x4_HDR          ,
		mtlpp::PixelFormat::ASTC_5x4_HDR          ,
		mtlpp::PixelFormat::ASTC_5x5_HDR          ,
		mtlpp::PixelFormat::ASTC_6x5_HDR          ,
		mtlpp::PixelFormat::ASTC_6x6_HDR          ,
		mtlpp::PixelFormat::ASTC_8x5_HDR          ,
		mtlpp::PixelFormat::ASTC_8x6_HDR          ,
		mtlpp::PixelFormat::ASTC_8x8_HDR          ,
		mtlpp::PixelFormat::ASTC_10x5_HDR         ,
		mtlpp::PixelFormat::ASTC_10x6_HDR         ,
		mtlpp::PixelFormat::ASTC_10x8_HDR         ,
		mtlpp::PixelFormat::ASTC_10x10_HDR        ,
		mtlpp::PixelFormat::ASTC_12x10_HDR        ,
		mtlpp::PixelFormat::ASTC_12x12_HDR        ,
		
		mtlpp::PixelFormat::GBGR422,
		
		mtlpp::PixelFormat::BGRG422,
		
		mtlpp::PixelFormat::Depth16Unorm,
		mtlpp::PixelFormat::Depth32Float,
		mtlpp::PixelFormat::Stencil8,
		mtlpp::PixelFormat::Depth24Unorm_Stencil8,
		mtlpp::PixelFormat::Depth32Float_Stencil8,
		
		mtlpp::PixelFormat::X32_Stencil8,
		mtlpp::PixelFormat::X24_Stencil8,
	};
	return Parse(str, ParseTags, ParseVals, 125, val);
}

bool Parse(char const* str, mtlpp::TessellationPartitionMode& val)
{
	static char const* ParseTags[] =
	{
		"MTLTessellationPartitionModePow2",
		"MTLTessellationPartitionModeInteger",
		"MTLTessellationPartitionModeFractionalOdd",
		"MTLTessellationPartitionModeFractionalEven",
	};
	return Parse(str, ParseTags, 4, val);
}

bool Parse(char const* str, mtlpp::TessellationFactorStepFunction& val)
{
	static char const* ParseTags[] =
	{
		"MTLTessellationFactorStepFunctionConstant",
		"MTLTessellationFactorStepFunctionPerPatch",
		"MTLTessellationFactorStepFunctionPerInstance",
		"MTLTessellationFactorStepFunctionPerPatchAndPerInstance",
	};
	return Parse(str, ParseTags, 4, val);
}

bool Parse(char const* str, mtlpp::TessellationFactorFormat& val)
{
	static char const* ParseTags[] =
	{
		"MTLTessellationFactorFormatHalf",
	};
	return Parse(str, ParseTags, 1, val);
}

bool Parse(char const* str, mtlpp::TessellationControlPointIndexType& val)
{
	static char const* ParseTags[] =
	{
		"MTLTessellationControlPointIndexTypeNone",
		"MTLTessellationControlPointIndexTypeUInt16",
		"MTLTessellationControlPointIndexTypeUInt32",
	};
	return Parse(str, ParseTags, 3, val);
}

bool Parse(char const* str, mtlpp::Winding& val)
{
	static char const* ParseTags[] =
	{
		"MTLWindingClockwise",
		"MTLWindingCounterClockwise",
	};
	return Parse(str, ParseTags, 2, val);
}

bool Parse(char const* str, mtlpp::VertexStepFunction& val)
{
	static char const* ParseTags[] =
	{
		"MTLVertexStepFunctionConstant",
		"MTLVertexStepFunctionPerVertex",
		"MTLVertexStepFunctionPerInstance",
		"MTLVertexStepFunctionPerPatch",
		"MTLVertexStepFunctionPerPatchControlPoint",
	};
	return Parse(str, ParseTags, 5, val);
}

bool Parse(char const* str, mtlpp::StepFunction& val)
{
	static char const* ParseTags[] =
	{
		"MTLStepFunctionConstant",
		
		// vertex functions only
		"MTLStepFunctionPerVertex",
		"MTLStepFunctionPerInstance",
		"MTLStepFunctionPerPatch",
		"MTLStepFunctionPerPatchControlPoint",
		
		// compute functions only
		"MTLStepFunctionThreadPositionInGridX",
		"MTLStepFunctionThreadPositionInGridY",
		"MTLStepFunctionThreadPositionInGridXIndexed",
		"MTLStepFunctionThreadPositionInGridYIndexed",
	};
	return Parse(str, ParseTags, 9, val);
}

bool Parse(char const* str, mtlpp::VertexFormat& val)
{
	static char const* ParseTags[] =
	{
		"MTLAttributeFormatInvalid",
	
		"MTLAttributeFormatUChar2",
		"MTLAttributeFormatUChar3",
		"MTLAttributeFormatUChar4",
		
		"MTLAttributeFormatChar2",
		"MTLAttributeFormatChar3",
		"MTLAttributeFormatChar4",
		
		"MTLAttributeFormatUChar2Normalized",
		"MTLAttributeFormatUChar3Normalized",
		"MTLAttributeFormatUChar4Normalized",
		
		"MTLAttributeFormatChar2Normalized",
		"MTLAttributeFormatChar3Normalized",
		"MTLAttributeFormatChar4Normalized",
		
		"MTLAttributeFormatUShort2",
		"MTLAttributeFormatUShort3",
		"MTLAttributeFormatUShort4",
		
		"MTLAttributeFormatShort2",
		"MTLAttributeFormatShort3",
		"MTLAttributeFormatShort4",
		
		"MTLAttributeFormatUShort2Normalized",
		"MTLAttributeFormatUShort3Normalized",
		"MTLAttributeFormatUShort4Normalized",
		
		"MTLAttributeFormatShort2Normalized",
		"MTLAttributeFormatShort3Normalized",
		"MTLAttributeFormatShort4Normalized",
		
		"MTLAttributeFormatHalf2",
		"MTLAttributeFormatHalf3",
		"MTLAttributeFormatHalf4",
		
		"MTLAttributeFormatFloat",
		"MTLAttributeFormatFloat2",
		"MTLAttributeFormatFloat3",
		"MTLAttributeFormatFloat4",
		
		"MTLAttributeFormatInt",
		"MTLAttributeFormatInt2",
		"MTLAttributeFormatInt3",
		"MTLAttributeFormatInt4",
		
		"MTLAttributeFormatUInt",
		"MTLAttributeFormatUInt2",
		"MTLAttributeFormatUInt3",
		"MTLAttributeFormatUInt4",
		
		"MTLAttributeFormatInt1010102Normalized",
		"MTLAttributeFormatUInt1010102Normalized",
		
		"MTLAttributeFormatUChar4Normalized_BGRA",
		
		"<UNDEFINED>",
		"<UNDEFINED>",
		
		"MTLAttributeFormatUChar",
		"MTLAttributeFormatChar",
		"MTLAttributeFormatUCharNormalized",
		"MTLAttributeFormatCharNormalized",
		
		"MTLAttributeFormatUShort",
		"MTLAttributeFormatShort",
		"MTLAttributeFormatUShortNormalized",
		"MTLAttributeFormatShortNormalized",
	
		"MTLAttributeFormatHalf",
	};
	return Parse(str, ParseTags, 54, val);
}

bool Parse(char const* str, mtlpp::AttributeFormat& val)
{
	static char const* ParseTags[] =
	{
		"MTLAttributeFormatInvalid",
		
		"MTLAttributeFormatUChar2",
		"MTLAttributeFormatUChar3",
		"MTLAttributeFormatUChar4",
		
		"MTLAttributeFormatChar2",
		"MTLAttributeFormatChar3",
		"MTLAttributeFormatChar4",
		
		"MTLAttributeFormatUChar2Normalized",
		"MTLAttributeFormatUChar3Normalized",
		"MTLAttributeFormatUChar4Normalized",
		
		"MTLAttributeFormatChar2Normalized",
		"MTLAttributeFormatChar3Normalized",
		"MTLAttributeFormatChar4Normalized",
		
		"MTLAttributeFormatUShort2",
		"MTLAttributeFormatUShort3",
		"MTLAttributeFormatUShort4",
		
		"MTLAttributeFormatShort2",
		"MTLAttributeFormatShort3",
		"MTLAttributeFormatShort4",
		
		"MTLAttributeFormatUShort2Normalized",
		"MTLAttributeFormatUShort3Normalized",
		"MTLAttributeFormatUShort4Normalized",
		
		"MTLAttributeFormatShort2Normalized",
		"MTLAttributeFormatShort3Normalized",
		"MTLAttributeFormatShort4Normalized",
		
		"MTLAttributeFormatHalf2",
		"MTLAttributeFormatHalf3",
		"MTLAttributeFormatHalf4",
		
		"MTLAttributeFormatFloat",
		"MTLAttributeFormatFloat2",
		"MTLAttributeFormatFloat3",
		"MTLAttributeFormatFloat4",
		
		"MTLAttributeFormatInt",
		"MTLAttributeFormatInt2",
		"MTLAttributeFormatInt3",
		"MTLAttributeFormatInt4",
		
		"MTLAttributeFormatUInt",
		"MTLAttributeFormatUInt2",
		"MTLAttributeFormatUInt3",
		"MTLAttributeFormatUInt4",
		
		"MTLAttributeFormatInt1010102Normalized",
		"MTLAttributeFormatUInt1010102Normalized",
		
		"MTLAttributeFormatUChar4Normalized_BGRA",
		
		"<UNDEFINED>",
		"<UNDEFINED>",
		
		"MTLAttributeFormatUChar",
		"MTLAttributeFormatChar",
		"MTLAttributeFormatUCharNormalized",
		"MTLAttributeFormatCharNormalized",
		
		"MTLAttributeFormatUShort",
		"MTLAttributeFormatShort",
		"MTLAttributeFormatUShortNormalized",
		"MTLAttributeFormatShortNormalized",
		
		"MTLAttributeFormatHalf",
	};
	return Parse(str, ParseTags, 54, val);
}

bool Parse(char const* str, mtlpp::BlendFactor& val)
{
	static char const* ParseTags[] =
	{
		"MTLBlendFactorZero",
		"MTLBlendFactorOne",
		"MTLBlendFactorSourceColor",
		"MTLBlendFactorOneMinusSourceColor",
		"MTLBlendFactorSourceAlpha",
		"MTLBlendFactorOneMinusSourceAlpha",
		"MTLBlendFactorDestinationColor",
		"MTLBlendFactorOneMinusDestinationColor",
		"MTLBlendFactorDestinationAlpha",
		"MTLBlendFactorOneMinusDestinationAlpha",
		"MTLBlendFactorSourceAlphaSaturated",
		"MTLBlendFactorBlendColor",
		"MTLBlendFactorOneMinusBlendColor",
		"MTLBlendFactorBlendAlpha",
		"MTLBlendFactorOneMinusBlendAlpha",
		"MTLBlendFactorSource1Color",
		"MTLBlendFactorOneMinusSource1Color",
		"MTLBlendFactorSource1Alpha",
		"MTLBlendFactorOneMinusSource1Alpha",
	};
	return Parse(str, ParseTags, 19, val);
}

bool Parse(char const* str, mtlpp::BlendOperation& val)
{
	static char const* ParseTags[] =
	{
		"MTLBlendOperationAdd",
		"MTLBlendOperationSubtract",
		"MTLBlendOperationReverseSubtract",
		"MTLBlendOperationMin",
		"MTLBlendOperationMax",
	};
	return Parse(str, ParseTags, 5, val);
}

bool Parse(char const* str, mtlpp::IndexType& val)
{
	static char const* ParseTags[] =
	{
		"MTLIndexTypeUInt16",
		"MTLIndexTypeUInt32",
	};
	return Parse(str, ParseTags, 2, val);
}

bool Parse(char const* str, mtlpp::ColorWriteMask& val)
{
	static char const* ParseTags[] =
	{
		"None",
		"Red",
		"Green",
		"Blue",
		"Alpha",
		"RB",
		"RG",
		"GB",
		"RGB",
		"RBA",
		"RGA",
		"GBA",
		"RGBA",
	};
	static mtlpp::ColorWriteMask ParseVals[] =
	{
		mtlpp::ColorWriteMask::None,
		mtlpp::ColorWriteMask::Red,
		mtlpp::ColorWriteMask::Green,
		mtlpp::ColorWriteMask::Blue,
		mtlpp::ColorWriteMask::Alpha,
		(mtlpp::ColorWriteMask)(mtlpp::ColorWriteMask::Red | mtlpp::ColorWriteMask::Blue),
		(mtlpp::ColorWriteMask)(mtlpp::ColorWriteMask::Red | mtlpp::ColorWriteMask::Green),
		(mtlpp::ColorWriteMask)(mtlpp::ColorWriteMask::Green | mtlpp::ColorWriteMask::Blue),
		(mtlpp::ColorWriteMask)(mtlpp::ColorWriteMask::Red | mtlpp::ColorWriteMask::Green | mtlpp::ColorWriteMask::Blue),
		(mtlpp::ColorWriteMask)(mtlpp::ColorWriteMask::Red | mtlpp::ColorWriteMask::Blue | mtlpp::ColorWriteMask::Alpha),
		(mtlpp::ColorWriteMask)(mtlpp::ColorWriteMask::Red | mtlpp::ColorWriteMask::Green | mtlpp::ColorWriteMask::Alpha),
		(mtlpp::ColorWriteMask)(mtlpp::ColorWriteMask::Green | mtlpp::ColorWriteMask::Blue | mtlpp::ColorWriteMask::Alpha),
		mtlpp::ColorWriteMask::All,
	};
	return Parse(str, ParseTags, ParseVals, 13, val);
}

static char const* ParseTags[ParseEntries::MAX] = {
	"Descriptor:",
	"label =",
	"Alpha to Coverage =",
	"Alpha to One =",
	"Rasterization Enabled =",
	"Sample Coverage =",
	"Sample Mask =",
	"Raster Sample Count =",
	"Input Primitive Topology =",
	"Depth Attachment Format =",
	"Stencil Attachment Format =",
	"tessellationPartitionMode =",
	"maxTessellationFactor =",
	"tessellationFactorScaleEnabled =",
	"tessellationFactorFormat =",
	"tessellationControlPointIndexType =",
	"tessellationFactorStepFunction =",
	"tessellationOutputWindingOrder =",
	"Vertex Function =",
	"Fragment Function =",
	"name =",
	"Vertex Array:",
	"Buffer ",
	"stepFunction",
	"stride",
	"Attribute ",
	"offset =",
	"format =",
	"Color Attachments:",
	"Color Attachment ",
	"pixelFormat =",
	"blending =",
	"Source blend factors:",
	"Destination blend factors:",
	"Blend operations:",
	"RGB =",
	"Alpha =",
	"writeMask =",
	"computeFunction =",
	"stageInputDescriptor = ",
	"IndexType: ",
	"IndexBufferIndex: ",
};

static char const* ParseFormats[ParseEntries::MAX] = {
	"Descriptor: %s",
	"label = %s",
	"Alpha to Coverage = %d",
	"Alpha to One = %d",
	"Rasterization Enabled = %d",
	"Sample Coverage = %d",
	"Sample Mask = %llu",
	"Raster Sample Count = %d",
	"Input Primitive Topology = %s",
	"Depth Attachment Format = %s",
	"Stencil Attachment Format = %s",
	"tessellationPartitionMode = %s",
	"maxTessellationFactor = %d",
	"tessellationFactorScaleEnabled = %d",
	"tessellationFactorFormat = %s",
	"tessellationControlPointIndexType = %s",
	"tessellationFactorStepFunction = %s",
	"tessellationOutputWindingOrder = %s",
	"Vertex Function = %s",
	"Fragment Function = %s",
	"name = %s",
	"Vertex Array:",
	"Buffer %d:",
	"stepFunction = %s",
	"stride = %d",
	"Attribute %d:",
	"offset = %d",
	"format = %s",
	"Color Attachments:",
	"Color Attachment %d:",
	"pixelFormat = %s",
	"blending = %d",
	"Source blend factors:",
	"Destination blend factors:",
	"Blend operations:",
	"RGB = %s",
	"Alpha = %s",
	"writeMask = %s",
	"computeFunction = %s",
	"stageInputDescriptor = ",
	"IndexType: %s",
	"IndexBufferIndex: %d",
};

#define SEARCH_AND_SCAN(search_string, tag, value) (search_string = strstr(search_string, ParseTags[tag])) && (sscanf(search_string, ParseFormats[tag], value) == 1)

bool ParseRenderPipelineDesc(char const* descriptor_path, mtlpp::Library const& VertexLib, mtlpp::Library const& FragmentLib, mtlpp::RenderPipelineDescriptor& descriptor)
{
	NSString* DescString = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:descriptor_path] encoding:NSUTF8StringEncoding error:nil];
	if (DescString)
	{
		char const* Desc = [DescString UTF8String];
		
		char Buffer[256];
		int IValue = 0;
		
		char const* search = Desc;
		char const* search_old = Desc;
		if (SEARCH_AND_SCAN(search, ParseEntries::Descriptor, Buffer))
		{
			if (SEARCH_AND_SCAN(search, ParseEntries::Label, Buffer))
			{
				descriptor.SetLabel([NSString stringWithUTF8String:Buffer]);
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::AlphaToCover, &IValue))
			{
				descriptor.SetAlphaToCoverageEnabled(IValue != 0);
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::AlphaToOne, &IValue))
			{
				descriptor.SetAlphaToOneEnabled(IValue != 0);
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::RasterOn, &IValue))
			{
				descriptor.SetRasterizationEnabled(IValue != 0);
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::SampleCount, &IValue))
			{
				descriptor.SetSampleCount((uint32_t)IValue);
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::Topology, Buffer))
			{
				mtlpp::PrimitiveTopologyClass val;
				if (Parse(Buffer, val))
				{
					descriptor.SetInputPrimitiveTopology(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::DepthFormat, Buffer))
			{
				mtlpp::PixelFormat val;
				if (Parse(Buffer, val))
				{
					descriptor.SetDepthAttachmentPixelFormat(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::StencilFormat, Buffer))
			{
				mtlpp::PixelFormat val;
				if (Parse(Buffer, val))
				{
					descriptor.SetStencilAttachmentPixelFormat(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::TessPartition, Buffer))
			{
				mtlpp::TessellationPartitionMode val;
				if (Parse(Buffer, val))
				{
					descriptor.SetTessellationPartitionMode(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::TessFactor, &IValue))
			{
				descriptor.SetMaxTessellationFactor((uint32_t)IValue);
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::TessScaleOn, &IValue))
			{
				descriptor.SetTessellationFactorScaleEnabled(IValue != 0);
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::TessFactorFormat, Buffer))
			{
				mtlpp::TessellationFactorFormat val;
				if (Parse(Buffer, val))
				{
					descriptor.SetTessellationFactorFormat(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::TessControlPoint, Buffer))
			{
				mtlpp::TessellationControlPointIndexType val;
				if (Parse(Buffer, val))
				{
					descriptor.SetTessellationControlPointIndexType(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::TessStepFunc, Buffer))
			{
				mtlpp::TessellationFactorStepFunction val;
				if (Parse(Buffer, val))
				{
					descriptor.SetTessellationFactorStepFunction(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if (SEARCH_AND_SCAN(search, ParseEntries::TessWindOrder, Buffer))
			{
				mtlpp::Winding val;
				if (Parse(Buffer, val))
				{
					descriptor.SetTessellationOutputWindingOrder(val);
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if ((search = strstr(search, ParseTags[ParseEntries::Vertex])))
			{
				search_old = search;
				if (SEARCH_AND_SCAN(search, ParseEntries::ShaderName, Buffer))
				{
					ns::String name = [NSString stringWithUTF8String:Buffer];
					mtlpp::Function func = VertexLib.NewFunction(name);
					descriptor.SetVertexFunction(func);
				}
				else
				{
					search = search_old;
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if ((search = strstr(search, ParseTags[ParseEntries::Fragment])))
			{
				search_old = search;
				if (SEARCH_AND_SCAN(search, ParseEntries::ShaderName, Buffer))
				{
					ns::String name = [NSString stringWithUTF8String:Buffer];
					mtlpp::Function func = FragmentLib.NewFunction(name);
					descriptor.SetFragmentFunction(func);
				}
				else
				{
					search = search_old;
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if ((search = strstr(search, ParseTags[ParseEntries::VertexArray])))
			{
				mtlpp::VertexDescriptor VertexDesc;
				
				char const* ColorAttachMarker = strstr(search, ParseTags[ParseEntries::ColorAttachTemplate]);
				char const* ColorAttachMarkerStr = ColorAttachMarker ? ParseTags[ParseEntries::ColorAttachTemplate] : ParseTags[ParseEntries::ColorAttachments];
				for(uint32_t i = 0; i < 31; i++)
				{
					// Buffers
					sprintf(Buffer, ParseFormats[ParseEntries::BufferTemplate], i);
					search_old = search;
					search = strstr(search, Buffer);
					if (!search || !strstr(search, ColorAttachMarkerStr))
					{
						search = search_old;
						continue;
					}
					
					mtlpp::VertexBufferLayoutDescriptor VBL = VertexDesc.GetLayouts()[i];
					
					search_old = search;
					if (SEARCH_AND_SCAN(search, ParseEntries::StepFunc, Buffer))
					{
						mtlpp::VertexStepFunction val;
						if (Parse(Buffer, val))
						{
							VBL.SetStepFunction(val);
							switch (val)
							{
								case mtlpp::VertexStepFunction::Constant:
									VBL.SetStepRate(0);
									break;
								default:
									VBL.SetStepRate(1);
									break;
							}
						}
					}
					else
					{
						search = search_old;
					}
					
					search_old = search;
					if (SEARCH_AND_SCAN(search, ParseEntries::Stride, &IValue))
					{
						VBL.SetStride((uint32_t)IValue);
					}
					else
					{
						search = search_old;
					}
					
					char const* NextBuffer = strstr(search, ParseTags[ParseEntries::BufferTemplate]);
					char const* Guard = NextBuffer;
					if (!Guard || (ColorAttachMarker < NextBuffer))
					{
						Guard = ColorAttachMarker;
					}
					
					// Attributes
					for (uint32_t a = 0; a < 31; a++)
					{
						sprintf(Buffer, ParseFormats[ParseEntries::AttributeTemplate], a);
						search_old = search;
						search = strstr(search, Buffer);
						if (!search || search >= Guard)
						{
							search = search_old;
							continue;
						}
						
						mtlpp::VertexAttributeDescriptor VADesc = VertexDesc.GetAttributes()[a];
						
						VADesc.SetBufferIndex(i);
						
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::Offset, &IValue))
						{
							VADesc.SetOffset(IValue);
						}
						else
						{
							search = search_old;
						}
						
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::Format, Buffer))
						{
							mtlpp::VertexFormat val;
							if (Parse(Buffer, val))
							{
								VADesc.SetFormat(val);
							}
						}
						else
						{
							search = search_old;
						}
					}
				}
				
				descriptor.SetVertexDescriptor(VertexDesc);
			}
			else
			{
				search = search_old;
			}
			
			if ((search = strstr(search, ParseTags[ParseEntries::ColorAttachments])))
			{
				for(uint32_t i = 0; i < 8; i++)
				{
					sprintf(Buffer, ParseFormats[ParseEntries::ColorAttachTemplate], i);
					search_old = search;
					search = strstr(search, Buffer);
					if (!search)
					{
						search = search_old;
						continue;
					}
					
					mtlpp::RenderPipelineColorAttachmentDescriptor ColorDesc = descriptor.GetColorAttachments()[i];
					
					search_old = search;
					if (SEARCH_AND_SCAN(search, ParseEntries::PixelFormat, Buffer))
					{
						mtlpp::PixelFormat val;
						if (Parse(Buffer, val))
						{
							ColorDesc.SetPixelFormat(val);
						}
					}
					else
					{
						search = search_old;
					}
					
					search_old = search;
					if (SEARCH_AND_SCAN(search, ParseEntries::Blending, &IValue))
					{
						ColorDesc.SetBlendingEnabled(IValue != 0);
					}
					else
					{
						search = search_old;
					}
					
					search_old = search;
					if ((search = strstr(search, ParseTags[ParseEntries::SourceFactors])))
					{
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::RGB, Buffer))
						{
							mtlpp::BlendFactor val;
							if (Parse(Buffer, val))
							{
								ColorDesc.SetSourceRgbBlendFactor(val);
							}
						}
						else
						{
							search = search_old;
						}
						
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::Alpha, Buffer))
						{
							mtlpp::BlendFactor val;
							if (Parse(Buffer, val))
							{
								ColorDesc.SetSourceAlphaBlendFactor(val);
							}
						}
						else
						{
							search = search_old;
						}
					}
					else
					{
						search = search_old;
					}
					
					search_old = search;
					if ((search = strstr(search, ParseTags[ParseEntries::DestFactors])))
					{
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::RGB, Buffer))
						{
							mtlpp::BlendFactor val;
							if (Parse(Buffer, val))
							{
								ColorDesc.SetDestinationRgbBlendFactor(val);
							}
						}
						else
						{
							search = search_old;
						}
						
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::Alpha, Buffer))
						{
							mtlpp::BlendFactor val;
							if (Parse(Buffer, val))
							{
								ColorDesc.SetDestinationAlphaBlendFactor(val);
							}
						}
						else
						{
							search = search_old;
						}
					}
					else
					{
						search = search_old;
					}
					
					search_old = search;
					if ((search = strstr(search, ParseTags[ParseEntries::BlendOps])))
					{
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::RGB, Buffer))
						{
							mtlpp::BlendOperation val;
							if (Parse(Buffer, val))
							{
								ColorDesc.SetRgbBlendOperation(val);
							}
						}
						else
						{
							search = search_old;
						}
						
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::Alpha, Buffer))
						{
							mtlpp::BlendOperation val;
							if (Parse(Buffer, val))
							{
								ColorDesc.SetAlphaBlendOperation(val);
							}
						}
						else
						{
							search = search_old;
						}
					}
					
					search_old = search;
					if (SEARCH_AND_SCAN(search, ParseEntries::WriteMask, Buffer))
					{
						mtlpp::ColorWriteMask val;
						if (Parse(Buffer, val))
						{
							ColorDesc.SetWriteMask(val);
						}
					}
					else
					{
						search = search_old;
					}
				 }
			}
		}
	}
	
	return true;
}

bool ParseComputePipelineDesc(const char* descriptor_path, mtlpp::Library const& ComputeLib, mtlpp::ComputePipelineDescriptor& descriptor)
{
	NSString* DescString = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:descriptor_path] encoding:NSUTF8StringEncoding error:nil];
	if (DescString)
	{
		char const* Desc = [DescString UTF8String];
		
		char Buffer[256];
		int IValue = 0;
		
		char const* search = Desc;
		char const* search_old = Desc;
		
		if (SEARCH_AND_SCAN(search, ParseEntries::Descriptor, Buffer))
		{
			if (SEARCH_AND_SCAN(search, ParseEntries::Label, Buffer))
			{
				descriptor.SetLabel([NSString stringWithUTF8String:Buffer]);
			}
			
			search_old = search;
			if ((search = strstr(search, ParseTags[ParseEntries::ComputeFunction])))
			{
				search_old = search;
				if (SEARCH_AND_SCAN(search, ParseEntries::ShaderName, Buffer))
				{
					ns::String name = [NSString stringWithUTF8String:Buffer];
					
					if (SEARCH_AND_SCAN(search, ParseEntries::IndexType, Buffer))
					{
						mtlpp::IndexType val;
						if (Parse(Buffer, val))
						{
							mtlpp::FunctionConstantValues Values;
							uint32_t IndexType = (uint32_t)val;
							Values.SetConstantValue(&IndexType, mtlpp::DataType::UInt, ns::String(@"indexBufferType"));
							
							ns::AutoReleasedError error;
							mtlpp::Function func = ComputeLib.NewFunction(name, Values, &error);
							if (error.GetPtr())
							{
								NSLog(@"ComputeLib.NewFunction Output: %@", error.GetPtr());
							}
							descriptor.SetComputeFunction(func);
						}
						else
						{
							mtlpp::Function func = ComputeLib.NewFunction(name);
							descriptor.SetComputeFunction(func);
						}
					}
					else
					{
						mtlpp::Function func = ComputeLib.NewFunction(name);
						descriptor.SetComputeFunction(func);
					}
					search = search_old;
				}
				else
				{
					search = search_old;
					
					if (SEARCH_AND_SCAN(search, ParseEntries::IndexType, Buffer))
					{
						mtlpp::IndexType val;
						if (Parse(Buffer, val))
						{
							mtlpp::FunctionConstantValues Values;
							uint32_t IndexType = (uint32_t)val;
							Values.SetConstantValue(&IndexType, mtlpp::DataType::UInt, ns::String(@"indexBufferType"));
							
							ns::AutoReleasedError error;
							mtlpp::Function func = ComputeLib.NewFunction(ComputeLib.GetFunctionNames()[0], Values, &error);
							if (error.GetPtr())
							{
								NSLog(@"ComputeLib.NewFunction Output: %@", error.GetPtr());
							}
							descriptor.SetComputeFunction(func);
						}
						else
						{
							mtlpp::Function func = ComputeLib.NewFunction(ComputeLib.GetFunctionNames()[0]);
							descriptor.SetComputeFunction(func);
						}
					}
					else
					{
						mtlpp::Function func = ComputeLib.NewFunction(ComputeLib.GetFunctionNames()[0]);
						descriptor.SetComputeFunction(func);
					}
					search = search_old;
				}
			}
			else
			{
				search = search_old;
			}
			
			search_old = search;
			if ((search = strstr(search, ParseTags[ParseEntries::StageInputDescriptor])))
			{
				mtlpp::StageInputOutputDescriptor ComputeDesc;
				
				char const* ColorAttachMarker = strstr(search, "buffers =");
				for(uint32_t i = 0; i < 31; i++)
				{
					// Buffers
					sprintf(Buffer, ParseFormats[ParseEntries::BufferTemplate], i);
					search_old = search;
					search = strstr(search, Buffer);
					if (!search || !strstr(search, "buffers ="))
					{
						search = search_old;
						continue;
					}
					
					mtlpp::BufferLayoutDescriptor VBL = ComputeDesc.GetLayouts()[i];
					
					search_old = search;
					if (SEARCH_AND_SCAN(search, ParseEntries::StepFunc, Buffer))
					{
						mtlpp::StepFunction val;
						if (Parse(Buffer, val))
						{
							VBL.SetStepFunction(val);
							switch (val)
							{
								case mtlpp::StepFunction::Constant:
									VBL.SetStepRate(0);
									break;
								default:
									VBL.SetStepRate(1);
									break;
							}
						}
					}
					else
					{
						search = search_old;
					}
					
					search_old = search;
					if (SEARCH_AND_SCAN(search, ParseEntries::Stride, &IValue))
					{
						VBL.SetStride((uint32_t)IValue);
					}
					else
					{
						search = search_old;
					}
					
					char const* NextBuffer = strstr(search, ParseTags[ParseEntries::BufferTemplate]);
					char const* Guard = NextBuffer;
					if (!Guard || (ColorAttachMarker < NextBuffer))
					{
						Guard = ColorAttachMarker;
					}
					
					// Attributes
					for (uint32_t a = 0; a < 31; a++)
					{
						sprintf(Buffer, ParseFormats[ParseEntries::AttributeTemplate], a);
						search_old = search;
						search = strstr(search, Buffer);
						if (!search || search >= Guard)
						{
							search = search_old;
							continue;
						}
						
						mtlpp::AttributeDescriptor VADesc = ComputeDesc.GetAttributes()[a];
						
						VADesc.SetBufferIndex(i);
						
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::Offset, &IValue))
						{
							VADesc.SetOffset(IValue);
						}
						else
						{
							search = search_old;
						}
						
						search_old = search;
						if (SEARCH_AND_SCAN(search, ParseEntries::Format, Buffer))
						{
							mtlpp::AttributeFormat val;
							if (Parse(Buffer, val))
							{
								VADesc.SetFormat(val);
							}
						}
						else
						{
							search = search_old;
						}
					}
				}
				
				search_old = search;
				if (SEARCH_AND_SCAN(search, ParseEntries::IndexType, Buffer))
				{
					mtlpp::IndexType val;
					if (Parse(Buffer, val))
					{
						ComputeDesc.SetIndexType(val);
					}
				}
				else
				{
					search = search_old;
				}
				
				search_old = search;
				if (SEARCH_AND_SCAN(search, ParseEntries::IndexBufferIndex, &IValue))
				{
					ComputeDesc.SetIndexBufferIndex((uint32_t)IValue);
				}
				else
				{
					search = search_old;
				}
				
				descriptor.SetStageInputDescriptor(ComputeDesc);
			}
			else
			{
				search = search_old;
			}
		}
		
		return true;
	}
	return false;
}

bool ParseTileRenderPipelineDesc()
{
	return false;
}

int main(int argc, const char * argv[])
{
	bool bOK = true;
	
	bool bPrintUsage = true;
	
	if (argc >= 3)
	{
		mtlpp::CompilerOptions Options;
		
		char const* compute_path = nullptr;
		char const* vertex_path = nullptr;
		char const* fragment_path = nullptr;
		char const* tile_path = nullptr;
		char const* descriptor_path = nullptr;

		int deviceId = -1;
		
		for (int i = 1; i < argc; i++)
		{
			if (!strcmp(argv[i], "-c") && i+1 < argc)
			{
				compute_path = argv[++i];
			}
			else if (!strcmp(argv[i], "-v") && i+1 < argc)
			{
				vertex_path = argv[++i];
			}
			else if (!strcmp(argv[i], "-f") && i+1 < argc)
			{
				fragment_path = argv[++i];
			}
			else if (!strcmp(argv[i], "-d") && i+1 < argc)
			{
				descriptor_path = argv[++i];
			}
			else if (!strcmp(argv[i], "-t") && i+1 < argc)
			{
				tile_path = argv[++i];
			}
			else if (!strcmp(argv[i], "-keep-debug-info"))
			{
				Options.KeepDebugInfo = true;
			}
			else if (!strcmp(argv[i], "-no-fast-math"))
			{
				Options.SetFastMathEnabled(false);
			}
			else if (!strcmp(argv[i], "-fast-math"))
			{
				Options.SetFastMathEnabled(true);
			}
			else if (!strcmp(argv[i], "-device") && i+1 < argc)
			{
				deviceId = atoi(argv[++i]);
			}
			else if ((!strcmp(argv[i], "-sdk")) && i+1 < argc)
			{
				i++;
				
				if (!strcmp(argv[i], "macosx"))
				{
					Options.Platform = mtlpp::Platform::macOS;
				}
				else if (!strcmp(argv[i], "iphoneos"))
				{
					Options.Platform = mtlpp::Platform::iOS;
				}
				else if (!strcmp(argv[i], "tvos"))
				{
					Options.Platform = mtlpp::Platform::tvOS;
				}
				else if (!strcmp(argv[i], "watchos"))
				{
					Options.Platform = mtlpp::Platform::watchOS;
				}
			}
			else if ((!strcmp(argv[i], "-vers")) && i+1 < argc)
			{
				i++;
				if (!strcmp(argv[i], "1.0"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version1_0);
				}
				else if (!strcmp(argv[i], "1.1"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version1_1);
				}
				else if (!strcmp(argv[i], "1.2"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version1_2);
				}
				else if (!strcmp(argv[i], "2.0"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_0);
				}
				else if (!strcmp(argv[i], "2.1"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_1);
				}
				else if (!strcmp(argv[i], "2.2"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_2);
				}
				else if (!strcmp(argv[i], "2.3"))
				{
					Options.SetLanguageVersion(mtlpp::LanguageVersion::Version2_3);
				}
			}
		}

		mtlpp::Device device;
		if (deviceId < 0)
		{
			device = mtlpp::Device::CreateSystemDefaultDevice();
		}
		else
		{
			device = mtlpp::Device::CopyAllDevices()[deviceId];
		}
		
		NSLog(@"Device: %@", device.GetName().GetPtr());
		
		if (compute_path)
		{
			bPrintUsage = false;
			
			mtlpp::Library ComputeLib;
			bOK = CompileAndLink([NSString stringWithUTF8String:compute_path], @"/tmp/Compute.tmp", @"/tmp/Compute.metallib", Options);
			if (bOK)
			{
				ns::AutoReleasedError error;
				ComputeLib = device.NewLibrary(ns::String(@"/tmp/Compute.metallib"), &error);
				if (error.GetPtr())
				{
					NSLog(@"ComputeLib Output: %@", error.GetPtr());
				}
				bOK = (ComputeLib.GetPtr());
			}
			
			if (bOK)
			{
				if (descriptor_path)
				{
					mtlpp::ComputePipelineDescriptor descriptor;
					bOK = ParseComputePipelineDesc(descriptor_path, ComputeLib, descriptor);
					
					// Compile pipeline
					if (bOK)
					{
						ns::AutoReleasedError error;
						mtlpp::ComputePipelineState state = device.NewComputePipelineState(descriptor, mtlpp::PipelineOption::NoPipelineOption, nullptr, &error);
						if (error.GetPtr())
						{
							NSLog(@"ComputePipelineState Output: %@", error.GetPtr());
						}
						bOK = (state.GetPtr());
					}
				}
				else
				{
					ns::AutoReleasedError error;
					mtlpp::FunctionConstantValues ConstVals;
					uint32_t Val = 0;
					for (uint32_t i = 0; i < 32; i++)
					{
						ConstVals.SetConstantValue(&Val, mtlpp::DataType::UInt, i);
					}
					mtlpp::Function func = ComputeLib.NewFunction(ComputeLib.GetFunctionNames()[0], ConstVals, &error);
					if (error.GetPtr())
					{
						NSLog(@"ComputeLib.NewFunction Output: %@", error.GetPtr());
					}
					if(func)
					{
						ns::AutoReleasedError error;
						mtlpp::ComputePipelineState state = device.NewComputePipelineState(func, &error);
						if (error.GetPtr())
						{
							NSLog(@"ComputePipelineState Output: %@", error.GetPtr());
						}
						bOK = (state.GetPtr());
					}
				}
			}
		}
		else if (tile_path && descriptor_path)
		{
			bPrintUsage = false;
			
			bOK = CompileAndLink([NSString stringWithUTF8String:tile_path], @"/tmp/Tile.tmp", @"/tmp/Tile.metallib", Options);
			
			if (bOK)
			{
				bOK = ParseTileRenderPipelineDesc();
			}
			
			// Compile pipeline
			if (bOK)
			{
			}
		}
		else if (vertex_path && descriptor_path)
		{
			bPrintUsage = false;
			
			mtlpp::Library VertexLib;
			bOK = CompileAndLink([NSString stringWithUTF8String:vertex_path], @"/tmp/Vertex.tmp.o", @"/tmp/Vertex.metallib", Options);
			if (bOK)
			{
				ns::AutoReleasedError error;
				VertexLib = device.NewLibrary(ns::String(@"/tmp/Vertex.metallib"), &error);
				if (error.GetPtr())
				{
					NSLog(@"VertexLib Output: %@", error.GetPtr());
				}
				bOK = (VertexLib.GetPtr());
			}
			
			mtlpp::Library FragmentLib;
			if (bOK && fragment_path)
			{
				bOK = CompileAndLink([NSString stringWithUTF8String:fragment_path], @"/tmp/Fragment.tmp.o", @"/tmp/Fragment.metallib", Options);
				if (bOK)
				{
					ns::AutoReleasedError error;
					FragmentLib = device.NewLibrary(ns::String(@"/tmp/Fragment.metallib"), &error);
					if (error.GetPtr())
					{
						NSLog(@"FragmentLib Output: %@", error.GetPtr());
					}
					bOK = (FragmentLib.GetPtr());
				}
			}
			
			mtlpp::RenderPipelineDescriptor descriptor;
			if (bOK)
			{
				bOK = ParseRenderPipelineDesc(descriptor_path, VertexLib, FragmentLib, descriptor);
			}
			
			// Compile pipeline
			if (bOK)
			{
				ns::AutoReleasedError error;
				mtlpp::RenderPipelineState renderPipelineState = device.NewRenderPipelineState(descriptor, &error);
				if (error.GetPtr())
				{
					NSLog(@"renderPipelineState Output: %@", error.GetPtr());
				}
				bOK = (renderPipelineState.GetPtr());
			}
		}
	}
	
	if (bPrintUsage)
	{
		printf("mtlpp <-v vertex-shader-path> <-f fragment-shader-path> <-c compute-shader-path> <-t tile-shader-path> <-d descriptor-input-path> <options>\n");
		printf("Usage:\n");
		printf("\tThis command-line tool will compile the given input .metal files and then compile them into a Metal pipeline state to verify the runtime compiler.\n");
		printf("\tFor compute shaders this can be done with just the shader file, but for advanced usage, or for render-pipelines, it is necessary to submit the appropriate pipeline descriptor information.\n");
		printf("\tThe pipeline descriptor information should be in the same form as reported by -[NSObject debugDescription] used by Xcode/LLDB's Print-Object feature.\n");
		printf("Options:\n");
		printf("\t-keep-debug-info: if specified the selected files will be compiled to keep debug info.\n");
		printf("\t-no-fast-math: if specified the selected files will be compiled with fast-math explicitly disabled.\n");
		printf("\t-fast-math: if specified the selected files will be compiled with fast-math explicitly enabled.\n");
		printf("\t-sdk [macosx/iphoneos/tvos/watchos]: if specified the selected files will be compiled with specified platform that must follow.\n");
		printf("\t-vers [1.0/1.1/1.2/2.0]: if specified the selected files will be compiled with specified shader standard that must follow.\n");
		printf("\t-device [0-n]: The Metal GPU index to use - omit for the system default device.\n");
	}
	
	return !bOK;
}
