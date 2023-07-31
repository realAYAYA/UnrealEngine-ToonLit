// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceUniformShaderParameters.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveSceneInfo.h"

void FInstanceSceneShaderData::Build
(
	uint32 PrimitiveId,
	uint32 RelativeId,
	uint32 InstanceFlags,
	uint32 LastUpdateFrame,
	uint32 CustomDataCount,
	float RandomID
)
{
	BuildInternal(PrimitiveId, RelativeId, InstanceFlags, LastUpdateFrame, CustomDataCount, RandomID, FRenderTransform::Identity, FRenderTransform::Identity);
}

void FInstanceSceneShaderData::Build
(
	uint32 PrimitiveId,
	uint32 RelativeId,
	uint32 InstanceFlags,
	uint32 LastUpdateFrame,
	uint32 CustomDataCount,
	float RandomID,
	const FRenderTransform& LocalToPrimitive,
	const FRenderTransform& PrimitiveToWorld,
	const FRenderTransform& PrevPrimitiveToWorld // TODO: Temporary PrevVelocityHack
)
{
	FRenderTransform LocalToWorld = LocalToPrimitive * PrimitiveToWorld;
	FRenderTransform PrevLocalToWorld = LocalToPrimitive * PrevPrimitiveToWorld; // TODO: Temporary PrevVelocityHack

	// Remove shear
	LocalToWorld.Orthogonalize();
	PrevLocalToWorld.Orthogonalize(); // TODO: Temporary PrevVelocityHack

	BuildInternal(PrimitiveId, RelativeId, InstanceFlags, LastUpdateFrame, CustomDataCount, RandomID, LocalToWorld, PrevLocalToWorld);
}

void FInstanceSceneShaderData::BuildInternal
(
	uint32 PrimitiveId,
	uint32 RelativeId,
	uint32 InstanceFlags,
	uint32 LastUpdateFrame,
	uint32 CustomDataCount,
	float RandomID,
	const FRenderTransform& LocalToWorld, // Assumes shear has been removed already
	const FRenderTransform& PrevLocalToWorld // Assumes shear has been removed already // TODO: Temporary PrevVelocityHack
)
{
	// Note: layout must match GetInstanceData in SceneData.ush and InitializeInstanceSceneData in GPUSceneWriter.ush

	if (LocalToWorld.RotDeterminant() < 0.0f)
	{
		InstanceFlags |= INSTANCE_SCENE_DATA_FLAG_DETERMINANT_SIGN;
	}
	else
	{
		InstanceFlags &= ~INSTANCE_SCENE_DATA_FLAG_DETERMINANT_SIGN;
	}

	checkSlow((PrimitiveId		& 0x000FFFFF) == PrimitiveId);
	checkSlow((InstanceFlags	& 0x00000FFF) == InstanceFlags);
	checkSlow((RelativeId		& 0x00FFFFFF) == RelativeId);
	checkSlow((CustomDataCount	& 0x000000FF) == CustomDataCount);

	const uint32 Packed0 = (InstanceFlags   << 20u) | PrimitiveId;
	const uint32 Packed1 = (CustomDataCount << 24u) | RelativeId;

	Data[0].X  = *(const float*)&Packed0;
	Data[0].Y  = *(const float*)&Packed1;
	Data[0].Z  = *(const float*)&LastUpdateFrame;
	Data[0].W  = *(const float*)&RandomID;

	// TODO: Temporary PrevVelocityHack
	if (FDataDrivenShaderPlatformInfo::GetSupportSceneDataCompressedTransforms(GMaxRHIShaderPlatform))
	{
		FCompressedTransform CompressedLocalToWorld(LocalToWorld);
		Data[1] = *(const FVector4f*)&CompressedLocalToWorld.Rotation[0];
		Data[2] = *(const FVector3f*)&CompressedLocalToWorld.Translation;

		FCompressedTransform CompressedPrevLocalToWorld(PrevLocalToWorld);
		Data[3] = *(const FVector4f*)&CompressedPrevLocalToWorld.Rotation[0];
		Data[4] = *(const FVector3f*)&CompressedPrevLocalToWorld.Translation;
	}
	else
	{
		// Note: writes 3x float4s
		LocalToWorld.To3x4MatrixTranspose((float*)&Data[1]);
		PrevLocalToWorld.To3x4MatrixTranspose((float*)&Data[4]);
	}
}
