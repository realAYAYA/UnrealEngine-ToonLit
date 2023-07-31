// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "IDetailCustomization.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FDelegateHandle;
class IDetailLayoutBuilder;
class IPropertyHandle;
class SComboButton;
class SWidget;
class UClass;
class UObject;
class USkeleton;
struct FAssetData;

class DETAILCUSTOMIZATIONS_API FSkeletalMeshComponentDetails : public IDetailCustomization
{
public:
	FSkeletalMeshComponentDetails();
	~FSkeletalMeshComponentDetails();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	static TSharedRef<SWidget> CreateAsyncSceneValueWidgetWithWarning(const TSharedPtr<IPropertyHandle>& AsyncScenePropertyHandle);

private:
	void UpdateAnimationCategory(IDetailLayoutBuilder& DetailBuilder);
	void UpdatePhysicsCategory(IDetailLayoutBuilder& DetailBuilder);

	/** Function that returns whether the specified animation mode should be visible */
	EVisibility VisibilityForAnimationMode(EAnimationMode::Type AnimationMode) const;

	/** Helper wrapper functions for VisibilityForAnimationMode */
	EVisibility VisibilityForBlueprintMode() const { return VisibilityForAnimationMode(EAnimationMode::AnimationBlueprint); }
	EVisibility VisibilityForSingleAnimMode() const { return VisibilityForAnimationMode(EAnimationMode::AnimationSingleNode); }
	bool AnimPickerIsEnabled() const;

	/** Handler for filtering animation assets in the UI picker when asset mode is selected */
	bool OnShouldFilterAnimAsset(const FAssetData& AssetData);

	/** Delegate called when a skeletal mesh property is changed on a selected object */
	USkeletalMeshComponent::FOnSkeletalMeshPropertyChanged OnSkeletalMeshPropertyChanged;
	
	/** Register/Unregister the mesh changed delegate to TargetComponent */
	void PerformInitialRegistrationOfSkeletalMeshes(IDetailLayoutBuilder& DetailBuilder);
	void RegisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh);
	void UnregisterSkeletalMeshPropertyChanged(TWeakObjectPtr<USkeletalMeshComponent> Mesh);
	void UnregisterAllMeshPropertyChangedCallers();

	/**
	* Iterates over registered meshes and returns a pointer to the common skeleton used by all of them.
	* If the meshes use more than one different skeleton, NULL is returned.
	*/
	USkeleton* GetValidSkeletonFromRegisteredMeshes() const;

	/** Bound to the delegate used to detect changes in skeletal mesh properties */
	void SkeletalMeshPropertyChanged();

	/** Generates menu content for the class picker when it is clicked */
	TSharedRef<SWidget> GetClassPickerMenuContent();

	/** Gets the currently selected blueprint name to display on the class picker combo button */
	FText GetSelectedAnimBlueprintName() const;

	/** Callback from the class picker when the user selects a class */
	void OnClassPicked(UClass* PickedClass);

	/** Callback from the detail panel to browse to the selected anim asset */
	void OnBrowseToAnimBlueprint();

	/** Callback from the details panel to use the currently selected asset in the content browser */
	void UseSelectedAnimBlueprint();

	/** Called when a skeletal mesh property changes. */
	void UpdateSkeletonNameAndPickerVisibility();

	/** Cached layout builder for use after customization */
	IDetailLayoutBuilder* CurrentDetailBuilder;

	/** Cached selected objects to use when the skeletal mesh property changes */
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;

	/** Cache of mesh components in the current selection */
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> SelectedSkeletalMeshComponents;

	/** Caches the AnimationMode Handle so we can look up its value after customization has finished */
	TSharedPtr<IPropertyHandle> AnimationModeHandle;

	/** Caches the AnimationBlueprintGeneratedClass Handle so we can look up its value after customization has finished */
	TSharedPtr<IPropertyHandle> AnimationBlueprintHandle;

	/** Caches the AsyncScene handle so we can look up its value after customization has finished. */
	TSharedPtr<IPropertyHandle> AsyncSceneHandle;

	/** Full name of the currently selected skeleton to use for filtering animation assets */
	FString SelectedSkeletonName;

	/** The skeleton that we grab the name from for filtering. */
	USkeleton* Skeleton;

	/** Current enabled state of the animation asset picker in the details panel */
	bool bAnimPickerEnabled;

	/** The combo button for the class picker, Cached so we can close it when the user picks something */
	TSharedPtr<SComboButton> ClassPickerComboButton;

	/** Per-mesh handles to registered OnSkeletalMeshPropertyChanged delegates */
	TMap<USkeletalMeshComponent*, FDelegateHandle> OnSkeletalMeshPropertyChangedDelegateHandles;
};
