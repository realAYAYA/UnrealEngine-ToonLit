// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Font/AvaFontObject.h"
#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"
#include "PropertyEditorModule.h"
#include "UObject/StrongObjectPtr.h"

class FAvaFontView;
class FUICommandList;
class SAvaFontField;
class SAvaFontSelector;
class SBox;
class SButton;
class STextBlock;
class UAvaFontManagerSubsystem;
class UFactory;
class UFont;

struct FAvaFont;
struct FAvaMultiFontSelectionData;

class FAvaFontDetailsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual ~FAvaFontDetailsCustomization() override;

	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> FontStructPropertyHandle
		, FDetailWidgetRow& HeaderRow
		, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle
		, IDetailChildrenBuilder& ChildBuilder
		, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization

private:
	void RefreshSelectedFont();

	void RefreshLocalFontOptions();

	void Refresh();

	void SetDefaultValue();

	FReply ImportButtonClicked();

	TSharedRef<SWidget> HandleGenerateWidget(const TSharedPtr<FAvaFontView>& InItem);

	TSharedRef<SWidget> GenerateMultipleSelectionField() const;

	FReply OnBrowseToAssetClicked();
	
	FReply OnRefreshFontClicked();

	TSharedRef<SWidget> MakeBrowseButton();

	TSharedRef<SWidget> MakeRefreshFontButton();

	TSharedRef<SWidget> MakeImportButton();

	void UpdateOrGenerateWidget(const TSharedPtr<FAvaFontView>& InItem);

	void HandleSelectionChanged(const TSharedPtr<FAvaFontView> InItem, ESelectInfo::Type InSelectionType);

	void OnFontFieldUpdated(const TSharedPtr<FAvaFontView>& InItem) const;

	void UpdateFontObjectProperty(UAvaFontObject* InFontObject);

	TSharedPtr<FAvaFontView> GetCurrentAvaFontView(FPropertyAccess::Result& OutAccessResult);

	FAvaFont* GetCurrentAvaFont(FPropertyAccess::Result& OutAccessResult) const;

	void UpdateButtonsStatus(const TSharedPtr<FAvaFontView>& InAvaFont);

	void OnProjectFontCreated(const UFont* InFont);

	void OnProjectFontDeleted(const UFont* InObject);

	void OnSystemFontsUpdated();

	void RegisterFontManagerCallbacks();

	void UnregisterFontManagerCallbacks() const;

	void RefreshMissingFontWidget() const;

	void RefreshComboboxContent();

	/** The true objects bound to the Slate combobox. */
	TArray<TSharedPtr<FAvaFontView>> Options;

	/** A shared pointer to the underlying slate combobox */
	TSharedPtr<SAvaFontSelector> FontSelector;

	/** A shared pointer to a container that holds the combobox content that is selected */
	TSharedPtr<SBox> ComboBoxContent;

	/** A shared pointer to the current selected font (points to font in font manager list) */
	TSharedPtr<FAvaFontView> SelectedOption;

	/** Reference to default font, used also as fallback */
	TSharedPtr<FAvaFontView> DefaultOption;

	/** Stores a reference to the font currently handled by this customization PropertyHandle */
	TSharedPtr<FAvaFontView> CurrentAvaFontValue;

	TSharedPtr<IPropertyHandle> AvaFontPropertyHandle;

	TSharedPtr<STextBlock> MissingFontText;

	/** Import is enabled for system-only fonts (they are not assets) */
	bool bImportEnabled = true;

	/** Refresh is enabled for fonts which are available on the local OS */
	bool bRefreshFontEnabled = true;

	TWeakObjectPtr<UAvaFontManagerSubsystem> FontManagerSubsystemWeak;
};
