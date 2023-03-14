// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "Engine/EngineTypes.h"
#include "SceneTypes.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"
#include "InstanceUniformShaderParameters.h"
#include "LightmapUniformShaderParameters.h"
#include "UnifiedBuffer.h"
#include "Containers/StaticArray.h"
#include "NaniteDefinitions.h"
#include "UnrealEngine.h"

/** 
 * The uniform shader parameters associated with a primitive. 
 * Note: Must match FPrimitiveSceneData in shaders.
 * Note 2: Try to keep this 16 byte aligned. i.e |Matrix4x4|Vector3,float|Vector3,float|Vector4|  _NOT_  |Vector3,(waste padding)|Vector3,(waste padding)|Vector3. Or at least mark out padding if it can't be avoided.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FPrimitiveUniformShaderParameters,ENGINE_API)
	SHADER_PARAMETER(uint32,		Flags)
	SHADER_PARAMETER(uint32,		InstanceSceneDataOffset)
	SHADER_PARAMETER(uint32,		NumInstanceSceneDataEntries)
	SHADER_PARAMETER(int32,			SingleCaptureIndex)										// Should default to 0 if no reflection captures are provided, as there will be a default black (0,0,0,0) cubemap in that slot
	SHADER_PARAMETER(FVector3f,		TilePosition)
	SHADER_PARAMETER(uint32,		PrimitiveComponentId)									// TODO: Refactor to use PersistentPrimitiveIndex, ENGINE USE ONLY - will be removed
	SHADER_PARAMETER(FMatrix44f,	LocalToRelativeWorld)									// Always needed
	SHADER_PARAMETER(FMatrix44f,	RelativeWorldToLocal)									// Rarely needed
	SHADER_PARAMETER(FMatrix44f,	PreviousLocalToRelativeWorld)							// Used to calculate velocity
	SHADER_PARAMETER(FMatrix44f,	PreviousRelativeWorldToLocal)							// Rarely used when calculating velocity, if material uses vertex offset along with world->local transform
	SHADER_PARAMETER_EX(FVector3f,	InvNonUniformScale,  EShaderPrecisionModifier::Half)	// Often needed
	SHADER_PARAMETER(float,			ObjectBoundsX)											// Only needed for editor/development
	SHADER_PARAMETER(FVector4f,		ObjectRelativeWorldPositionAndRadius)					// Needed by some materials
	SHADER_PARAMETER(FVector3f,		ActorRelativeWorldPosition)
	SHADER_PARAMETER(uint32,		LightmapUVIndex)										// Only needed if static lighting is enabled
	SHADER_PARAMETER_EX(FVector3f,	ObjectOrientation,   EShaderPrecisionModifier::Half)
	SHADER_PARAMETER(uint32,		LightmapDataIndex)										// Only needed if static lighting is enabled
	SHADER_PARAMETER_EX(FVector4f,	NonUniformScale,     EShaderPrecisionModifier::Half)
	SHADER_PARAMETER(FVector3f,		PreSkinnedLocalBoundsMin)								// Local space min bounds, pre-skinning
	SHADER_PARAMETER(uint32,		NaniteResourceID)
	SHADER_PARAMETER(FVector3f,		PreSkinnedLocalBoundsMax)								// Local space bounds, pre-skinning
	SHADER_PARAMETER(uint32,		NaniteHierarchyOffset)
	SHADER_PARAMETER(FVector3f,		LocalObjectBoundsMin)									// This is used in a custom material function (ObjectLocalBounds.uasset)
	SHADER_PARAMETER(float,			ObjectBoundsY)											// Only needed for editor/development
	SHADER_PARAMETER(FVector3f,		LocalObjectBoundsMax)									// This is used in a custom material function (ObjectLocalBounds.uasset)
	SHADER_PARAMETER(float,			ObjectBoundsZ)											// Only needed for editor/development
	SHADER_PARAMETER(FVector3f,		InstanceLocalBoundsCenter)
	SHADER_PARAMETER(uint32,		InstancePayloadDataOffset)
	SHADER_PARAMETER(FVector3f,		InstanceLocalBoundsExtent)
	SHADER_PARAMETER(uint32,		InstancePayloadDataStride)
	SHADER_PARAMETER(FVector3f,		WireframeColor)											// Only needed for editor/development
	SHADER_PARAMETER(uint32,		PackedNaniteFlags)
	SHADER_PARAMETER(FVector3f,		LevelColor)												// Only needed for editor/development
	SHADER_PARAMETER(int32,			PersistentPrimitiveIndex)
	SHADER_PARAMETER(FVector2f,		InstanceDrawDistanceMinMaxSquared)
	SHADER_PARAMETER(float,			InstanceWPODisableDistanceSquared)
	SHADER_PARAMETER(uint32,		NaniteRayTracingDataOffset)
	SHADER_PARAMETER(FVector3f,		Unused)
	SHADER_PARAMETER(float,			BoundsScale)
	SHADER_PARAMETER_ARRAY(FVector4f, CustomPrimitiveData, [FCustomPrimitiveData::NumCustomPrimitiveDataFloat4s]) // Custom data per primitive that can be accessed through material expression parameters and modified through UStaticMeshComponent
END_GLOBAL_SHADER_PARAMETER_STRUCT()

// Must match SceneData.ush
#define PRIMITIVE_SCENE_DATA_FLAG_CAST_SHADOWS							0x1
#define PRIMITIVE_SCENE_DATA_FLAG_USE_SINGLE_SAMPLE_SHADOW_SL			0x2
#define PRIMITIVE_SCENE_DATA_FLAG_USE_VOLUMETRIC_LM_SHADOW_SL			0x4
#define PRIMITIVE_SCENE_DATA_FLAG_DECAL_RECEIVER						0x8
#define PRIMITIVE_SCENE_DATA_FLAG_CACHE_SHADOW_AS_STATIC				0x10
#define PRIMITIVE_SCENE_DATA_FLAG_OUTPUT_VELOCITY						0x20
#define PRIMITIVE_SCENE_DATA_FLAG_DETERMINANT_SIGN						0x40
#define PRIMITIVE_SCENE_DATA_FLAG_HAS_CAPSULE_REPRESENTATION			0x80
#define PRIMITIVE_SCENE_DATA_FLAG_HAS_CAST_CONTACT_SHADOW				0x100
#define PRIMITIVE_SCENE_DATA_FLAG_HAS_PRIMITIVE_CUSTOM_DATA				0x200
#define PRIMITIVE_SCENE_DATA_FLAG_LIGHTING_CHANNEL_0					0x400
#define PRIMITIVE_SCENE_DATA_FLAG_LIGHTING_CHANNEL_1					0x800
#define PRIMITIVE_SCENE_DATA_FLAG_LIGHTING_CHANNEL_2					0x1000
#define PRIMITIVE_SCENE_DATA_FLAG_HAS_INSTANCE_LOCAL_BOUNDS				0x2000
#define PRIMITIVE_SCENE_DATA_FLAG_HAS_NANITE_IMPOSTER					0x4000
#define PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_GAME						0x8000
#define PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_EDITOR						0x10000
#define PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_REFLECTION_CAPTURES		0x20000
#define PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_REAL_TIME_SKY_CAPTURES		0x40000
#define PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_RAY_TRACING				0x80000
#define PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_SCENE_CAPTURE_ONLY			0x100000
#define PRIMITIVE_SCENE_DATA_FLAG_HIDDEN_IN_SCENE_CAPTURE				0x200000
#define PRIMITIVE_SCENE_DATA_FLAG_FORCE_HIDDEN							0x400000
#define PRIMITIVE_SCENE_DATA_FLAG_CAST_HIDDEN_SHADOW					0x800000
#define PRIMITIVE_SCENE_DATA_FLAG_EVALUATE_WORLD_POSITION_OFFSET		0x1000000
#define PRIMITIVE_SCENE_DATA_FLAG_INSTANCE_DRAW_DISTANCE_CULL			0x2000000
#define PRIMITIVE_SCENE_DATA_FLAG_WPO_DISABLE_DISTANCE					0x4000000

struct FPrimitiveUniformShaderParametersBuilder
{
public:
	inline FPrimitiveUniformShaderParametersBuilder& Defaults()
	{
		ObjectRadius = 0.0f;

		// Flags defaulted on
		bCastShadow									= true;
		bCastContactShadow							= true;
		bEvaluateWorldPositionOffset				= true;
		bVisibleInGame								= true;
		bVisibleInEditor							= true;
		bVisibleInReflectionCaptures				= true;
		bVisibleInRealTimeSkyCaptures				= true;
		bVisibleInRayTracing						= true;

		// Flags defaulted off
		bReceivesDecals								= false;
		bUseSingleSampleShadowFromStationaryLights	= false;
		bUseVolumetricLightmap						= false;
		bCacheShadowAsStatic						= false;
		bOutputVelocity								= false;
		bHasCapsuleRepresentation					= false;
		bHasPreSkinnedLocalBounds					= false;
		bHasPreviousLocalToWorld					= false;
		bHasInstanceLocalBounds						= false;
		bCastHiddenShadow							= false;
		bVisibleInSceneCaptureOnly					= false;
		bHiddenInSceneCapture						= false;
		bForceHidden								= false;
		bHasNaniteImposter							= false;
		bHasInstanceDrawDistanceCull				= false;
		bHasWPODisableDistance						= false;

		Parameters.BoundsScale						= 1.0f;

		// Default colors
		Parameters.WireframeColor					= FVector3f(1.0f, 1.0f, 1.0f);
		Parameters.LevelColor						= FVector3f(1.0f, 1.0f, 1.0f);

		// Invalid indices
		Parameters.LightmapDataIndex				= INDEX_NONE;
		Parameters.LightmapUVIndex					= INDEX_NONE;
		Parameters.SingleCaptureIndex				= INDEX_NONE;
		Parameters.PersistentPrimitiveIndex			= INDEX_NONE;
		Parameters.PrimitiveComponentId				= ~uint32(0u);

		// Nanite
		Parameters.NaniteResourceID						= INDEX_NONE;
		Parameters.NaniteHierarchyOffset				= INDEX_NONE;
		Parameters.PackedNaniteFlags					= NANITE_IMPOSTER_INDEX_MASK;
		Parameters.NaniteRayTracingDataOffset			= INDEX_NONE;

		// Instance data
		Parameters.InstanceSceneDataOffset			= INDEX_NONE;
		Parameters.NumInstanceSceneDataEntries		= 0;
		Parameters.InstancePayloadDataOffset		= INDEX_NONE;
		Parameters.InstancePayloadDataStride		= 0;

		LightingChannels = GetDefaultLightingChannelMask();

		return CustomPrimitiveData(nullptr);
	}

#define PRIMITIVE_UNIFORM_BUILDER_METHOD(INPUT_TYPE, VARIABLE_NAME) \
	inline FPrimitiveUniformShaderParametersBuilder& VARIABLE_NAME(INPUT_TYPE In##VARIABLE_NAME) { Parameters.VARIABLE_NAME = In##VARIABLE_NAME; return *this; }

#define PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(INPUT_TYPE, VARIABLE_NAME) \
	inline FPrimitiveUniformShaderParametersBuilder& VARIABLE_NAME(INPUT_TYPE In##VARIABLE_NAME) { b##VARIABLE_NAME = In##VARIABLE_NAME; return *this; }

	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			ReceivesDecals);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			HasCapsuleRepresentation);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			HasInstanceLocalBounds);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			CastContactShadow);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			CastHiddenShadow);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			CastShadow);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			UseSingleSampleShadowFromStationaryLights);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			UseVolumetricLightmap);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			CacheShadowAsStatic);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			OutputVelocity);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			EvaluateWorldPositionOffset);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			VisibleInGame);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			VisibleInEditor);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			VisibleInReflectionCaptures);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			VisibleInRealTimeSkyCaptures);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			VisibleInRayTracing);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			VisibleInSceneCaptureOnly);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			HiddenInSceneCapture);
	PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD(bool,			ForceHidden);

	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			InstanceSceneDataOffset);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			NumInstanceSceneDataEntries);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			InstancePayloadDataOffset);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			InstancePayloadDataStride);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(int32,				SingleCaptureIndex);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(int32,				PersistentPrimitiveIndex);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			PrimitiveComponentId);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			NaniteResourceID);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			NaniteHierarchyOffset);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			NaniteRayTracingDataOffset);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			LightmapUVIndex);
	PRIMITIVE_UNIFORM_BUILDER_METHOD(uint32,			LightmapDataIndex);

#undef PRIMITIVE_UNIFORM_BUILDER_FLAG_METHOD
#undef PRIMITIVE_UNIFORM_BUILDER_METHOD

	inline FPrimitiveUniformShaderParametersBuilder& LightingChannelMask(uint32 InLightingChannelMask)
	{
		LightingChannels = InLightingChannelMask;
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& BoundsScale(float InBoundsScale)
	{
		Parameters.BoundsScale = InBoundsScale;
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& ObjectBounds(const FVector3f& InObjectBounds)
	{
		Parameters.ObjectBoundsX = InObjectBounds.X;
		Parameters.ObjectBoundsY = InObjectBounds.Y;
		Parameters.ObjectBoundsZ = InObjectBounds.Z;
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& WorldBounds(const FBoxSphereBounds& InWorldBounds)
	{
		AbsoluteObjectWorldPosition = InWorldBounds.Origin;
		ObjectRadius = static_cast<float>(InWorldBounds.SphereRadius);											//LWC_TODO: Precision loss
		Parameters.ObjectBoundsX = static_cast<float>(InWorldBounds.BoxExtent.X);
		Parameters.ObjectBoundsY = static_cast<float>(InWorldBounds.BoxExtent.Y);
		Parameters.ObjectBoundsZ = static_cast<float>(InWorldBounds.BoxExtent.Z);
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& LocalBounds(const FBoxSphereBounds& InLocalBounds)
	{
		Parameters.LocalObjectBoundsMin = FVector3f(InLocalBounds.GetBoxExtrema(0)); // 0 == minimum		//LWC_TODO: Precision loss
		Parameters.LocalObjectBoundsMax = FVector3f(InLocalBounds.GetBoxExtrema(1)); // 1 == maximum		//LWC_TODO: Precision loss
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& PreSkinnedLocalBounds(const FBoxSphereBounds& InPreSkinnedLocalBounds)
	{
		bHasPreSkinnedLocalBounds = true;
		Parameters.PreSkinnedLocalBoundsMin = FVector3f(InPreSkinnedLocalBounds.GetBoxExtrema(0)); // 0 == minimum		//LWC_TODO: Precision loss
		Parameters.PreSkinnedLocalBoundsMax = FVector3f(InPreSkinnedLocalBounds.GetBoxExtrema(1)); // 1 == maximum		//LWC_TODO: Precision loss
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& ActorWorldPosition(const FVector& InActorWorldPosition)
	{
		AbsoluteActorWorldPosition = InActorWorldPosition;
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& LocalToWorld(const FMatrix& InLocalToWorld)
	{
		AbsoluteLocalToWorld = InLocalToWorld;
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& InstanceLocalBounds(const FRenderBounds& InInstanceLocalBounds)
	{
		bHasInstanceLocalBounds = true;
		Parameters.InstanceLocalBoundsCenter = InInstanceLocalBounds.GetCenter();
		Parameters.InstanceLocalBoundsExtent = InInstanceLocalBounds.GetExtent();
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& PreviousLocalToWorld(const FMatrix& InPreviousLocalToWorld)
	{
		bHasPreviousLocalToWorld = true;
		AbsolutePreviousLocalToWorld = InPreviousLocalToWorld;
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& EditorColors(const FLinearColor& InWireframeColor, const FLinearColor& InLevelColor)
	{
		Parameters.WireframeColor = FVector3f(InWireframeColor.R, InWireframeColor.G, InWireframeColor.B);
		Parameters.LevelColor = FVector3f(InLevelColor.R, InLevelColor.G, InLevelColor.B);
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& CustomPrimitiveData(const FCustomPrimitiveData* InCustomPrimitiveData)
	{
		// If this primitive has custom primitive data, set it
		if (InCustomPrimitiveData)
		{
			// Copy at most up to the max supported number of floats for safety
			FMemory::Memcpy(
				&Parameters.CustomPrimitiveData,
				InCustomPrimitiveData->Data.GetData(),
				InCustomPrimitiveData->Data.GetTypeSize() * FMath::Min(InCustomPrimitiveData->Data.Num(),
				FCustomPrimitiveData::NumCustomPrimitiveDataFloats)
			);

			bHasCustomData = true;
		}
		else
		{
			// Clear to 0
			FMemory::Memzero(Parameters.CustomPrimitiveData);
			bHasCustomData = false;
		}

		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& NaniteImposterIndex(uint32 ImposterIndex)
	{
		bHasNaniteImposter = ImposterIndex != INDEX_NONE;

		check(!bHasNaniteImposter || ImposterIndex < NANITE_IMPOSTER_INDEX_MASK);
		Parameters.PackedNaniteFlags = (Parameters.PackedNaniteFlags & NANITE_FILTER_FLAGS_MASK) | (ImposterIndex & NANITE_IMPOSTER_INDEX_MASK);

		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& NaniteFilterFlags(uint32 FilterFlags)
	{
		check(FilterFlags < (1u << NANITE_FILTER_FLAGS_NUM_BITS));
		Parameters.PackedNaniteFlags = (FilterFlags << NANITE_IMPOSTER_INDEX_NUM_BITS) | (Parameters.PackedNaniteFlags & NANITE_IMPOSTER_INDEX_MASK);

		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& InstanceDrawDistance(FVector2f DistanceMinMax)
	{
		// Only scale the far distance by scalability parameters
		DistanceMinMax.Y *= GetCachedScalabilityCVars().ViewDistanceScale;
		Parameters.InstanceDrawDistanceMinMaxSquared = FMath::Square(DistanceMinMax);
		bHasInstanceDrawDistanceCull = true;
		return *this;
	}

	inline FPrimitiveUniformShaderParametersBuilder& InstanceWorldPositionOffsetDisableDistance(float WPODisableDistance)
	{
		WPODisableDistance *= GetCachedScalabilityCVars().ViewDistanceScale;
		bHasWPODisableDistance = true;
		Parameters.InstanceWPODisableDistanceSquared = WPODisableDistance * WPODisableDistance;

		return *this;
	}

	inline const FPrimitiveUniformShaderParameters& Build()
	{
		const FLargeWorldRenderPosition AbsoluteWorldPosition(AbsoluteLocalToWorld.GetOrigin());
		const FVector TilePositionOffset = AbsoluteWorldPosition.GetTileOffset();

		Parameters.TilePosition = AbsoluteWorldPosition.GetTile();

		{
			// Inverse on FMatrix44f can generate NaNs if the source matrix contains large scaling, so do it in double precision.
			FMatrix LocalToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrixDouble(TilePositionOffset, AbsoluteLocalToWorld);
			Parameters.LocalToRelativeWorld = FMatrix44f(LocalToRelativeWorld);
			Parameters.RelativeWorldToLocal = FMatrix44f(LocalToRelativeWorld.Inverse());
		}

		Parameters.ActorRelativeWorldPosition = FVector3f(AbsoluteActorWorldPosition - TilePositionOffset);	//LWC_TODO: Precision loss
		const FVector3f ObjectRelativeWorldPositionAsFloat = FVector3f(AbsoluteObjectWorldPosition - TilePositionOffset);
		Parameters.ObjectRelativeWorldPositionAndRadius = FVector4f(ObjectRelativeWorldPositionAsFloat, ObjectRadius);

		if (bHasPreviousLocalToWorld)
		{
			// Inverse on FMatrix44f can generate NaNs if the source matrix contains large scaling, so do it in double precision.
			FMatrix PrevLocalToRelativeWorld = FLargeWorldRenderScalar::MakeClampedToRelativeWorldMatrixDouble(TilePositionOffset, AbsolutePreviousLocalToWorld);
			Parameters.PreviousLocalToRelativeWorld = FMatrix44f(PrevLocalToRelativeWorld);
			Parameters.PreviousRelativeWorldToLocal = FMatrix44f(PrevLocalToRelativeWorld.Inverse());
		}
		else
		{
			Parameters.PreviousLocalToRelativeWorld = Parameters.LocalToRelativeWorld;
			Parameters.PreviousRelativeWorldToLocal = Parameters.RelativeWorldToLocal;
		}

		if (!bHasInstanceLocalBounds)
		{
			FRenderBounds InstanceLocalBounds(Parameters.LocalObjectBoundsMin, Parameters.LocalObjectBoundsMax);
			Parameters.InstanceLocalBoundsCenter = InstanceLocalBounds.GetCenter();
			Parameters.InstanceLocalBoundsExtent = InstanceLocalBounds.GetExtent();
		}

		if (!bHasPreSkinnedLocalBounds)
		{
			Parameters.PreSkinnedLocalBoundsMin = Parameters.LocalObjectBoundsMin;
			Parameters.PreSkinnedLocalBoundsMax = Parameters.LocalObjectBoundsMax;
		}

		Parameters.ObjectOrientation = Parameters.LocalToRelativeWorld.GetUnitAxis(EAxis::Z);

		{
			// Extract per axis scales from LocalToWorld transform
			FVector4f WorldX = FVector4f(Parameters.LocalToRelativeWorld.M[0][0], Parameters.LocalToRelativeWorld.M[0][1], Parameters.LocalToRelativeWorld.M[0][2], 0);
			FVector4f WorldY = FVector4f(Parameters.LocalToRelativeWorld.M[1][0], Parameters.LocalToRelativeWorld.M[1][1], Parameters.LocalToRelativeWorld.M[1][2], 0);
			FVector4f WorldZ = FVector4f(Parameters.LocalToRelativeWorld.M[2][0], Parameters.LocalToRelativeWorld.M[2][1], Parameters.LocalToRelativeWorld.M[2][2], 0);
			float ScaleX = FVector3f(WorldX).Size();
			float ScaleY = FVector3f(WorldY).Size();
			float ScaleZ = FVector3f(WorldZ).Size();
			Parameters.NonUniformScale = FVector4f(ScaleX, ScaleY, ScaleZ, FMath::Max3(FMath::Abs(ScaleX), FMath::Abs(ScaleY), FMath::Abs(ScaleZ)));
			Parameters.InvNonUniformScale = FVector3f(
				ScaleX > UE_KINDA_SMALL_NUMBER ? 1.0f / ScaleX : 0.0f,
				ScaleY > UE_KINDA_SMALL_NUMBER ? 1.0f / ScaleY : 0.0f,
				ScaleZ > UE_KINDA_SMALL_NUMBER ? 1.0f / ScaleZ : 0.0f);
		}

		// If SingleCaptureIndex is invalid, set it to 0 since there will be a default cubemap at that slot
		Parameters.SingleCaptureIndex = FMath::Max(Parameters.SingleCaptureIndex, 0);

		Parameters.Flags = 0;
		Parameters.Flags |= bReceivesDecals ? PRIMITIVE_SCENE_DATA_FLAG_DECAL_RECEIVER : 0u;
		Parameters.Flags |= bHasCapsuleRepresentation ? PRIMITIVE_SCENE_DATA_FLAG_HAS_CAPSULE_REPRESENTATION : 0u;
		Parameters.Flags |= bUseSingleSampleShadowFromStationaryLights ? PRIMITIVE_SCENE_DATA_FLAG_USE_SINGLE_SAMPLE_SHADOW_SL : 0u;
		Parameters.Flags |= (bUseVolumetricLightmap && bUseSingleSampleShadowFromStationaryLights) ? PRIMITIVE_SCENE_DATA_FLAG_USE_VOLUMETRIC_LM_SHADOW_SL : 0u;
		Parameters.Flags |= bCacheShadowAsStatic ? PRIMITIVE_SCENE_DATA_FLAG_CACHE_SHADOW_AS_STATIC : 0u;
		Parameters.Flags |= bOutputVelocity ? PRIMITIVE_SCENE_DATA_FLAG_OUTPUT_VELOCITY : 0u;
		Parameters.Flags |= bEvaluateWorldPositionOffset ? PRIMITIVE_SCENE_DATA_FLAG_EVALUATE_WORLD_POSITION_OFFSET : 0u;
		Parameters.Flags |= (Parameters.LocalToRelativeWorld.RotDeterminant() < 0.0f) ? PRIMITIVE_SCENE_DATA_FLAG_DETERMINANT_SIGN : 0u;
		Parameters.Flags |= bHasCustomData ? PRIMITIVE_SCENE_DATA_FLAG_HAS_PRIMITIVE_CUSTOM_DATA : 0u;
		Parameters.Flags |= ((LightingChannels & 0x1) != 0) ? PRIMITIVE_SCENE_DATA_FLAG_LIGHTING_CHANNEL_0 : 0u;
		Parameters.Flags |= ((LightingChannels & 0x2) != 0) ? PRIMITIVE_SCENE_DATA_FLAG_LIGHTING_CHANNEL_1 : 0u;
		Parameters.Flags |= ((LightingChannels & 0x4) != 0) ? PRIMITIVE_SCENE_DATA_FLAG_LIGHTING_CHANNEL_2 : 0u;
		Parameters.Flags |= bHasNaniteImposter ? PRIMITIVE_SCENE_DATA_FLAG_HAS_NANITE_IMPOSTER : 0u;
		Parameters.Flags |= bHasInstanceLocalBounds ? PRIMITIVE_SCENE_DATA_FLAG_HAS_INSTANCE_LOCAL_BOUNDS : 0u;
		Parameters.Flags |= bCastShadow ? PRIMITIVE_SCENE_DATA_FLAG_CAST_SHADOWS : 0u;
		Parameters.Flags |= bCastContactShadow ? PRIMITIVE_SCENE_DATA_FLAG_HAS_CAST_CONTACT_SHADOW : 0u;
		Parameters.Flags |= bCastHiddenShadow ? PRIMITIVE_SCENE_DATA_FLAG_CAST_HIDDEN_SHADOW : 0u;
		Parameters.Flags |= bVisibleInGame ? PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_GAME : 0u;
	#if WITH_EDITOR
		Parameters.Flags |= bVisibleInEditor ? PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_EDITOR : 0u;
	#endif
		Parameters.Flags |= bVisibleInReflectionCaptures ? PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_REFLECTION_CAPTURES : 0u;
		Parameters.Flags |= bVisibleInRealTimeSkyCaptures ? PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_REAL_TIME_SKY_CAPTURES : 0u;
		Parameters.Flags |= bVisibleInRayTracing ? PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_RAY_TRACING : 0u;
		Parameters.Flags |= bVisibleInSceneCaptureOnly ? PRIMITIVE_SCENE_DATA_FLAG_VISIBLE_IN_SCENE_CAPTURE_ONLY : 0u;
		Parameters.Flags |= bHiddenInSceneCapture ? PRIMITIVE_SCENE_DATA_FLAG_HIDDEN_IN_SCENE_CAPTURE : 0u;
		Parameters.Flags |= bForceHidden ? PRIMITIVE_SCENE_DATA_FLAG_FORCE_HIDDEN : 0u;
		Parameters.Flags |= bHasInstanceDrawDistanceCull ? PRIMITIVE_SCENE_DATA_FLAG_INSTANCE_DRAW_DISTANCE_CULL : 0u;
		Parameters.Flags |= bHasWPODisableDistance ? PRIMITIVE_SCENE_DATA_FLAG_WPO_DISABLE_DISTANCE : 0u;
		return Parameters;
	}

private:
	FPrimitiveUniformShaderParameters Parameters;
	
	FMatrix AbsoluteLocalToWorld;
	FMatrix AbsolutePreviousLocalToWorld;
	FVector AbsoluteObjectWorldPosition;
	FVector AbsoluteActorWorldPosition;

	float ObjectRadius;

	uint32 LightingChannels : 3;
	uint32 bReceivesDecals : 1;
	uint32 bUseSingleSampleShadowFromStationaryLights : 1;
	uint32 bUseVolumetricLightmap : 1;
	uint32 bCacheShadowAsStatic : 1;
	uint32 bOutputVelocity : 1;
	uint32 bEvaluateWorldPositionOffset : 1;
	uint32 bCastShadow : 1;
	uint32 bCastContactShadow : 1;
	uint32 bCastHiddenShadow : 1;
	uint32 bHasCapsuleRepresentation : 1;
	uint32 bHasPreSkinnedLocalBounds : 1;
	uint32 bHasInstanceLocalBounds : 1;
	uint32 bHasCustomData : 1;
	uint32 bHasPreviousLocalToWorld : 1;
	uint32 bVisibleInGame : 1;
	uint32 bVisibleInEditor : 1;
	uint32 bVisibleInReflectionCaptures : 1;
	uint32 bVisibleInRealTimeSkyCaptures : 1;
	uint32 bVisibleInRayTracing : 1;
	uint32 bVisibleInSceneCaptureOnly : 1;
	uint32 bHiddenInSceneCapture : 1;
	uint32 bForceHidden : 1;
	uint32 bHasNaniteImposter : 1;
	uint32 bHasInstanceDrawDistanceCull : 1;
	uint32 bHasWPODisableDistance : 1;
};

inline TUniformBufferRef<FPrimitiveUniformShaderParameters> CreatePrimitiveUniformBufferImmediate(
	const FMatrix& LocalToWorld,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	const FBoxSphereBounds& PreSkinnedLocalBounds,
	bool bReceivesDecals,
	bool bOutputVelocity
)
{
	check(IsInRenderingThread());
	return TUniformBufferRef<FPrimitiveUniformShaderParameters>::CreateUniformBufferImmediate(
		FPrimitiveUniformShaderParametersBuilder{}
		.Defaults()
			.LocalToWorld(LocalToWorld)
			.ActorWorldPosition(WorldBounds.Origin)
			.WorldBounds(WorldBounds)
			.LocalBounds(LocalBounds)
			.PreSkinnedLocalBounds(PreSkinnedLocalBounds)
			.ReceivesDecals(bReceivesDecals)
			.OutputVelocity(bOutputVelocity)
		.Build(),
		UniformBuffer_MultiFrame
	);
}

inline FPrimitiveUniformShaderParameters GetIdentityPrimitiveParameters()
{
	// Don't use FMatrix44f::Identity here as GetIdentityPrimitiveParameters is used by TGlobalResource<FIdentityPrimitiveUniformBuffer> and because
	// static initialization order is undefined, FMatrix44f::Identity might be all 0's or random data the first time this is called.
	return FPrimitiveUniformShaderParametersBuilder{}
		.Defaults()
			.LocalToWorld(FMatrix(FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 1)))
			.ActorWorldPosition(FVector(0.0, 0.0, 0.0))
			.WorldBounds(FBoxSphereBounds(EForceInit::ForceInit))
			.LocalBounds(FBoxSphereBounds(EForceInit::ForceInit))
		.Build();
}

/**
 * Primitive uniform buffer containing only identity transforms.
 */
