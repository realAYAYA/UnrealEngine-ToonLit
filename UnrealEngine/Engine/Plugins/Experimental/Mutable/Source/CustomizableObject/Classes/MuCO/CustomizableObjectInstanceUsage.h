// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectInstance.h"
#include "Tickable.h"
#include "Engine/EngineBaseTypes.h"

#include "CustomizableObjectInstanceUsage.generated.h"

class AActor;
class FObjectPreSaveContext;
class UObject;
class USkeletalMesh;
class UCustomizableSkeletalComponent;
struct FFrame;
enum class EUpdateResult : uint8;

DECLARE_DELEGATE(FCustomizableObjectInstanceUsageUpdatedDelegate);

// This class can be used instead of a UCustomizableComponent (for example for non-BP projects) to link a 
// UCustomizableObjectInstance and a USkeletalComponent so that the CustomizableObjectSystem takes care of updating it and its LODs, 
// streaming, etc. It's a UObject, so it will be much cheaper than a UCustomizableComponent as it won't have to refresh its transforms
// every time it's moved.
UCLASS(Blueprintable, BlueprintType, ClassGroup = (CustomizableObject), meta = (BlueprintSpawnableComponent))
class CUSTOMIZABLEOBJECT_API UCustomizableObjectInstanceUsage : public UObject, public FTickableGameObject
{
public:
	GENERATED_BODY()

	UPROPERTY(Transient)
	float SkippedLastRenderTime;

	FCustomizableObjectInstanceUsageUpdatedDelegate UpdatedDelegate;

	void SetCustomizableObjectInstance(UCustomizableObjectInstance* CustomizableObjectInstance);
	UCustomizableObjectInstance* GetCustomizableObjectInstance() const;

	void SetComponentIndex(int32 ComponentIndex);
	int32 GetComponentIndex() const;

	void AttachTo(USkeletalMeshComponent* SkeletalMeshComponent);
	USkeletalMeshComponent* GetAttachParent() const;

	/** Common end point of all updates. Even those which failed. */
	void Callbacks() const;
	
	USkeletalMesh* GetSkeletalMesh() const;
	USkeletalMesh* GetAttachedSkeletalMesh() const;

	/** Update Skeletal Mesh asynchronously. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	void UpdateSkeletalMeshAsync(bool bNeverSkipUpdate = false);

	/** Update Skeletal Mesh asynchronously. Callback will be called once the update finishes, even if it fails. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist = false, bool bForceHighPriority = false);

	void SetSkeletalMesh(USkeletalMesh* SkeletalMesh);
	void SetPhysicsAsset(class UPhysicsAsset* PhysicsAsset);
	void UpdateDistFromComponentToPlayer(const AActor* const Pawn, bool bForceEvenIfNotBegunPlay = false);

	void SetVisibilityOfSkeletalMeshSectionWithMaterialName(bool bVisible, const FString& MaterialName, int32 LOD);

	// Set to true to replace the SkeletalMesh of the parent component by the ReferenceSkeletalMesh or the generated SkeletalMesh 
	void SetPendingSetSkeletalMesh(bool bIsActive);
	bool GetPendingSetSkeletalMesh() const;

	// Set to true to avoid replacing the SkeletalMesh of the parent component by the ReferenceSkeletalMesh if bPendingSetSkeletalMesh is true
	void SetSkipSetReferenceSkeletalMesh(bool bIsActive);
	bool GetSkipSetReferenceSkeletalMesh() const;

	// USceneComponent interface
	virtual void BeginDestroy() override;

	// Returns true if the NetMode of the associated UCustomizableSkeletalComponent (or the associated SkeletalMeshComponent if the former does not exist) is equal to InNetMode
	bool IsNetMode(ENetMode InNetMode) const;

#if WITH_EDITOR
	// Used to generate instances outside the CustomizableObject editor and PIE
	void UpdateDistFromComponentToLevelEditorCamera(const FVector& CameraPosition);
	void EditorUpdateComponent();
#endif

private:
	// If this CustomizableSkeletalComponent is not null, it means this Usage was created by it, and all persistent properties should be obtained through it
	UPROPERTY()
	TObjectPtr<UCustomizableSkeletalComponent> CustomizableSkeletalComponent;

	// If no CustomizableSkeletalComponent is associated, this SkeletalComponent will be used
	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> UsedSkeletalMeshComponent;

	// If no CustomizableSkeletalComponent is associated, this Instance will be used
	UPROPERTY()
	TObjectPtr<UCustomizableObjectInstance> UsedCustomizableObjectInstance;

	// If no CustomizableSkeletalComponent is associated, this Index will be used
	UPROPERTY()
	int32 UsedComponentIndex;

	// Used to replace the SkeletalMesh of the parent component by the ReferenceSkeletalMesh or the generated SkeletalMesh 
	bool bUsedPendingSetSkeletalMesh = false;

	// Used to avoid replacing the SkeletalMesh of the parent component by the ReferenceSkeletalMesh if bPendingSetSkeletalMesh is true
	bool bUsedSkipSetReferenceSkeletalMesh = false;

	// Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickable() const override;
	// End FTickableGameObject

	friend UCustomizableSkeletalComponent;
};

