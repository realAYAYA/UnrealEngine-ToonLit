// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rendering/SkinWeightVertexBuffer.h"
#include "PerPlatformProperties.h"
#include "UObject/NameTypes.h"
#include "Misc/CoreStats.h"
#include "RenderingThread.h"
#include "HAL/UnrealMemory.h"
#include "BoneIndices.h"
#include "RHIGPUReadback.h"
#include "Templates/UniquePtr.h"

#include "SkinWeightProfile.generated.h"

class USkeletalMesh;
class UDebugSkelMeshComponent;

namespace SkeletalMeshImportData
{
	struct FVertInfluence;
}

extern ENGINE_API int32 GSkinWeightProfilesLoadByDefaultMode;
extern ENGINE_API int32 GSkinWeightProfilesDefaultLODOverride;
extern ENGINE_API int32 GSkinWeightProfilesAllowedFromLOD;

extern ENGINE_API FAutoConsoleVariableRef CVarSkinWeightsLoadByDefaultMode;
extern ENGINE_API FAutoConsoleVariableRef CVarSkinWeightProfilesDefaultLODOverride;
extern ENGINE_API FAutoConsoleVariableRef CVarSkinWeightProfilesAllowedFromLOD;

/** Structure storing user facing properties, and is used to identify profiles at the SkeletalMesh level*/
USTRUCT()
struct FSkinWeightProfileInfo
{
	GENERATED_BODY()

	/** Name of the Skin Weight Profile*/
	UPROPERTY(EditAnywhere, Category = SkinWeights)
	FName Name;
	
	/** Whether or not this Profile should be considered the Default loaded for specific LODs rather than the original Skin Weights of the Skeletal Mesh */
	UPROPERTY(EditAnywhere, Category = SkinWeights)
	FPerPlatformBool DefaultProfile;

	/** When DefaultProfile is set any LOD below this LOD Index will override the Skin Weights of the Skeletal Mesh with the Skin Weights from this Profile */
	UPROPERTY(EditAnywhere, Category = SkinWeights, meta=(EditCondition="DefaultProfile", ClampMin=0, DisplayName = "Default Profile from LOD Index"))
	FPerPlatformInt DefaultProfileFromLODIndex;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = SkinWeights)
	TMap<int32, FString> PerLODSourceFiles;
#endif
};

#if WITH_EDITORONLY_DATA

/** Editor only skin weight representation */
struct FRawSkinWeight
{
	// MAX_TOTAL_INFLUENCES for now
	FBoneIndexType InfluenceBones[MAX_TOTAL_INFLUENCES];
	uint16 InfluenceWeights[MAX_TOTAL_INFLUENCES];

	friend FArchive& operator<<(FArchive& Ar, FRawSkinWeight& OverrideEntry);
};

/** Editor only representation of a Skin Weight profile, stored as part of FSkeletalMeshLODModel, used as a base for generating the runtime version (FSkeletalRenderDataSkinWeightProfilesData) */
struct FImportedSkinWeightProfileData
{
	TArray<FRawSkinWeight> SkinWeights;

	//This is the result of the imported data before the chunking
	//We use this data every time we need to re-chunk the skeletal mesh
	TArray<SkeletalMeshImportData::FVertInfluence> SourceModelInfluences;

	friend FArchive& operator<<(FArchive& Ar, FImportedSkinWeightProfileData& ProfileData);
};

#endif // WITH_EDITORONLY_DATA

/** Runtime structure containing the set of override weights and the associated vertex indices */
struct FRuntimeSkinWeightProfileData
{
	/** Structure containing per Skin Weight offset and length */
	struct FSkinWeightOverrideInfo
	{
		/** Offset into FRuntimeSkinWeightOverrideData.Weights */
		uint32 InfluencesOffset;
#if WITH_EDITORONLY_DATA
		/** Number of influences to be read from FRuntimeSkinWeightOverrideData.Weights */
		uint8 NumInfluences_DEPRECATED;
#endif
		friend FArchive& operator<<(FArchive& Ar, FSkinWeightOverrideInfo& OverrideInfo);
	};

	void ApplyOverrides(FSkinWeightVertexBuffer* OverrideBuffer, const uint8* DataBuffer, const int32 NumVerts) const;	
	void ApplyDefaultOverride(FSkinWeightVertexBuffer* Buffer) const;

#if WITH_EDITORONLY_DATA
	/** Per skin weight offset into Weights array and number of weights stored */
	TArray<FSkinWeightOverrideInfo> OverridesInfo_DEPRECATED;
	/** Bulk data containing all Weights, stored as bone id in upper and weight in lower (8) bits */
	TArray<uint16> Weights_DEPRECATED;	
#endif 

	// Either contains FBoneIndexType or uint8 bone indices
	TArray<uint8> BoneIDs;
	TArray<uint8> BoneWeights;
	/** Map between Vertex Indices and the influence offset into BoneIDs/BoneWeights (DEPRECATED and entries of OverridesInfo) */
	TMap<uint32, uint32> VertexIndexToInfluenceOffset;

	uint8 NumWeightsPerVertex;
	bool b16BitBoneIndices;
	