class FIdentityPrimitiveUniformBuffer : public TUniformBuffer<FPrimitiveUniformShaderParameters>
{
public:

	/** Default constructor. */
	FIdentityPrimitiveUniformBuffer()
	{
		SetContents(GetIdentityPrimitiveParameters());
	}
};

/** Global primitive uniform buffer resource containing identity transformations. */
extern ENGINE_API TGlobalResource<FIdentityPrimitiveUniformBuffer> GIdentityPrimitiveUniformBuffer;

struct FPrimitiveSceneShaderData
{
	// Must match PRIMITIVE_SCENE_DATA_STRIDE in SceneData.ush
	enum { DataStrideInFloat4s = 42 };

	TStaticArray<FVector4f, DataStrideInFloat4s> Data;

	FPrimitiveSceneShaderData()
		: Data(InPlace, NoInit)
	{
		static_assert(FPrimitiveSceneShaderData::DataStrideInFloat4s == FScatterUploadBuffer::PrimitiveDataStrideInFloat4s,"");
		Setup(GetIdentityPrimitiveParameters());
	}

	explicit FPrimitiveSceneShaderData(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters)
		: Data(InPlace, NoInit)
	{
		Setup(PrimitiveUniformShaderParameters);
	}

	ENGINE_API FPrimitiveSceneShaderData(const class FPrimitiveSceneProxy* RESTRICT Proxy);

