// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineBaseTypes.h"
#include "Math/UnrealMathSSE.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableSkeletalComponent.generated.h"

class AActor;
class FObjectPreSaveContext;
class UCustomizableSkeletalComponent;
class UObject;
class USkeletalMesh;
struct FFrame;
class UCustomizableObjectInstance;
enum class EUpdateResult : uint8;

DECLARE_DELEGATE_TwoParams(FCustomizableSkeletalComponentPreUpdateDelegate, UCustomizableSkeletalComponent* Component, USkeletalMesh* NextMesh);
DECLARE_DELEGATE(FCustomizableSkeletalComponentUpdatedDelegate);

UCLASS(Blueprintable, BlueprintType, ClassGroup = (CustomizableObject), meta = (BlueprintSpawnableComponent))
class CUSTOMIZABLEOBJECT_API UCustomizableSkeletalComponent : public USceneComponent
{
public:
	GENERATED_BODY()

	UCustomizableSkeletalComponent();

	// Used to replace the SkeletalMesh of the parent component by the ReferenceSkeletalMesh or the generated SkeletalMesh 
	bool bPendingSetSkeletalMesh = false;

	// Used to avoid replacing the SkeletalMesh of the parent component by the ReferenceSkeletalMesh if bPendingSetSkeletalMesh is true
	bool bSkipSetReferenceSkeletalMesh = false;

	UPROPERTY(Transient)
	float SkippedLastRenderTime;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CustomizableSkeletalMesh)
	TObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CustomizableSkeletalMesh)
	int32 ComponentIndex;

	FCustomizableSkeletalComponentPreUpdateDelegate PreUpdateDelegate;
	FCustomizableSkeletalComponentUpdatedDelegate UpdatedDelegate;
	
	void Updated(EUpdateResult Result) const;
	
	USkeletalMesh* GetSkeletalMesh() const;
	USkeletalMesh* GetAttachedSkeletalMesh() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObject)
	void UpdateSkeletalMeshAsync(bool bNeverSkipUpdate = false);

	void SetSkeletalMesh(USkeletalMesh* SkeletalMesh, bool bReinitPose=true, bool bForceClothReset=true);
	void SetPhysicsAsset(class UPhysicsAsset* PhysicsAsset);
	void UpdateDistFromComponentToPlayer(const AActor* const Pawn, bool bForceEvenIfNotBegunPlay = false);

	void SetVisibilityOfSkeletalMeshSectionWithMaterialName(bool bVisible, const FString& MaterialName, int32 LOD);

	// USceneComponent interface
	virtual void BeginDestroy() override;

	// UObject interface
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

#if WITH_EDITOR
	// Used to generate instances outside the CustomizableObject editor and PIE
	void UpdateDistFromComponentToLevelEditorCamera(const FVector& CameraPosition);
	void EditorUpdateComponent();
#endif

private:
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void OnAttachmentChanged() override;

};

