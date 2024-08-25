// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "IPropertyTypeCustomization.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "Serialization/Archive.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SWidget.h"

class IPropertyHandle;
class SWidget;
struct FAssetData;
struct FSlateFontInfo;

/** Customize the appearance of an FSlateFontInfo */
class DETAILCUSTOMIZATIONS_API FSlateFontInfoStructCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;

protected:
	void AddFontSizeProperty(IDetailChildrenBuilder& InStructBuilder);
	bool IsFontSizeEnabled() const;

	/** @return The value or unset if properties with multiple values are viewed */
	TOptional<float> OnFontSizeGetValue() const;
	void OnFontSizeValueChanged(float NewDisplayValue);
	void OnFontSizeValueCommitted(float NewDisplayValue, ETextCommit::Type CommitInfo);

	/**
	 * Called when the slider begins to move.  We create a transaction here to undo the property
	 */
	void OnFontSizeBeginSliderMovement();

	/**
	 * Called when the slider stops moving.  We end the previously created transaction
	 */
	void OnFontSizeEndSliderMovement(float NewDisplayValue);

	/** @return a dynamic text explaining what the size does and showing the current Font DPI setting */
	FText GetFontSizeTooltipText() const;

	static float ConvertFontSizeFromNativeToDisplay(float FontSize);
	static float ConvertFontSizeFromDisplayToNative(float FontSize);

	/** Called to filter out invalid font assets */
	static bool OnFilterFontAsset(const FAssetData& InAssetData);

	/** Called when the font object used by this FSlateFontInfo has been changed */
	void OnFontChanged(const FAssetData& InAssetData);

	/** Called to see whether the font entry combo should be enabled */
	bool IsFontEntryComboEnabled() const;

	/** Called before the font entry combo is opened - used to update the list of available font entries */
	void OnFontEntryComboOpening();

	/** Called when the selection of the font entry combo is changed */
	void OnFontEntrySelectionChanged(TSharedPtr<FName> InNewSelection, ESelectInfo::Type);

	/** Make the widget for an entry in the font entry combo */
	TSharedRef<SWidget> MakeFontEntryWidget(TSharedPtr<FName> InFontEntry);

	/** Get the text to use for the font entry combo button */
	FText GetFontEntryComboText() const;

	/** Get the name of the currently active font entry (may not be the selected entry if the entry is set to use "None") */
	FName GetActiveFontEntry() const;

	/** Get the array of FSlateFontInfo instances this customization is currently editing */
	TArray<FSlateFontInfo*> GetFontInfoBeingEdited();
	TArray<const FSlateFontInfo*> GetFontInfoBeingEdited() const;

	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** Handle to the "FontObject" property being edited */
	TSharedPtr<IPropertyHandle> FontObjectProperty;

	/** Handle to the "TypefaceFontName" property being edited */
	TSharedPtr<IPropertyHandle> TypefaceFontNameProperty;

	/** Handle to the "Size" property being edited */
	TSharedPtr<IPropertyHandle> FontSizeProperty;

	/** Font entry combo box widget */
	TSharedPtr<SComboBox<TSharedPtr<FName>>> FontEntryCombo;

	/** Source data for the font entry combo widget */
	TArray<TSharedPtr<FName>> FontEntryComboData;

	/** True if the slider is being used to change the value of the property */
	bool bIsUsingSlider = false;

	/** When using the slider, what was the last committed value */
	float LastSliderFontSizeCommittedValue = 0.0f;

};
