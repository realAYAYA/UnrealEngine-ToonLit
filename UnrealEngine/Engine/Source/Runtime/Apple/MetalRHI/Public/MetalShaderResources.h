// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaderResources.h: Metal shader resource RHI definitions.
=============================================================================*/

#pragma once

#include "CrossCompilerCommon.h"

/**
* Shader related constants.
*/
enum
{
	METAL_MAX_UNIFORM_BUFFER_BINDINGS = 12,	// @todo-mobile: Remove me
	METAL_FIRST_UNIFORM_BUFFER = 0,			// @todo-mobile: Remove me
	METAL_MAX_COMPUTE_STAGE_UAV_UNITS = 8,	// @todo-mobile: Remove me
	METAL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT = -1, // for now, only CS supports UAVs/ images
	METAL_MAX_BUFFERS = 31,
};

/**
* Buffer data-types for MetalRHI & MetalSL
*/
enum class EMetalBufferFormat : uint8
{
	Unknown					=0,
	
	R8Sint					=1,
	R8Uint					=2,
	R8Snorm					=3,
	R8Unorm					=4,
	
	R16Sint					=5,
	R16Uint					=6,
	R16Snorm				=7,
	R16Unorm				=8,
	R16Half					=9,
	
	R32Sint					=10,
	R32Uint					=11,
	R32Float				=12,
	
	RG8Sint					=13,
	RG8Uint					=14,
	RG8Snorm				=15,
	RG8Unorm				=16,
	
	RG16Sint				=17,
	RG16Uint				=18,
	RG16Snorm				=19,
	RG16Unorm				=20,
	RG16Half				=21,
	
	RG32Sint				=22,
	RG32Uint				=23,
	RG32Float				=24,
	
	RGB8Sint				=25,
	RGB8Uint				=26,
	RGB8Snorm				=27,
	RGB8Unorm				=28,
	
	RGB16Sint				=29,
	RGB16Uint				=30,
	RGB16Snorm				=31,
	RGB16Unorm				=32,
	RGB16Half				=33,
	
	RGB32Sint				=34,
	RGB32Uint				=35,
	RGB32Float				=36,
	
	RGBA8Sint				=37,
	RGBA8Uint				=38,
	RGBA8Snorm				=39,
	RGBA8Unorm				=40,
	
	BGRA8Unorm				=41,
	
	RGBA16Sint				=42,
	RGBA16Uint				=43,
	RGBA16Snorm				=44,
	RGBA16Unorm				=45,
	RGBA16Half				=46,
	
	RGBA32Sint				=47,
	RGBA32Uint				=48,
	RGBA32Float				=49,
	
	RGB10A2Unorm			=50,
	
	RG11B10Half 			=51,
	
	R5G6B5Unorm         	=52,
	B5G5R5A1Unorm           =53,

	Max						=54
};

struct FMetalShaderBindings
{
	TArray<TArray<CrossCompiler::FPackedArrayInfo>>	PackedUniformBuffers;
	TArray<CrossCompiler::FPackedArrayInfo>			PackedGlobalArrays;
	FShaderResourceTable							ShaderResourceTable;
	TMap<uint8, TArray<uint8>>						ArgumentBufferMasks;
	CrossCompiler::FShaderBindingInOutMask			InOutMask;
    FString                                         IRConverterReflectionJSON;
    uint32                                          RSNumCBVs;
    uint32                                          OutputSizeVS;
    uint32                                          MaxInputPrimitivesPerMeshThreadgroupGS;

	uint32 	ConstantBuffers;
	uint32  ArgumentBuffers;
	uint8	NumSamplers;
	uint8	NumUniformBuffers;
	uint8	NumUAVs;
	bool	bDiscards;

	FMetalShaderBindings() :
        RSNumCBVs(0),
        OutputSizeVS(0),
        MaxInputPrimitivesPerMeshThreadgroupGS(0),
		ConstantBuffers(0),
		ArgumentBuffers(0),
		NumSamplers(0),
		NumUniformBuffers(0),
		NumUAVs(0),
		bDiscards(false)
	{
	}
};

inline FArchive& operator<<(FArchive& Ar, FMetalShaderBindings& Bindings)
{
	Ar << Bindings.PackedUniformBuffers;
	Ar << Bindings.PackedGlobalArrays;
	Ar << Bindings.ShaderResourceTable;
	Ar << Bindings.ArgumentBufferMasks;
	Ar << Bindings.ConstantBuffers;
	Ar << Bindings.ArgumentBuffers;
	Ar << Bindings.InOutMask;
	Ar << Bindings.NumSamplers;
	Ar << Bindings.NumUniformBuffers;
	Ar << Bindings.NumUAVs;
	Ar << Bindings.bDiscards;
    Ar << Bindings.IRConverterReflectionJSON;
    Ar << Bindings.RSNumCBVs;
    Ar << Bindings.OutputSizeVS;
    Ar << Bindings.MaxInputPrimitivesPerMeshThreadgroupGS;
	return Ar;
}

