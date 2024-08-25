// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"

#include "MuCO/CustomizableObjectInstance.h"

#include "CustomizableSkeletalComponent.generated.h"


DECLARE_DELEGATE(FCustomizableSkeletalComponentUpdatedDelegate);

UCLASS(Blueprintable, BlueprintType, ClassGroup = (CustomizableObject), meta = (BlueprintSpawnableComponent))
class CUSTOMIZABLEOBJECT_API UCustomizableSkeletalComponent : public USceneComponent
{
public:
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CustomizableSkeletalMesh)
	TObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CustomizableSkeletalMesh)
	int32 ComponentIndex;

	// Used to replace the SkeletalMesh of the parent component by the ReferenceSkeletalMesh or the generated SkeletalMesh 
	bool bPendingSetSkeletalMesh = false;

	// Used to avoid replacing the SkeletalMesh of the parent component by the ReferenceSkeletalMesh if bPendingSetSkeletalMesh is true
	bool bSkipSetReferenceSkeletalMesh = false;

	FCustomizableSkeletalComponentUpdatedDelegate UpdatedDelegate;

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

#if WITH_EDITOR
	// Used to generate instances outside the CustomizableObject editor and PIE
	void UpdateDistFromComponentToLevelEditorCamera(const FVector& CameraPosition);
	void EditorUpdateComponent();
#endif

private:
	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage;

	void OnAttachmentChanged() override;
	void CreateCustomizableObjectInstanceUsage();

protected:
	virtual void PostInitProperties() override;
};

