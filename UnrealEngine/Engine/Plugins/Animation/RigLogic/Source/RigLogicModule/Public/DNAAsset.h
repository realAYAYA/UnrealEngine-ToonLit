// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "Engine/AssetUserData.h"
#include "Interfaces/Interface_AssetUserData.h"

#include "RigLogic.h"

#include "DNAAsset.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDNAAsset, Log, All);

class IDNAReader;
class IBehaviorReader;
class IGeometryReader;
class FRigLogicMemoryStream;
class UAssetUserData;
class USkeleton;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FDNAIndexMapping;
struct FSharedRigRuntimeContext;
enum class EDNADataLayer: uint8;

 /** An asset holding the data needed to generate/update/animate a RigLogic character
  * It is imported from character's DNA file as a bit stream, and separated out it into runtime (behavior) and design-time chunks;
  * Currently, the design-time part still loads the geometry, as it is needed for the skeletal mesh update; once SkeletalMeshDNAReader is
  * fully implemented, it will be able to read the geometry directly from the SkeletalMesh and won't load it into this asset 
  **/
UCLASS(BlueprintType, meta = (DisplayName = "MetaHuman DNA data"))
class RIGLOGICMODULE_API UDNAAsset : public UAssetUserData
{
	GENERATED_BODY()

public:
	UDNAAsset();
	~UDNAAsset();

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;
#endif

	TSharedPtr<IDNAReader> GetBehaviorReader();
#if WITH_EDITORONLY_DATA
	TSharedPtr<IDNAReader> GetGeometryReader();
#endif

	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = ImportSettings)
	FString DnaFileName;

	/** In non-editor builds, the DNA source data will be unloaded to save memory after the runtime
	  * data has been initialized from it.
	  * 
	  * Set this property to true to keep the DNA in memory, e.g. if you need to modify it at
	  * runtime. For most use cases, this shouldn't be needed.
	 **/
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	bool bKeepDNAAfterInitialization;

	/** Collection of runtime metadata related to a specific character.
	  * The value field is a FString and requires casting for a derived types.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RuntimeSettings)
	TMap<FString, FString> MetaData;

	bool Init(const FString& Filename);
	void Serialize(FArchive& Ar) override;

	/** Used when importing behavior into archetype SkelMesh in the editor,
	  * and when updating SkeletalMesh runtime with GeneSplicer
	**/
	void SetBehaviorReader(TSharedPtr<IDNAReader> SourceDNAReader);
	void SetGeometryReader(TSharedPtr<IDNAReader> SourceDNAReader);

	/** Initialize this object for use at runtime from another instance that has already been
	  * initialized. 
	  * 
	  * Overwrites all member variables. Only data needed for runtime evaluation will be copied.
	  * 
	  * Performs a shallow copy, so the runtime data is shared between the two instances and the
	  * memory cost of the copied UDNAAsset is very low.
	  *
	  * Note that the reference to the shared runtime data will be dropped if the source DNA is
	  * modified, so the two instances are effectively independent and can safely be modified or
	  * deleted without affecting the other.
	 **/
	void InitializeForRuntimeFrom(UDNAAsset* Other);

private:
	friend struct FAnimNode_RigLogic;
	friend struct FRigUnit_RigLogic;

	TSharedPtr<FSharedRigRuntimeContext> GetRigRuntimeContext();
	void InvalidateRigRuntimeContext();
	void InitializeRigRuntimeContext();
	TSharedPtr<FDNAIndexMapping> GetDNAIndexMapping(const USkeleton* Skeleton,
													const USkeletalMesh* SkeletalMesh,
													const USkeletalMeshComponent* SkeletalMeshComponent);

private:
	// Synchronize DNA updates
	FRWLock DNAUpdateLock;

	// Synchronize Rig Runtime Context updates
	FRWLock RigRuntimeContextUpdateLock;

	// Synchronize DNA Index Mapping updates
	FRWLock DNAIndexMappingUpdateLock;

	/** Part of the .dna file needed for run-time execution of RigLogic;
	 **/
	TSharedPtr<IDNAReader> BehaviorReader;

	/** Part of the .dna file used design-time for updating SkeletalMesh geometry
	 **/
	TSharedPtr<IDNAReader> GeometryReader;

	/** Runtime data necessary for rig computations that is shared between
	  * multiple rig instances based on the same DNA.
	**/
	TSharedPtr<FSharedRigRuntimeContext> RigRuntimeContext;

	/** Container for Skeleton <-> DNAAsset index mappings
	  * The mapping object owners will be the SkeletalMeshes, and periodic cleanups will
	  * ensure that dead objects are deleted from the map.
	 **/
	TMap<TWeakObjectPtr<const USkeletalMesh>, TSharedPtr<FDNAIndexMapping>> DNAIndexMappingContainer;
};
