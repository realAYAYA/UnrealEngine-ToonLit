// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphFwd.h"
#include "ShaderParameterStruct.h"

namespace NiagaraStateless
{
	BEGIN_SHADER_PARAMETER_STRUCT(FInitializeParticleModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(uint32,		InitializeParticle_ModuleFlags)
		SHADER_PARAMETER(FUintVector3,	InitializeParticle_InitialPosition)

		SHADER_PARAMETER(FLinearColor,	InitializeParticle_ColorScale)				// Unset / Direct Set / Random Range(Link RGBA / Link RGB|Link A / Random Channels) / Random Hue|Saturation|Value
		SHADER_PARAMETER(FLinearColor,	InitializeParticle_ColorBias)

		SHADER_PARAMETER(FVector2f, InitializeParticle_SpriteSizeScale)			// Unset / Uniform / Random Uniform / Non-Uniform / Random Non-Uniform
		SHADER_PARAMETER(FVector2f, InitializeParticle_SpriteSizeBias)
		SHADER_PARAMETER(float,		InitializeParticle_SpriteRotationScale)				// Unset / Random / Direct Set Deg / Direct Set Normalized
		SHADER_PARAMETER(float,		InitializeParticle_SpriteRotationBias)
		//SHADER_PARAMETER(FVector3f, InitializeParticle_SpriteUVMode)			// Unset / Random / Random X / Random Y / Random XY / Direct Set

		SHADER_PARAMETER(FVector3f,	InitializeParticle_MeshScaleScale)			// Unset / Uniform / Random Uniform / Non-Uniform / Random Non-Uniform
		SHADER_PARAMETER(FVector3f,	InitializeParticle_MeshScaleBias)

		SHADER_PARAMETER(float,		InitializeParticle_RibbonWidthScale)		// Unset / Direct Set
		SHADER_PARAMETER(float,		InitializeParticle_RibbonWidthBias)
		//SHADER_PARAMETER(FVector3f, InitializeParticle_RibbonFacingVector)		// Unset / Direct Set
		//SHADER_PARAMETER(FVector3f, InitializeParticle_RibbonTwist)				// Unset / Direct Set
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FInitialMeshOrientationModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FVector3f,	InitialMeshOrientation_Rotation)
		SHADER_PARAMETER(FVector3f,	InitialMeshOrientation_RandomRangeScale)
	END_SHADER_PARAMETER_STRUCT()
		

