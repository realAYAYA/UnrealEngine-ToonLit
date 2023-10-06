// Copyright Epic Games, Inc. All Rights Reserved. 

#pragma once

#include "Widgets/SBoxPanel.h"

#include "CustomizableObjectCustomSettings.generated.h"

namespace ESelectInfo { enum Type : int; }
namespace ETextCommit { enum Type : int; }
template <typename ItemType> class SListView;

class SWidget;
struct FAssetData;
struct FGeometry;
struct FPointerEvent;

UCLASS()
class UCustomizableObjectEmptyClassForSettings : public UObject
{
	GENERATED_BODY()
public:
	TWeakPtr<class SCustomizableObjectEditorViewportTabBody> Viewport;
	TObjectPtr<class UDebugSkelMeshComponent>* PreviewSkeletalMeshComp = nullptr;
};

class SCustomizableObjectCustomSettings : public SVerticalBox
{
public:
	void Construct(const FArguments& InArgs, UCustomizableObjectEmptyClassForSettings* InObject);

private:
	class FCustomizableObjectEditorViewportClient* ViewportClient;

	// Lighting
public:
	void SetViewportLightsByAsset(const FAssetData& InAsset);

private:

	TSharedRef<SWidget> GetLightComboButtonContent();
	void CloseLightComboButtonContent();
	FReply OnSaveViewportLightsAsset();
	FReply OnNewViewportLightsAsset();

	FReply OnPointLightAdded();
	FReply OnSpotLightAdded();
	FReply OnLightRemoved();
	FReply OnLightUnselected();
	TSharedRef<class ITableRow> OnGenerateWidgetForList(TSharedPtr<FString> Item, const TSharedRef<class STableViewBase>& OwnerTable);
	void OnListSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// LightProperties
	TOptional<float> GetIntensityValue() const;
	void OnIntensityValueCommited(float Value, ETextCommit::Type CommitType);

	FLinearColor GetLightColorValue() const;
	FReply OnLightColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void OnSetLightColorFromColorPicker(FLinearColor InColor);

	TOptional<float> GetAttenuationRadius() const;
	void OnAttenuationRadiusValueCommited(float Value, ETextCommit::Type CommitType);

	TOptional<float> GetLightSourceRadius() const;
	void OnLightSourceRadiusValueCommited(float Value, ETextCommit::Type CommitType);

	TOptional<float> GetLightSourceLength() const;
	void OnLightSourceLengthValueCommited(float Value, ETextCommit::Type CommitType);

	TOptional<float> GetLightInnerConeAngle() const;
	void OnLightInnerConeAngleValueCommited(float Value, ETextCommit::Type CommitType);

	TOptional<float> GetLightOuterConeAngle() const;
	void OnLightOuterConeAngleValueCommited(float Value, ETextCommit::Type CommitType);

private:
	TArray<TSharedPtr<FString>> LightNames;
	TArray<class UPointLightComponent*> Lights;

	static const FString LightPackagePath;
	TSharedPtr<class SEditableTextBox> LightsAssetNameInputText;

	TSharedPtr<class SComboButton> LightComboButton;
	TSharedPtr<class STextBlock> LightAssetText;
	TSharedPtr<SListView<TSharedPtr<FString>>> LightsListView;
	class ULightComponent* SelectedLight;
	TSharedPtr<class SExpandableArea> PointLightProperties;
	TSharedPtr<class SExpandableArea> SpotLightProperties;

};