	friend FArchive& operator<<(FArchive& Ar, FRuntimeSkinWeightProfileData& OverrideData);
};

struct FSkinweightReadbackData
{
	TUniquePtr<FRHIGPUBufferReadback> BufferReadback;
	TArray<uint8> ReadbackData;
	uint32 ReadbackFinishedFrameIndex;
};

/** Runtime structure for keeping track of skin weight profile(s) and the associated buffer */
struct FSkinWeightProfilesData
{
	FSkinWeightProfilesData() : BaseBuffer(nullptr), DefaultOverrideSkinWeightBuffer(nullptr), bDefaultOverridden(false), bStaticOverridden(false), DefaultProfileName(NAME_None) {}
	ENGINE_API void Init(FSkinWeightVertexBuffer* InBaseBuffer);

	ENGINE_API ~FSkinWeightProfilesData();
	
	DECLARE_DELEGATE_RetVal_ThreeParams(int32 /** Index into Profiles ArrayView */, FOnPickOverrideSkinWeightProfile, const USkeletalMesh* /** Skeletal Mesh to pick the profile for */, const TArrayView<const FSkinWeightProfileInfo> /** Available skin weight profiles to pick from */, int32 /** LOD Index */);
	static ENGINE_API FOnPickOverrideSkinWeightProfile OnPickOverrideSkinWeightProfile;

#if !WITH_EDITOR
	// Mark this as non-editor only to prevent mishaps from users
	ENGINE_API void OverrideBaseBufferSkinWeightData(USkeletalMesh* Mesh, int32 LODIndex);
#endif 
	ENGINE_API void SetDynamicDefaultSkinWeightProfile(USkeletalMesh* Mesh, int32 LODIndex, bool bSerialization = false);	
	ENGINE_API void ClearDynamicDefaultSkinWeightProfile(USkeletalMesh* Mesh, int32 LODIndex);
	ENGINE_API void SetupDynamicDefaultSkinweightProfile();
	FSkinWeightVertexBuffer* GetDefaultOverrideBuffer() const { return DefaultOverrideSkinWeightBuffer; }

	ENGINE_API bool ContainsProfile(const FName& ProfileName) const;
	ENGINE_API FSkinWeightVertexBuffer* GetOverrideBuffer(const FName& ProfileName) const;
	ENGINE_API bool ContainsOverrideBuffer(const FName& ProfileName) const;
	
	ENGINE_API const FRuntimeSkinWeightProfileData* GetOverrideData(const FName& ProfileName) const;
	ENGINE_API FRuntimeSkinWeightProfileData& AddOverrideData(const FName& ProfileName);
	
	ENGINE_API void ReleaseBuffer(const FName& ProfileName, bool bForceRelease = false);
	ENGINE_API void ReleaseResources();

	ENGINE_API SIZE_T GetResourcesSize() const;
	ENGINE_API SIZE_T GetCPUAccessMemoryOverhead() const;

	friend FArchive& operator<<(FArchive& Ar, FSkinWeightProfilesData& OverrideData);

	ENGINE_API void SerializeMetaData(FArchive& Ar);

	ENGINE_API void ReleaseCPUResources();

	ENGINE_API void CreateRHIBuffers(FRHICommandListBase& RHICmdList, TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers);

	UE_DEPRECATED(5.4, "Use CreateRHIBuffers instead.")
	ENGINE_API void CreateRHIBuffers_RenderThread(TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers);
	UE_DEPRECATED(5.4, "Use CreateRHIBuffers instead.")
	ENGINE_API void CreateRHIBuffers_Async(TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers);

	ENGINE_API void InitRHIForStreaming(const TArray<TPair<FName, FSkinWeightRHIInfo>>& IntermediateBuffers, FRHIResourceUpdateBatcher& Batcher);
	ENGINE_API void ReleaseRHIForStreaming(FRHIResourceUpdateBatcher& Batcher);

	ENGINE_API bool IsPendingReadback() const;
	ENGINE_API void EnqueueGPUReadback();
	ENGINE_API bool IsGPUReadbackFinished() const;
	ENGINE_API void EnqueueDataReadback();
	ENGINE_API bool IsDataReadbackPending() const;
	ENGINE_API bool IsDataReadbackFinished() const;
	ENGINE_API void ResetGPUReadback();
	ENGINE_API void InitialiseProfileBuffer(const FName& ProfileName);

	ENGINE_API bool IsDefaultOverridden() const { return bDefaultOverridden; }
	ENGINE_API bool IsStaticOverridden() const { return bStaticOverridden; }
protected:
	ENGINE_API void ApplyOverrideProfile(FSkinWeightVertexBuffer* OverrideBuffer, const FName& ProfileName);

	FSkinWeightVertexBuffer* BaseBuffer;
	FSkinWeightVertexBuffer* DefaultOverrideSkinWeightBuffer;

	TMap<FName, FSkinWeightVertexBuffer*> ProfileNameToBuffer;
	TMap<FName, FRuntimeSkinWeightProfileData> OverrideData;

	bool bDefaultOverridden;
	bool bStaticOverridden;
	FName DefaultProfileName;

protected:
	FSkinweightReadbackData ReadbackData;
};