enum class EMetalOutputWindingMode : uint8
{
	Clockwise = 0,
	CounterClockwise = 1,
};

enum class EMetalPartitionMode : uint8
{
	Pow2 = 0,
	Integer = 1,
	FractionalOdd = 2,
	FractionalEven = 3,
};

enum class EMetalComponentType : uint8
{
	Uint = 0,
	Int,
	Half,
	Float,
	Bool,
	Max
};

struct FMetalRayTracingHeader
{
	uint32 InstanceIndexBuffer;

	bool IsValid() const
	{
		return InstanceIndexBuffer != UINT32_MAX;
	}

	FMetalRayTracingHeader()
		: InstanceIndexBuffer(UINT32_MAX)
	{

	}

	friend FArchive& operator<<(FArchive& Ar, FMetalRayTracingHeader& Header)
	{
		Ar << Header.InstanceIndexBuffer;
		return Ar;
	}
};

struct FMetalAttribute
{
	uint32 Index;
	uint32 Components;
	uint32 Offset;
	EMetalComponentType Type;
	uint32 Semantic;
	
	FMetalAttribute()
	: Index(0)
	, Components(0)
	, Offset(0)
	, Type(EMetalComponentType::Uint)
	, Semantic(0)
	{
		
	}
	
	friend FArchive& operator<<(FArchive& Ar, FMetalAttribute& Attr)
	{
		Ar << Attr.Index;
		Ar << Attr.Type;
		Ar << Attr.Components;
		Ar << Attr.Offset;
		Ar << Attr.Semantic;
		return Ar;
	}
};

struct FMetalCodeHeader
{
	FMetalShaderBindings Bindings;
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;

	uint64 CompilerBuild;
	uint32 CompilerVersion;
	uint32 SourceLen;
	uint32 SourceCRC;
	uint32 NumThreadsX;
	uint32 NumThreadsY;
	uint32 NumThreadsZ;
	uint32 CompileFlags;
	uint8 Frequency;
	uint32 Version;
	int8 SideTable;
	bool bDeviceFunctionConstants;
	FMetalRayTracingHeader RayTracing;

	FMetalCodeHeader()
	: CompilerBuild(0)
	, CompilerVersion(0)
	, SourceLen(0)
	, SourceCRC(0)
	, NumThreadsX(0)
	, NumThreadsY(0)
	, NumThreadsZ(0)
	, CompileFlags(0)
	, Frequency(0)
	, Version(0)
	, SideTable(-1)
	, bDeviceFunctionConstants(false)
	{
	}
};

inline FArchive& operator<<(FArchive& Ar, FMetalCodeHeader& Header)
{
	Ar << Header.Bindings;

	int32 NumInfos = Header.UniformBuffersCopyInfo.Num();
	Ar << NumInfos;
	if (Ar.IsSaving())
	{
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			Ar << Header.UniformBuffersCopyInfo[Index];
		}
	}
	else if (Ar.IsLoading())
	{
		Header.UniformBuffersCopyInfo.Empty(NumInfos);
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			CrossCompiler::FUniformBufferCopyInfo Info;
			Ar << Info;
			Header.UniformBuffersCopyInfo.Add(Info);
		}
	}
	
	Ar << Header.CompilerBuild;
	Ar << Header.CompilerVersion;
	Ar << Header.SourceLen;
	Ar << Header.SourceCRC;
	Ar << Header.NumThreadsX;
	Ar << Header.NumThreadsY;
	Ar << Header.NumThreadsZ;
	Ar << Header.CompileFlags;
	Ar << Header.Frequency;
	Ar << Header.Version;
	Ar << Header.SideTable;
	Ar << Header.bDeviceFunctionConstants;
	Ar << Header.RayTracing;
    return Ar;
}

struct FMetalShaderLibraryHeader
{
	FString Format;
	uint32 NumLibraries;
	uint32 NumShadersPerLibrary;
	
	friend FArchive& operator<<(FArchive& Ar, FMetalShaderLibraryHeader& Header)
	{
		return Ar << Header.Format << Header.NumLibraries << Header.NumShadersPerLibrary;
	}
};
