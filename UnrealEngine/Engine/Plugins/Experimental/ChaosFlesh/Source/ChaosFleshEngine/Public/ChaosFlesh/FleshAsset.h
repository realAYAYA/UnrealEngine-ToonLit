// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ChaosFlesh/FleshCollection.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowEngineTypes.h"
#include "Engine/StaticMesh.h"
#include "UObject/ObjectMacros.h"

#include "FleshAsset.generated.h"

class UFleshAsset;
class UDataflow;
class USkeletalMesh;
class USkeleton;

/**
*	FFleshAssetEdit
*     Structured RestCollection access where the scope
*     of the object controls serialization back into the
*     dynamic collection
*
*/
class CHAOSFLESHENGINE_API FFleshAssetEdit
{
public:
	typedef TFunctionRef<void()> FPostEditFunctionCallback;
	friend UFleshAsset;

	/**
	 * @param UFleshAsset				The FAsset to edit
	 */
	FFleshAssetEdit(UFleshAsset* InAsset, FPostEditFunctionCallback InCallable);
	~FFleshAssetEdit();

	FFleshCollection* GetFleshCollection();

private:
	FPostEditFunctionCallback PostEditCallback;
	UFleshAsset* Asset;
};

/**
* UFleshAsset (UObject)
*
* UObject wrapper for the FFleshAsset
*
*/
UCLASS(customconstructor)
class CHAOSFLESHENGINE_API UFleshAsset : public UObject
{
	GENERATED_UCLASS_BODY()
	friend class FFleshAssetEdit;

	//
	// FleshCollection
	// 
	// The FleshCollection stores all the user per-particle properties 
	// for the asset. This is used for simulation and artists 
	// configuration. Only edit the FleshCollection using its Edit
	// object. For example;
	// 
	// {
	//		FFleshAssetEdit EditObject = UFleshAsset->EditCollection();
	//		if( TSharedPtr<FFleshCollection> FleshCollection = EditObject.GetFleshCollection() )
	//      {
	//		}
	//		// the destructor of the edit object will perform invalidation. 
	// }
	//
	TSharedPtr<FFleshCollection, ESPMode::ThreadSafe> FleshCollection;

	void PostEditCallback();

public:

	UFleshAsset(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	/**Editing the collection should only be through the edit object.*/
	void SetCollection(FFleshCollection* InCollection);
	const FFleshCollection* GetCollection() const { return FleshCollection.Get(); }
	FFleshCollection* GetCollection() { return FleshCollection.Get(); }

	TManagedArray<FVector3f>& GetPositions();
	const TManagedArray<FVector3f>* FindPositions() const;

	FFleshAssetEdit EditCollection() const {
		UFleshAsset* ThisNC = const_cast<UFleshAsset*>(this); 
		return FFleshAssetEdit(ThisNC, [ThisNC]() {ThisNC->PostEditCallback(); });
	}

	void Serialize(FArchive& Ar);

	//
	// Dataflow
	//
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	TObjectPtr<UDataflow> DataflowAsset;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString DataflowTerminal = "FleshAssetTerminal";

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	TArray<FStringValuePair> Overrides;

	//
	// SkeletalMesh
	//
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<USkeleton> Skeleton;

	/**
	* Skeleton to use with the flesh deformer or \c GetSkeletalMeshEmbeddedPositions() on the flesh component. 
	* Bindings for this skeletal mesh must be stored in the rest collection.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	TObjectPtr<USkeletalMesh> TargetDeformationSkeleton;

	//
	// SkeletalMesh
	//
	UPROPERTY(EditAnywhere, Category = "Geometry")
	TObjectPtr<UStaticMesh> StaticMesh;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Render")
	bool bRenderInEditor = true;

	/** Information for thumbnail rendering */
	UPROPERTY()
	TObjectPtr<class UThumbnailInfo> ThumbnailInfo;
#endif // WITH_EDITORONLY_DATA

};
