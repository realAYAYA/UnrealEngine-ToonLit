// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Animation/AnimInstance.h"
#include "Engine/StaticMesh.h"
#include "AnimToTextureDataAsset.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

USTRUCT(Blueprintable)
struct ANIMTOTEXTURE_API FAnimToTextureMaterialParamNames
{
	GENERATED_BODY()

	//
	// Scalar Parameters
	//

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName RowsPerFrame;

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName BoneWeightRowsPerFrame;

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName NumFrames;

	//
	// Vector Parameters
	//

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName BoundingBoxMin;

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName BoundingBoxScale;

	//
	// Texture Parameters
	//

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName VertexPositionTexture;

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName VertexNormalTexture;

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName BonePositionTexture;

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName BoneRotationTexture;

	UPROPERTY(BlueprintReadWrite, Category = Default, EditAnywhere)
	FName BoneWeightsTexture;

	// Initialize Names
	FAnimToTextureMaterialParamNames();
};

UENUM(Blueprintable)
enum class EAnimToTextureMode : uint8
{
	/* Position and Normal Per-Vertex */
	Vertex,
	/* Linear Blending Skinnin */
	Bone,
};

UENUM(Blueprintable)
enum class EAnimToTextureBonePrecision : uint8
{
	/* Bone positions and rotations stored in 8 bits */
	EightBits,
	/* Bone positions and rotations stored in 16 bits */
	SixteenBits,
};

USTRUCT(Blueprintable)
struct FAnimSequenceInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite)
	TObjectPtr<UAnimSequence> AnimSequence = nullptr;

	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite)
	bool bLooping = true;

	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite)
	bool bUseCustomRange = false;

	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite, meta = (EditCondition = "bUseCustomRange"))
	int32 StartFrame = 0;

	UPROPERTY(EditAnywhere, Category = Default, BlueprintReadWrite, meta = (EditCondition = "bUseCustomRange"))
	int32 EndFrame = 1;

};

USTRUCT(Blueprintable)
struct FAnimInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Default)
	int32 NumFrames = 0;

	UPROPERTY(VisibleAnywhere, Category = Default)
	int32 AnimStart = 0;

	UPROPERTY(EditAnywhere, Category = Default)
	bool bLooping = true;
};

UCLASS(Blueprintable, BlueprintType)
class ANIMTOTEXTURE_API UAnimToTextureDataAsset : public UPrimaryDataAsset
{
public:
	GENERATED_BODY()

	/**
	* SkeletalMesh to bake animations from.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkeletalMesh", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/**
	* SkeletalMesh LOD to bake.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkeletalMesh", Meta = (DisplayName = "LODIndex"))
	int32 SkeletalLODIndex = 0;

	/**
	* StaticMesh to bake to.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/**
	* StaticMesh LOD to bake to.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh", Meta = (DisplayName = "LODIndex"))
	int32 StaticLODIndex = 0;

	/**
	* StaticMesh UVChannel Index for storing vertex information.
	* Make sure this index does not conflict with the Lightmap UV Index.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh")
	int32 UVChannel = 1;

	// ------------------------------------------------------
	// Texture

	/**
	* Max resolution of the texture.
	* A smaller size will be used if the data fits.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	int32 MaxHeight = 4096;

	/**
	* Max resolution of the texture.
	* A smaller size will be used if the data fits.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	int32 MaxWidth = 4096;

	/**
	* Enforce Power Of Two on texture resolutions.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	bool bEnforcePowerOfTwo = false;

	/**
	* Storage Mode.
	* Vertex: will store per-vertex position and normal.
	* Bone: Will store per-bone position and rotation and per-vertex bone weight. 
	        This is the preferred method if meshes share skeleton.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	EAnimToTextureMode Mode;

	/**
	* Texture for storing vertex positions
	* This is only used on Vertex Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture|Vertex", meta = (EditCondition = "Mode == EAnimToTextureMode::Vertex"))
	TSoftObjectPtr<UTexture2D> VertexPositionTexture;

	/**
	* Texture for storing vertex normals
	* This is only used on Vertex Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture|Vertex", meta = (EditCondition = "Mode == EAnimToTextureMode::Vertex"))
	TSoftObjectPtr<UTexture2D> VertexNormalTexture;

	/**
	* Texture Precision for: BonePosition and BoneRotations.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture|Bone", meta = (EditCondition = "Mode == EAnimToTextureMode::Bone"))
	EAnimToTextureBonePrecision BonePrecision = EAnimToTextureBonePrecision::EightBits;

	/**
	* Texture for storing bone positions
	* This is only used on Bone Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture|Bone", meta = (EditCondition = "Mode == EAnimToTextureMode::Bone"))
	TSoftObjectPtr<UTexture2D> BonePositionTexture;

	/**
	* Texture for storing bone rotations
	* This is only used on Bone Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture|Bone", meta = (EditCondition = "Mode == EAnimToTextureMode::Bone"))
	TSoftObjectPtr<UTexture2D> BoneRotationTexture;

	/**
	* Texture for storing vertex/bone weighting
	* This is only used on Bone Mode
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture|Bone", meta = (EditCondition = "Mode == EAnimToTextureMode::Bone"))
	TSoftObjectPtr<UTexture2D> BoneWeightTexture;

	// ------------------------------------------------------
	// Animation

	/** 
	* Bone used for Rigid Binding. The bone needs to be part of the RawBones. 
	* Sockets and VirtualBones are not supported.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (EditCondition = "Mode == EAnimToTextureMode::Bone"))
	FName AttachToSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float SampleRate = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<FAnimSequenceInfo> AnimSequences;

	// ------------------------------------------------------
	// Info

	/* Total Number of Frames in all animations */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	int32 NumFrames = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info|Vertex", Meta = (DisplayName = "RowsPerFrame"))
	int32 VertexRowsPerFrame = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info|Vertex", Meta = (DisplayName = "MinBBox"))
	FVector VertexMinBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info|Vertex", Meta = (DisplayName = "SizeBBox"))
	FVector VertexSizeBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info|Bone")
	int32 BoneWeightRowsPerFrame = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info|Bone")
	int32 BoneRowsPerFrame = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info|Bone", Meta = (DisplayName = "MinBBox"))
	FVector BoneMinBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info|Bone", Meta = (DisplayName = "SizeBBox"))
	FVector BoneSizeBBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Info")
	TArray<FAnimInfo> Animations;