	BEGIN_SHADER_PARAMETER_STRUCT(FRotateAroundPointModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(float,	RotateAroundPoint_RateScale)
		SHADER_PARAMETER(float,	RotateAroundPoint_RateBias)
		SHADER_PARAMETER(float,	RotateAroundPoint_RadiusScale)
		SHADER_PARAMETER(float,	RotateAroundPoint_RadiusBias)
		SHADER_PARAMETER(float,	RotateAroundPoint_InitialPhaseScale)
		SHADER_PARAMETER(float,	RotateAroundPoint_InitialPhaseBias)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FDynamicMaterialParametersModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(uint32,		DynamicMaterialParameters_ChannelMask)
		SHADER_PARAMETER(FUintVector3,	DynamicMaterialParameters_Parameter0X)
		SHADER_PARAMETER(FUintVector3,	DynamicMaterialParameters_Parameter0Y)
		SHADER_PARAMETER(FUintVector3,	DynamicMaterialParameters_Parameter0Z)
		SHADER_PARAMETER(FUintVector3,	DynamicMaterialParameters_Parameter0W)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FCameraOffsetModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FUintVector3,	CameraOffset_Distribution)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FScaleColorModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FUintVector3,	ScaleColor_Distribution)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FScaleMeshSizeModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FUintVector3,	ScaleMeshSize_Distribution)
		SHADER_PARAMETER(FVector3f,		ScaleMeshSize_CurveScale)
		SHADER_PARAMETER(int32,			ScaleMeshSize_CurveScaleOffset)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FScaleMeshSizeBySpeedModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FUintVector2,	ScaleMeshSizeBySpeed_ScaleDistribution)
		SHADER_PARAMETER(float,			ScaleMeshSizeBySpeed_VelocityNorm)
	END_SHADER_PARAMETER_STRUCT()
		
	BEGIN_SHADER_PARAMETER_STRUCT(FMeshIndexModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(int32,		MeshIndex_Index)
		SHADER_PARAMETER(int32,		MeshIndex_TableOffset)
		SHADER_PARAMETER(int32,		MeshIndex_TableNumElements)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FMeshRotationRateModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FVector3f,		MeshRotationRate_Scale)
		SHADER_PARAMETER(FVector3f,		MeshRotationRate_Bias)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FScaleSpriteSizeModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FUintVector3,	ScaleSpriteSize_Distribution)
		SHADER_PARAMETER(FVector2f,		ScaleSpriteSize_CurveScale)
		SHADER_PARAMETER(int32,			ScaleSpriteSize_CurveScaleOffset)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FScaleSpriteSizeBySpeedModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FUintVector2,	ScaleSpriteSizeBySpeed_ScaleDistribution)
		SHADER_PARAMETER(float,			ScaleSpriteSizeBySpeed_VelocityNorm)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSpriteFacingAndAlignmentModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FVector3f,	SpriteFacingAndAlignment_SpriteFacing)
		SHADER_PARAMETER(FVector3f,	SpriteFacingAndAlignment_SpriteAlignment)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSpriteRotationRateModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(float,		SpriteRotationRate_Scale)
		SHADER_PARAMETER(float,		SpriteRotationRate_Bias)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSubUVAnimationModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(int,		SubUVAnimation_Mode)
		SHADER_PARAMETER(float,		SubUVAnimation_NumFrames)
		SHADER_PARAMETER(float,		SubUVAnimation_InitialFrameScale)
		SHADER_PARAMETER(float,		SubUVAnimation_InitialFrameBias)
		SHADER_PARAMETER(float,		SubUVAnimation_InitialFrameRateChange)
		SHADER_PARAMETER(float,		SubUVAnimation_AnimFrameStart)
		SHADER_PARAMETER(float,		SubUVAnimation_AnimFrameRange)
		SHADER_PARAMETER(float,		SubUVAnimation_RateScale)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FShapeLocationModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(FUintVector4,	ShapeLocation_Mode)
		SHADER_PARAMETER(FVector4f,		ShapeLocation_Parameters0)
		SHADER_PARAMETER(FVector4f,		ShapeLocation_Parameters1)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSolveVelocitiesAndForcesModule_ShaderParameters, NIAGARASHADER_API)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_MassScale)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_MassBias)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_DragScale)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_DragBias)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_VelocityScale)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_VelocityBias)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_WindScale)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_WindBias)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_AccelerationScale)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_AccelerationBias)

		SHADER_PARAMETER(uint32,		SolveVelocitiesAndForces_ConeVelocityEnabled)
		SHADER_PARAMETER(FQuat4f,		SolveVelocitiesAndForces_ConeQuat)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_ConeVelocityScale)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_ConeVelocityBias)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_ConeAngleScale)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_ConeAngleBias)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_ConeVelocityFalloff)

		SHADER_PARAMETER(uint32,		SolveVelocitiesAndForces_PontVelocityEnabled)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_PointVelocityScale)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_PointVelocityBias)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_PointOrigin)

		SHADER_PARAMETER(uint32,		SolveVelocitiesAndForces_NoiseEnabled)
		SHADER_PARAMETER(float,			SolveVelocitiesAndForces_NoiseAmplitude)
		SHADER_PARAMETER(FVector3f,		SolveVelocitiesAndForces_NoiseFrequency)
		SHADER_PARAMETER_TEXTURE(Texture3D,		SolveVelocitiesAndForces_NoiseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState,	SolveVelocitiesAndForces_NoiseSampler)

		SHADER_PARAMETER(uint32, SolveVelocitiesAndForces_NoiseMode)
		SHADER_PARAMETER(uint32, SolveVelocitiesAndForces_NoiseLUTOffset)
		SHADER_PARAMETER(uint32, SolveVelocitiesAndForces_NoiseLUTNumChannel)
		SHADER_PARAMETER(uint32, SolveVelocitiesAndForces_NoiseLUTChannelWidth)

	END_SHADER_PARAMETER_STRUCT()
}
