// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "IDetailCustomization.h"
#include "ScalableFloat.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class IPropertyUtilities;
class SComboButton;
class SSearchBox;

DECLARE_LOG_CATEGORY_EXTERN(LogAttributeDetails, Log, All);

class FAttributeDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	TSharedPtr<FString> GetPropertyType() const;

	TArray<TSharedPtr<FString>> PropertyOptions;

	TSharedPtr<IPropertyHandle> MyProperty;

	void OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);	
};


class FAttributePropertyDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

private:

	// the attribute property
	TSharedPtr<IPropertyHandle> MyProperty;
	// the owner property
	TSharedPtr<IPropertyHandle> OwnerProperty;
	// the name property
	TSharedPtr<IPropertyHandle> NameProperty;

	TArray<TSharedPtr<FString>> PropertyOptions;

	TSharedPtr<FString>	GetPropertyType() const;

	void OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	void OnAttributeChanged(FProperty* SelectedAttribute);
};

class GAMEPLAYABILITIESEDITOR_API FScalableFloatDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static constexpr float DefaultMinPreviewLevel = 0.f;
	static constexpr float DefaultMaxPreviewLevel = 30.f;

	FScalableFloatDetails()
		: PreviewLevel(0.f)
		, MinPreviewLevel(DefaultMinPreviewLevel)
		, MaxPreviewLevel(DefaultMaxPreviewLevel) 
	{
	}

protected:

	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

	bool IsEditable() const;
	void UpdatePreviewLevels();

	// Curve Table selector
	TSharedRef<SWidget> CreateCurveTableWidget();
	TSharedRef<SWidget> GetCurveTablePicker();
	void OnSelectCurveTable(const FAssetData& AssetData);
	void OnCloseMenu();
	FText GetCurveTableText() const;
	FText GetCurveTableTooltip() const;
	EVisibility GetCurveTableVisiblity() const;
	EVisibility GetAssetButtonVisiblity() const;
	void OnBrowseTo();
	void OnClear();
	void OnUseSelected();
	
	// Registry Type selector
	TSharedRef<SWidget> CreateRegistryTypeWidget();
	FString GetRegistryTypeValueString() const;
	FText GetRegistryTypeTooltip() const;
	EVisibility GetRegistryTypeVisiblity() const;

	// Curve source accessors
	void OnCurveSourceChanged();
	void RefreshSourceData();
	class UCurveTable* GetCurveTable(FPropertyAccess::Result* OutResult = nullptr) const;
	FDataRegistryType GetRegistryType(FPropertyAccess::Result* OutResult = nullptr) const;

	// Row/item name widget
	TSharedRef<SWidget> CreateRowNameWidget();
	EVisibility GetRowNameVisibility() const;
	FText GetRowNameComboBoxContentText() const;
	FText GetRowNameComboBoxContentTooltip() const;
	void OnRowNameChanged();

	// Preview widgets
	EVisibility GetPreviewVisibility() const;
	float GetPreviewLevel() const;
	void SetPreviewLevel(float NewLevel);
	FText GetRowValuePreviewLabel() const;
	FText GetRowValuePreviewText() const;

	// Row accessors and callbacks
	FName GetRowName(FPropertyAccess::Result* OutResult = nullptr) const;
	const FRealCurve* GetRealCurve(FPropertyAccess::Result* OutResult = nullptr) const;
	FDataRegistryId GetRegistryId(FPropertyAccess::Result* OutResult = nullptr) const;
	void SetRegistryId(FDataRegistryId NewId);
	void GetCustomRowNames(TArray<FName>& OutRows) const;

	TSharedPtr<IPropertyHandle> ValueProperty;
	TSharedPtr<IPropertyHandle> CurveTableHandleProperty;
	TSharedPtr<IPropertyHandle> CurveTableProperty;
	TSharedPtr<IPropertyHandle> RowNameProperty;
	TSharedPtr<IPropertyHandle> RegistryTypeProperty;

	TWeakPtr<IPropertyUtilities> PropertyUtilities;

	float PreviewLevel;
	float MinPreviewLevel;
	float MaxPreviewLevel;
	bool bSourceRefreshQueued;
};
