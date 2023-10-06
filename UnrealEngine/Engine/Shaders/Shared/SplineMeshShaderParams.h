// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// The packed size of the spline mesh data in float4s
#define SPLINE_MESH_PARAMS_FLOAT4_SIZE 8

#ifdef __cplusplus
#include "HLSLTypeAliases.h"

namespace UE::HLSL
{
#endif // __cplusplus

struct FSplineMeshShaderParams
{
	float3 StartPos;
	float3 EndPos;
	float3 StartTangent;
	float3 EndTangent;
	float2 StartScale;
	float2 EndScale;
	float2 StartOffset;
	float2 EndOffset;
	float StartRoll;
	float EndRoll;
	float MeshScaleZ;
	float MeshMinZ;
	float2 MeshDeformScaleMinMax;
	bool bSmoothInterpRollScale;
	float3 SplineUpDir;
	float3 MeshDir;
	float3 MeshX;
	float3 MeshY;
};

#ifdef __cplusplus
} // namespace UE::HLSL
using FSplineMeshShaderParams = UE::HLSL::FSplineMeshShaderParams;
#endif // __cplusplus

#ifndef __cplusplus
#include "/Engine/Private/Quaternion.ush"

// HLSL unpack method
FSplineMeshShaderParams UnpackSplineMeshParams(float4 PackedParams[SPLINE_MESH_PARAMS_FLOAT4_SIZE])
{
	FSplineMeshShaderParams Output;
	Output.StartPos 				= PackedParams[0].xyz;
	Output.EndPos 					= PackedParams[1].xyz;
	Output.StartTangent 			= PackedParams[2].xyz;
	Output.EndTangent 				= PackedParams[3].xyz;
	Output.StartScale 				= float2(PackedParams[0].w, PackedParams[1].w);
	Output.EndScale 				= float2(PackedParams[2].w, PackedParams[3].w);
	Output.StartOffset 				= PackedParams[4].xy;
	Output.EndOffset 				= PackedParams[4].zw;
	Output.StartRoll 				= PackedParams[5].x;
	Output.EndRoll 					= PackedParams[5].y;
	Output.MeshScaleZ 				= PackedParams[5].z;
	Output.MeshMinZ 				= PackedParams[5].w;
	Output.SplineUpDir 				= float3(SNorm16ToF32(asuint(PackedParams[6].x)),
											 SNorm16ToF32(asuint(PackedParams[6].x) >> 16u),
											 SNorm16ToF32(asuint(PackedParams[6].y)));

	FQuat MeshQuat					= FQuat(SNorm16ToF32(asuint(PackedParams[6].z)),
											SNorm16ToF32(asuint(PackedParams[6].z) >> 16u),
											SNorm16ToF32(asuint(PackedParams[6].w)),
											SNorm16ToF32(asuint(PackedParams[6].w) >> 16u));
	float3x3 MeshRot 				= QuatToMatrix(MeshQuat);
	Output.MeshDir					= MeshRot[0];
	Output.MeshX					= MeshRot[1];
	Output.MeshY					= MeshRot[2];

	Output.MeshDeformScaleMinMax	= PackedParams[7].xy;
	Output.bSmoothInterpRollScale 	= PackedParams[7].z != 0.0f;

	return Output;
}
#endif // !__cplusplus
