// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFAnalyticsBuilder.h"
#include "Converters/GLTFAccessorConverters.h"
#include "Converters/GLTFMeshConverters.h"
#include "Converters/GLTFMeshDataConverters.h"
#include "Converters/GLTFMaterialConverters.h"
#include "Converters/GLTFSamplerConverters.h"
#include "Converters/GLTFTextureConverters.h"
#include "Converters/GLTFImageConverters.h"
#include "Converters/GLTFNodeConverters.h"
#include "Converters/GLTFSkinConverters.h"
#include "Converters/GLTFAnimationConverters.h"
#include "Converters/GLTFSceneConverters.h"
#include "Converters/GLTFCameraConverters.h"
#include "Converters/GLTFLightConverters.h"
#include "Converters/GLTFMaterialVariantConverters.h"

class UMeshComponent;
class UPropertyValue;
class FGLTFNormalArray;
class FGLTFUVArray;

class GLTFEXPORTER_API FGLTFConvertBuilder : public FGLTFAnalyticsBuilder
{
public:

	const TSet<AActor*> SelectedActors;

	FGLTFConvertBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr, const TSet<AActor*>& SelectedActors = {});

	bool IsSelectedActor(const AActor* Object) const;
	bool IsRootActor(const AActor* Actor) const;

	FGLTFJsonAccessor* AddUniquePositionAccessor(const FPositionVertexBuffer* VertexBuffer);
	FGLTFJsonAccessor* AddUniquePositionAccessor(const FGLTFMeshSection* MeshSection, const FPositionVertexBuffer* VertexBuffer);
	FGLTFJsonAccessor* AddUniqueColorAccessor(const FGLTFMeshSection* MeshSection, const FColorVertexBuffer* VertexBuffer);
	FGLTFJsonAccessor* AddUniqueNormalAccessor(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer);
	FGLTFJsonAccessor* AddUniqueNormalAccessor(const FGLTFNormalArray* Normals);
	FGLTFJsonAccessor* AddUniqueTangentAccessor(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer);
	FGLTFJsonAccessor* AddUniqueUVAccessor(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer, int32 UVIndex);
	FGLTFJsonAccessor* AddUniqueUVAccessor(const FGLTFUVArray* UVs); /* Supports single UV channel */
	FGLTFJsonAccessor* AddUniqueJointAccessor(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, int32 InfluenceOffset);
	FGLTFJsonAccessor* AddUniqueWeightAccessor(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, int32 InfluenceOffset);
	FGLTFJsonAccessor* AddUniqueIndexAccessor(const FGLTFMeshSection* MeshSection);
	FGLTFJsonAccessor* AddUniqueIndexAccessor(const FGLTFIndexArray* IndexBuffer, const FString& MeshName);

	FGLTFJsonMesh* AddUniqueMesh(const UStaticMesh* StaticMesh, const FGLTFMaterialArray& Materials = {}, int32 LODIndex = INDEX_NONE);
	FGLTFJsonMesh* AddUniqueMesh(const USkeletalMesh* SkeletalMesh, const FGLTFMaterialArray& Materials = {}, int32 LODIndex = INDEX_NONE);
	FGLTFJsonMesh* AddUniqueMesh(const UMeshComponent* MeshComponent, const FGLTFMaterialArray& Materials = {}, int32 LODIndex = INDEX_NONE);
	FGLTFJsonMesh* AddUniqueMesh(const UStaticMeshComponent* StaticMeshComponent, const FGLTFMaterialArray& Materials = {}, int32 LODIndex = INDEX_NONE);
	FGLTFJsonMesh* AddUniqueMesh(const USkeletalMeshComponent* SkeletalMeshComponent, const FGLTFMaterialArray& Materials = {}, int32 LODIndex = INDEX_NONE);
	FGLTFJsonMesh* AddUniqueMesh(const USplineMeshComponent* SplineMeshComponent, const FGLTFMaterialArray& Materials = {}, int32 LODIndex = INDEX_NONE);
	FGLTFJsonMesh* AddUniqueMesh(const ULandscapeComponent* LandscapeComponent, const UMaterialInterface* LandscapeMaterial = nullptr);

	const FGLTFMeshData* AddUniqueMeshData(const UStaticMesh* StaticMesh, const UStaticMeshComponent* StaticMeshComponent = nullptr, int32 LODIndex = INDEX_NONE);
	const FGLTFMeshData* AddUniqueMeshData(const USkeletalMesh* SkeletalMesh, const USkeletalMeshComponent* SkeletalMeshComponent = nullptr, int32 LODIndex = INDEX_NONE);

	FGLTFJsonMaterial* AddUniqueMaterial(const UMaterialInterface* Material, const UStaticMesh* StaticMesh, int32 LODIndex = INDEX_NONE, int32 MaterialIndex = INDEX_NONE);
	FGLTFJsonMaterial* AddUniqueMaterial(const UMaterialInterface* Material, const USkeletalMesh* SkeletalMesh, int32 LODIndex = INDEX_NONE, int32 MaterialIndex = INDEX_NONE);
	FGLTFJsonMaterial* AddUniqueMaterial(const UMaterialInterface* Material, const UMeshComponent* MeshComponent, int32 LODIndex = INDEX_NONE, int32 MaterialIndex = INDEX_NONE);
	FGLTFJsonMaterial* AddUniqueMaterial(const UMaterialInterface* Material, const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex = INDEX_NONE, int32 MaterialIndex = INDEX_NONE);
	FGLTFJsonMaterial* AddUniqueMaterial(const UMaterialInterface* Material, const USkeletalMeshComponent* SkeletalMeshComponent, int32 LODIndex = INDEX_NONE, int32 MaterialIndex = INDEX_NONE);
	FGLTFJsonMaterial* AddUniqueMaterial(const UMaterialInterface* Material, const FGLTFMeshData* MeshData = nullptr, const FGLTFIndexArray& SectionIndices = {});

	FGLTFJsonSampler* AddUniqueSampler(const UTexture* Texture);
	FGLTFJsonSampler* AddUniqueSampler(TextureAddress Address, TextureFilter Filter, TextureGroup LODGroup = TEXTUREGROUP_MAX);
	FGLTFJsonSampler* AddUniqueSampler(TextureAddress AddressX, TextureAddress AddressY, TextureFilter Filter, TextureGroup LODGroup = TEXTUREGROUP_MAX);

	FGLTFJsonTexture* AddUniqueTexture(const UTexture* Texture);
	FGLTFJsonTexture* AddUniqueTexture(const UTexture2D* Texture);
	FGLTFJsonTexture* AddUniqueTexture(const UTextureRenderTarget2D* Texture);
	FGLTFJsonTexture* AddUniqueTexture(const UTexture* Texture, bool bToSRGB, TextureAddress TextureAddressX = TextureAddress::TA_MAX, TextureAddress TextureAddressY = TextureAddress::TA_MAX);
	FGLTFJsonTexture* AddUniqueTexture(const UTexture2D* Texture, bool bToSRGB, TextureAddress TextureAddressX, TextureAddress TextureAddressY);
	FGLTFJsonTexture* AddUniqueTexture(const UTextureRenderTarget2D* Texture, bool bToSRGB);
	FGLTFJsonImage* AddUniqueImage(TGLTFSharedArray<FColor>& Pixels, FIntPoint Size, bool bIgnoreAlpha, const FString& Name);

	FGLTFJsonSkin* AddUniqueSkin(FGLTFJsonNode* RootNode, const USkeletalMesh* SkeletalMesh);
	FGLTFJsonSkin* AddUniqueSkin(FGLTFJsonNode* RootNode, const USkeletalMeshComponent* SkeletalMeshComponent);
	FGLTFJsonAnimation* AddUniqueAnimation(FGLTFJsonNode* RootNode, const USkeletalMesh* SkeletalMesh, const UAnimSequence* AnimSequence);
	FGLTFJsonAnimation* AddUniqueAnimation(FGLTFJsonNode* RootNode, const USkeletalMeshComponent* SkeletalMeshComponent);
	FGLTFJsonAnimation* AddUniqueAnimation(const ULevel* Level, const ULevelSequence* LevelSequence);
	FGLTFJsonAnimation* AddUniqueAnimation(const ALevelSequenceActor* LevelSequenceActor);

	FGLTFJsonNode* AddUniqueNode(const AActor* Actor);
	FGLTFJsonNode* AddUniqueNode(const USceneComponent* SceneComponent);
	FGLTFJsonNode* AddUniqueNode(const USceneComponent* SceneComponent, FName SocketName);
	FGLTFJsonNode* AddUniqueNode(FGLTFJsonNode* RootNode, const UStaticMesh* StaticMesh, FName SocketName);
	FGLTFJsonNode* AddUniqueNode(FGLTFJsonNode* RootNode, const USkeletalMesh* SkeletalMesh, FName SocketName);
	FGLTFJsonNode* AddUniqueNode(FGLTFJsonNode* RootNode, const USkeletalMesh* SkeletalMesh, int32 BoneIndex);
	FGLTFJsonScene* AddUniqueScene(const UWorld* World);

	FGLTFJsonCamera* AddUniqueCamera(const UCameraComponent* CameraComponent);
	FGLTFJsonLight* AddUniqueLight(const ULightComponent* LightComponent);

	FGLTFJsonMaterialVariant* AddUniqueMaterialVariant(const UVariant* Variant);

	void RegisterObjectVariant(const UObject* Object, const UPropertyValue* Property);
	const TArray<const UPropertyValue*>* GetObjectVariants(const UObject* Object) const;

	TUniquePtr<IGLTFPositionBufferConverterRaw> PositionBufferConverterRaw = MakeUnique<FGLTFPositionBufferConverterRaw>(*this);
	TUniquePtr<IGLTFNormalBufferConverterRaw> NormalBufferConverterRaw = MakeUnique<FGLTFNormalBufferConverterRaw>(*this);
	TUniquePtr<IGLTFUVBufferConverterRaw> UVBufferConverterRaw = MakeUnique<FGLTFUVBufferConverterRaw>(*this);
	TUniquePtr<IGLTFIndexBufferConverterRaw> IndexBufferConverterRaw = MakeUnique<FGLTFIndexBufferConverterRaw>(*this);
	TUniquePtr<IGLTFSplineMeshConverter> SplineMeshConverter = MakeUnique<FGLTFSplineMeshConverter>(*this);
	TUniquePtr<IGLTFLandscapeMeshConverter> LandscapeConverter = MakeUnique<FGLTFLandscapeMeshConverter>(*this);

	TUniquePtr<IGLTFPositionBufferConverter> PositionBufferConverter = MakeUnique<FGLTFPositionBufferConverter>(*this);
	TUniquePtr<IGLTFColorBufferConverter> ColorBufferConverter = MakeUnique<FGLTFColorBufferConverter>(*this);
	TUniquePtr<IGLTFNormalBufferConverter> NormalBufferConverter = MakeUnique<FGLTFNormalBufferConverter>(*this);
	TUniquePtr<IGLTFTangentBufferConverter> TangentBufferConverter = MakeUnique<FGLTFTangentBufferConverter>(*this);
	TUniquePtr<IGLTFUVBufferConverter> UVBufferConverter = MakeUnique<FGLTFUVBufferConverter>(*this);
	TUniquePtr<IGLTFBoneIndexBufferConverter> BoneIndexBufferConverter = MakeUnique<FGLTFBoneIndexBufferConverter>(*this);
	TUniquePtr<IGLTFBoneWeightBufferConverter> BoneWeightBufferConverter = MakeUnique<FGLTFBoneWeightBufferConverter>(*this);
	TUniquePtr<IGLTFIndexBufferConverter> IndexBufferConverter = MakeUnique<FGLTFIndexBufferConverter>(*this);

	TUniquePtr<IGLTFStaticMeshConverter> StaticMeshConverter = MakeUnique<FGLTFStaticMeshConverter>(*this);
	TUniquePtr<IGLTFSkeletalMeshConverter> SkeletalMeshConverter = MakeUnique<FGLTFSkeletalMeshConverter>(*this);

	TUniquePtr<IGLTFMaterialConverter> MaterialConverter = MakeUnique<FGLTFMaterialConverter>(*this);
	TUniquePtr<IGLTFStaticMeshDataConverter> StaticMeshDataConverter = MakeUnique<FGLTFStaticMeshDataConverter>(*this);
	TUniquePtr<IGLTFSkeletalMeshDataConverter> SkeletalMeshDataConverter = MakeUnique<FGLTFSkeletalMeshDataConverter>(*this);

	TUniquePtr<IGLTFSamplerConverter> SamplerConverter = MakeUnique<FGLTFSamplerConverter>(*this);

	TUniquePtr<IGLTFTexture2DConverter> Texture2DConverter = MakeUnique<FGLTFTexture2DConverter>(*this);
	TUniquePtr<IGLTFTextureRenderTarget2DConverter> TextureRenderTarget2DConverter = MakeUnique<FGLTFTextureRenderTarget2DConverter>(*this);
	TUniquePtr<IGLTFImageConverter> ImageConverter = MakeUnique<FGLTFImageConverter>(*this);

	TUniquePtr<IGLTFSkinConverter> SkinConverter = MakeUnique<FGLTFSkinConverter>(*this);
	TUniquePtr<IGLTFAnimationConverter> AnimationConverter = MakeUnique<FGLTFAnimationConverter>(*this);
	TUniquePtr<IGLTFAnimationDataConverter> AnimationDataConverter = MakeUnique<FGLTFAnimationDataConverter>(*this);
	TUniquePtr<IGLTFLevelSequenceConverter> LevelSequenceConverter = MakeUnique<FGLTFLevelSequenceConverter>(*this);
	TUniquePtr<IGLTFLevelSequenceDataConverter> LevelSequenceDataConverter = MakeUnique<FGLTFLevelSequenceDataConverter>(*this);

	TUniquePtr<IGLTFActorConverter> ActorConverter = MakeUnique<FGLTFActorConverter>(*this);
	TUniquePtr<IGLTFComponentConverter> ComponentConverter = MakeUnique<FGLTFComponentConverter>(*this);
	TUniquePtr<IGLTFComponentSocketConverter> ComponentSocketConverter = MakeUnique<FGLTFComponentSocketConverter>(*this);
	TUniquePtr<IGLTFStaticSocketConverter> StaticSocketConverter = MakeUnique<FGLTFStaticSocketConverter>(*this);
	TUniquePtr<IGLTFSkeletalSocketConverter> SkeletalSocketConverter = MakeUnique<FGLTFSkeletalSocketConverter>(*this);
	TUniquePtr<IGLTFSkeletalBoneConverter> SkeletalBoneConverter = MakeUnique<FGLTFSkeletalBoneConverter>(*this);
	TUniquePtr<IGLTFSceneConverter> SceneConverter = MakeUnique<FGLTFSceneConverter>(*this);

	TUniquePtr<IGLTFCameraConverter> CameraConverter = MakeUnique<FGLTFCameraConverter>(*this);
	TUniquePtr<IGLTFLightConverter> LightConverter = MakeUnique<FGLTFLightConverter>(*this);
	TUniquePtr<IGLTFMaterialVariantConverter> MaterialVariantConverter = MakeUnique<FGLTFMaterialVariantConverter>(*this);

private:

	TMap<const UObject*, TArray<const UPropertyValue*>> ObjectVariants;
};
