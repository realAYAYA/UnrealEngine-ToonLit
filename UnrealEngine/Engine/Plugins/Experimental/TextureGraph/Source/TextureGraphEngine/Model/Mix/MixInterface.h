// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h" 
#include "Data/TiledBlob.h"
#include "Model/Mix/MixUpdateCycle.h"

#include "MixInterface.generated.h"

class UMix;
class ULayerStack;
class UMixSettings;
class AActor;

class RenderMesh;
typedef std::shared_ptr<RenderMesh>		RenderMeshPtr;

class MeshAsset;
typedef std::shared_ptr<MeshAsset>		MeshAssetPtr;

//////////////////////////////////////////////////////////////////////////
/// Mix can now be saved as Mix or MixInstance.
/// To support this functionality, we introduced the new class named MixInterface. 
/// This will be the base class for both Mix and MixInstance.
//////////////////////////////////////////////////////////////////////////
UCLASS(Abstract)
class TEXTUREGRAPHENGINE_API UMixInterface : public UModelObject
{
	GENERATED_BODY()

private:
	static const TMap<FString, FString> s_uriAlias;						/// Contains the list of URI aliases map to keep the data meaningful, short and precise.
	static const TMap<FString, FString> s_uriAliasDecryptor;			/// We need alias decryptor to convert it so we can extract the object from the data. 

	static TMap<FString, FString>		InitURIAlias();
	static TMap<FString, FString>		InitURIAliasDecryptor();

	FString								GetObjectAlias(FString object);
	FString								GetAliasToObject(FString Alias);

protected:
	UPROPERTY()
	int32								Priority;			/// The priority of this mix in terms of update cycle

	UPROPERTY()
	UMixSettings*						Settings = nullptr;/// The settings for this mix interface

	UPROPERTY()
	bool								bInvalidateTextures;/// Invalidate the scene textures or not

	std::atomic_int64_t					InvalidationFrameId;/// When was the mix last invalidated
	std::atomic_int64_t					UpdatedFrameId;		/// When was the mix actually last updated

	bool								bEnableLOD = true;	/// Enable LOD-ing in the system

	virtual UModelObject*				RootURI() { checkNoEntry() return nullptr; };
	virtual void						Invalidate(FModelInvalidateInfo InvalidateInfo);

public:
	virtual								~UMixInterface() override;


	/** The mesh used by the TS editor to preview.*/
	UPROPERTY(EditAnywhere, Category = Previewing, meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh", ExactClass = "true"))
	FSoftObjectPath						PreviewMesh;  // Adding it here because this will be the base class for TS_Script and TS_ScriptInstance.

	DECLARE_DELEGATE_TwoParams(FOnRenderDone, UMixInterface*, const FInvalidationDetails*);
	FOnRenderDone						OnRenderDone;
	virtual void						PostMeshLoad();

	virtual bool						CanEdit() { return false; }

	virtual void						SetMesh(RenderMeshPtr MeshObj, int MeshType, FVector Scale, FVector2D Dimension);

#if WITH_EDITOR
	virtual AsyncActionResultPtr		SetEditorMesh(AActor* Actor);
	virtual AsyncActionResultPtr		SetEditorMesh(UStaticMeshComponent* MeshComponent, UWorld* World);
#endif
	
	virtual void						Update(MixUpdateCyclePtr cycle);	

	virtual RenderMeshPtr				GetMesh() const; // Get the mesh assigned in Settings

	virtual int32						Width() const; 
	virtual int32						Height() const;
	
	virtual int32						GetNumXTiles() const;
	virtual int32						GetNumYTiles()const;
	
	virtual UMixSettings*				GetSettings() const;

	bool								IsHigherPriorityThan(const UMixInterface* RHS) const;

	virtual void						InvalidateWithDetails(const FInvalidationDetails& Details);
	virtual void						InvalidateAll();

	virtual void						BroadcastOnRenderingDone(const FInvalidationDetails* Details);
	
	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE int32					GetPriority() const { return Priority; }

	FORCEINLINE int64					GetInvalidationFrameId() const { return InvalidationFrameId; }
	FORCEINLINE int64					GetUpdateFrameId() const { return UpdatedFrameId; }
};


