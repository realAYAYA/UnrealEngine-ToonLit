// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IKRetargeterController.h"
#include "Retargeter/IKRetargetProcessor.h"

#include "IKRetargetDetails.generated.h"

enum class EIKRetargetTransformType : int8
{
	Current,
	Reference,
	RelativeOffset,
};

UCLASS(config = Engine, hidecategories = UObject)
class IKRIGEDITOR_API UIKRetargetBoneDetails : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category = "Selection")
	FName SelectedBone;

	UPROPERTY()
	FTransform OffsetTransform;
	
	UPROPERTY()
	FTransform CurrentTransform;

	UPROPERTY()
	FTransform ReferenceTransform;
	
	TSharedPtr<FIKRetargetEditorController> EditorController;

#if WITH_EDITOR

	FEulerTransform GetTransform(EIKRetargetTransformType TransformType, const bool bLocalSpace) const;
	bool IsComponentRelative(ESlateTransformComponent::Type Component, EIKRetargetTransformType TransformType) const;
	void OnComponentRelativeChanged(ESlateTransformComponent::Type Component, bool bIsRelative, EIKRetargetTransformType TransformType);
	void OnCopyToClipboard(ESlateTransformComponent::Type Component, EIKRetargetTransformType TransformType) const;
	void OnPasteFromClipboard(ESlateTransformComponent::Type Component, EIKRetargetTransformType TransformType);
	void OnNumericValueCommitted(
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		FVector::FReal Value,
		ETextCommit::Type CommitType,
		EIKRetargetTransformType TransformType,
		bool bIsCommit);

	TOptional<FVector::FReal> GetNumericValue(
		EIKRetargetTransformType TransformType,
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent);
	/** Method to react to changes of numeric values in the widget */
	static void OnMultiNumericValueCommitted(
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		FVector::FReal Value,
		ETextCommit::Type CommitType,
		EIKRetargetTransformType TransformType,
		TArrayView<UIKRetargetBoneDetails*> Bones,
		bool bIsCommit);

	template<typename DataType>
	void GetContentFromData(const DataType& InData, FString& Content) const;

#endif

private:

	static TOptional<FVector::FReal> CleanRealValue(TOptional<FVector::FReal> InValue);
	
	bool IsRootBone() const;
	
	bool RelativeOffsetTransformRelative[3] = { false, true, false };
	bool CurrentTransformRelative[3];
	bool ReferenceTransformRelative[3];
};

struct FIKRetargetTransformUIData
{
	TArray<EIKRetargetTransformType> TransformTypes;
	TArray<FText> ButtonLabels;
	TArray<FText> ButtonTooltips;
	TAttribute<TArray<EIKRetargetTransformType>> VisibleTransforms;
	TArray<TSharedRef<IPropertyHandle>> Properties;
};

class FIKRetargetBoneDetailCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FIKRetargetBoneDetailCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	void GetTransformUIData(
		const bool bIsEditingPose,
		const IDetailLayoutBuilder& DetailBuilder,
		FIKRetargetTransformUIData& OutData) const;

	TArray<UIKRetargetBoneDetails*> Bones;
};

/** ------------------------------------- BEGIN CHAIN DETAILS CUSTOMIZATION -------------*/


class FRetargetChainSettingsCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRetargetChainSettingsCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	void AddSettingsSection(
		const IDetailLayoutBuilder& DetailBuilder,
		IDetailCategoryBuilder& SettingsCategory,
		const FString& StructPropertyName,
		const FName& GroupName,
		const FText& LocalizedGroupName,
		const UScriptStruct* SettingsClass,
		const FString& EnabledPropertyName,
		const bool& bIsSectionEnabled,
		const FText& DisabledMessage) const;
	
	TArray<TWeakObjectPtr<URetargetChainSettings>> ChainSettingsObjects;
	TSharedPtr<FIKRetargetEditorController> Controller;
	TArray<TSharedPtr<FString>> SourceChainOptions;
};

/** ------------------------------------- BEGIN ROOT DETAILS CUSTOMIZATION -------------*/

class FRetargetRootSettingsCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRetargetRootSettingsCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	TWeakObjectPtr<URetargetRootSettings> RootSettingsObject;
	TSharedPtr<FIKRetargetEditorController> Controller;
};

/** ------------------------------------- BEGIN GLOBAL DETAILS CUSTOMIZATION -------------*/

class FRetargetGlobalSettingsCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRetargetGlobalSettingsCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	TWeakObjectPtr<UIKRetargetGlobalSettings> GlobalSettingsObject;
	TSharedPtr<FIKRetargetEditorController> Controller;

	TArray<TSharedPtr<FString>> TargetChainOptions;
};