	ENGINE_API void Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters);
};

class FSinglePrimitiveStructured : public FRenderResource
{
public:

	FSinglePrimitiveStructured()
		: ShaderPlatform(SP_NumPlatforms)
	{}

	ENGINE_API virtual void InitRHI() override;

	virtual void ReleaseRHI() override
	{
		PrimitiveSceneDataBufferRHI.SafeRelease();
		PrimitiveSceneDataBufferSRV.SafeRelease();
		SkyIrradianceEnvironmentMapRHI.SafeRelease();
		SkyIrradianceEnvironmentMapSRV.SafeRelease();
		InstanceSceneDataBufferRHI.SafeRelease();
		InstanceSceneDataBufferSRV.SafeRelease();
		InstancePayloadDataBufferRHI.SafeRelease();
		InstancePayloadDataBufferSRV.SafeRelease();
		PrimitiveSceneDataTextureRHI.SafeRelease();
		PrimitiveSceneDataTextureSRV.SafeRelease();
		LightmapSceneDataBufferRHI.SafeRelease();
		LightmapSceneDataBufferSRV.SafeRelease();
//#if WITH_EDITOR
		EditorVisualizeLevelInstanceDataBufferRHI.SafeRelease();
		EditorVisualizeLevelInstanceDataBufferSRV.SafeRelease();

		EditorSelectedDataBufferRHI.SafeRelease();
		EditorSelectedDataBufferSRV.SafeRelease();
//#endif
	}