	UFUNCTION(BlueprintCallable, Category = Default)
	int32 GetIndexFromAnimSequence(const UAnimSequence* Sequence);

	UFUNCTION()
	void Reset();

	// If we weren't in a plugin, we could unify this in a base class
	template<typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer)
	{
		AssetType* ReturnVal = nullptr;
		if (AssetPointer.ToSoftObjectPath().IsValid())
		{
			ReturnVal = AssetPointer.Get();
			if (!ReturnVal)
			{
				AssetType* LoadedAsset = Cast<AssetType>(AssetPointer.ToSoftObjectPath().TryLoad());
				if (ensureMsgf(LoadedAsset, TEXT("Failed to load asset pointer %s"), *AssetPointer.ToString()))
				{
					ReturnVal = LoadedAsset;
				}
			}
		}
		return ReturnVal;
	}

#define AnimToTextureDataAsset_ASSET_ACCESSOR(ClassName, PropertyName) \
	FORCEINLINE ClassName* Get##PropertyName() const { return GetAsset(PropertyName); }

	AnimToTextureDataAsset_ASSET_ACCESSOR(UStaticMesh, StaticMesh);
	AnimToTextureDataAsset_ASSET_ACCESSOR(USkeletalMesh, SkeletalMesh);
	AnimToTextureDataAsset_ASSET_ACCESSOR(UTexture2D, VertexPositionTexture);
	AnimToTextureDataAsset_ASSET_ACCESSOR(UTexture2D, VertexNormalTexture);
	AnimToTextureDataAsset_ASSET_ACCESSOR(UTexture2D, BonePositionTexture);
	AnimToTextureDataAsset_ASSET_ACCESSOR(UTexture2D, BoneRotationTexture);
	AnimToTextureDataAsset_ASSET_ACCESSOR(UTexture2D, BoneWeightTexture);

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Static Mesh"))
	UStaticMesh* BP_GetStaticMesh() { return GetStaticMesh(); }

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Skeletal Mesh"))
	USkeletalMesh* BP_GetSkeletalMesh() { return GetSkeletalMesh(); }

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Bone Position Texture"))
	UTexture2D* BP_GetBonePositionTexture() { return GetBonePositionTexture(); }

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Bone Rotation Texture"))
	UTexture2D* BP_GetBoneRotationTexture() { return GetBoneRotationTexture(); }

	UFUNCTION(BlueprintPure, Category = Default, meta = (DisplayName = "Get Bone Weight Texture"))
	UTexture2D* BP_GetBoneWeightTexture() { return GetBoneWeightTexture(); }
};

FORCEINLINE void UAnimToTextureDataAsset::Reset()
{
	// Common Info.
	this->NumFrames = 0;
	this->Animations.Reset();

	// Vertex Info
	this->VertexRowsPerFrame = 1;
	this->VertexMinBBox = FVector::ZeroVector;
	this->VertexSizeBBox = FVector::ZeroVector;

	// Bone Info
	this->BoneRowsPerFrame = 1;
	this->BoneWeightRowsPerFrame = 1;
	this->BoneMinBBox = FVector::ZeroVector; 
	this->BoneSizeBBox = FVector::ZeroVector;
};