// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "AssetRegistry/AssetData.h"
#include "IAssetFamily.h"

class UAnimBlueprint;
class UPhysicsAsset;
class UPersonaOptions;

class FPersonaAssetFamily : public IAssetFamily, public TSharedFromThis<FPersonaAssetFamily>
{
public:
	FPersonaAssetFamily(const UObject* InFromObject);
	FPersonaAssetFamily(const UObject* InFromObject, const TSharedRef<FPersonaAssetFamily> InFromFamily);

	virtual ~FPersonaAssetFamily() {}

	/** IAssetFamily interface */
	virtual void GetAssetTypes(TArray<UClass*>& OutAssetTypes) const override;
	virtual FAssetData FindAssetOfType(UClass* InAssetClass) const override;
	virtual void FindAssetsOfType(UClass* InAssetClass, TArray<FAssetData>& OutAssets) const override;
	virtual FText GetAssetTypeDisplayName(UClass* InAssetClass) const override;
	virtual const FSlateBrush* GetAssetTypeDisplayIcon(UClass* InAssetClass) const override;
	virtual FSlateColor GetAssetTypeDisplayTint(UClass* InAssetClass) const override;
	virtual bool IsAssetCompatible(const FAssetData& InAssetData) const override;
	virtual UClass* GetAssetFamilyClass(UClass* InClass) const override;
	virtual void RecordAssetOpened(const FAssetData& InAssetData) override;
	DECLARE_DERIVED_EVENT(FPersonaAssetFamily, IAssetFamily::FOnAssetOpened, FOnAssetOpened)
	virtual FOnAssetOpened& GetOnAssetOpened() override { return OnAssetOpened;  }
	DECLARE_DERIVED_EVENT(FPersonaAssetFamily, IAssetFamily::FOnAssetFamilyChanged, FOnAssetFamilyChanged)
	virtual FOnAssetFamilyChanged& GetOnAssetFamilyChanged() override { return OnAssetFamilyChanged; }

	/** Helper functions for constructor and other systems that need to discover meshes/skeletons from related assets */
	static void FindCounterpartAssets(const UObject* InAsset, TWeakObjectPtr<const USkeleton>& OutSkeleton, TWeakObjectPtr<const USkeletalMesh>& OutMesh);
	static void FindCounterpartAssets(const UObject* InAsset, const USkeleton*& OutSkeleton, const USkeletalMesh*& OutMesh);

private:
	/** Initialization to avoid shared ptr access in constructor */
	void Initialize();
	
	/** Hande key persona settings changes (e.g. skeleton compatibilty) */
	void OnSettingsChange(const UPersonaOptions* InOptions, EPropertyChangeType::Type InChangeType);
	
private:
	friend class FPersonaAssetFamilyManager;
	
	/** The skeleton that links all assets */
	TWeakObjectPtr<const USkeleton> Skeleton;

	/** The last mesh that was encountered */
	TWeakObjectPtr<const USkeletalMesh> Mesh;

	/** The last anim blueprint that was encountered */
	TWeakObjectPtr<const UAnimBlueprint> AnimBlueprint;

	/** The last animation asset that was encountered */
	TWeakObjectPtr<const UAnimationAsset> AnimationAsset;

	/** The last physics asset that was encountered */
	TWeakObjectPtr<const UPhysicsAsset> PhysicsAsset;

	/** Event fired when an asset is opened */
	FOnAssetOpened OnAssetOpened;

	/** Event fired when an asset family changes (e.g. relationships are altered) */
	FOnAssetFamilyChanged OnAssetFamilyChanged;
};