	ENGINE_API void UploadToGPU();

	EShaderPlatform ShaderPlatform=SP_NumPlatforms;

	FPrimitiveSceneShaderData PrimitiveSceneData;
	FInstanceSceneShaderData InstanceSceneData;
	FLightmapSceneShaderData LightmapSceneData;

	FBufferRHIRef PrimitiveSceneDataBufferRHI;
	FShaderResourceViewRHIRef PrimitiveSceneDataBufferSRV;

	FBufferRHIRef SkyIrradianceEnvironmentMapRHI;
	FShaderResourceViewRHIRef SkyIrradianceEnvironmentMapSRV;

	FBufferRHIRef InstanceSceneDataBufferRHI;
	FShaderResourceViewRHIRef InstanceSceneDataBufferSRV;

	FBufferRHIRef InstancePayloadDataBufferRHI;
	FShaderResourceViewRHIRef InstancePayloadDataBufferSRV;

	FTextureRHIRef PrimitiveSceneDataTextureRHI;
	FShaderResourceViewRHIRef PrimitiveSceneDataTextureSRV;

	FBufferRHIRef LightmapSceneDataBufferRHI;
	FShaderResourceViewRHIRef LightmapSceneDataBufferSRV;

//#if WITH_EDITOR
	FBufferRHIRef EditorVisualizeLevelInstanceDataBufferRHI;
	FShaderResourceViewRHIRef EditorVisualizeLevelInstanceDataBufferSRV;

	FBufferRHIRef EditorSelectedDataBufferRHI;
	FShaderResourceViewRHIRef EditorSelectedDataBufferSRV;
//#endif
};

/**
* Default Primitive data buffer.  
* This is used when the VF is used for rendering outside normal mesh passes, where there is no valid scene.
*/
extern ENGINE_API TGlobalResource<FSinglePrimitiveStructured> GIdentityPrimitiveBuffer;
extern ENGINE_API TGlobalResource<FSinglePrimitiveStructured> GTilePrimitiveBuffer;
