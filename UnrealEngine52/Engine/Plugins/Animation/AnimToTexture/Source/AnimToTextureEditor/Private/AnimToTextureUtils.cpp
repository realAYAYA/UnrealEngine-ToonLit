// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimToTextureUtils.h"

namespace AnimToTexture_Private
{

void DecomposeTransformation(const FTransform& Transform, 
	FVector3f& OutTranslation, FVector4& OutRotation)
{
	// Get Translation
	OutTranslation = (FVector3f)Transform.GetTranslation();

	// Get Rotation 
	const FQuat Quat = Transform.GetRotation();

	FVector Axis;
	float Angle;
	Quat.ToAxisAndAngle(Axis, Angle);

	OutRotation = FVector4(Axis, Angle);
}

void DecomposeTransformations(const TArray<FTransform>& Transforms, 
	TArray<FVector3f>& OutTranslations, TArray<FVector4>& OutRotations)
{
	const int32 NumTransforms = Transforms.Num();
	OutTranslations.SetNumUninitialized(NumTransforms);
	OutRotations.SetNumUninitialized(NumTransforms);

	for (int32 Index = 0; Index < NumTransforms; ++Index)
	{
		DecomposeTransformation(Transforms[Index], OutTranslations[Index], OutRotations[Index]);
	}
};

bool WriteSkinWeightsToTexture(const TArray<VertexSkinWeightFour>& SkinWeights,
	const int32 RowsPerFrame, const int32 Height, const int32 Width, UTexture2D* Texture)
{
	check(Texture);

	const int32 NumVertices = SkinWeights.Num();

	// Allocate PixelData.
	TArray<FLowPrecision::ColorType> Pixels;
	Pixels.Init(FLowPrecision::ColorType::Black, Height * Width);

	for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		const VertexSkinWeightFour& VertexSkinWeight = SkinWeights[VertexIndex];

		// Write Influence
		// NOTE: we are assuming the bone index is under < 256
		Pixels[VertexIndex].R = (uint8)VertexSkinWeight.MeshBoneIndices[0];
		Pixels[VertexIndex].G = (uint8)VertexSkinWeight.MeshBoneIndices[1];
		Pixels[VertexIndex].B = (uint8)VertexSkinWeight.MeshBoneIndices[2];
		Pixels[VertexIndex].A = (uint8)VertexSkinWeight.MeshBoneIndices[3];
			
		// Write Weight
		Pixels[Width * RowsPerFrame + VertexIndex].R = VertexSkinWeight.BoneWeights[0];
		Pixels[Width * RowsPerFrame + VertexIndex].G = VertexSkinWeight.BoneWeights[1];
		Pixels[Width * RowsPerFrame + VertexIndex].B = VertexSkinWeight.BoneWeights[2];
		Pixels[Width * RowsPerFrame + VertexIndex].A = VertexSkinWeight.BoneWeights[3];
	};

	// Write to Texture
	return WriteToTexture<FLowPrecision>(Texture, Height, Width, Pixels);
}

} // end namespace AnimToTexture_Private