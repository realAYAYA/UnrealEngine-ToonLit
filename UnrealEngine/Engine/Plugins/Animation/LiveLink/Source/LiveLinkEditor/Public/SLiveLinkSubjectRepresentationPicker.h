// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "Input/Reply.h"
#include "LiveLinkRole.h"
#include "LiveLinkPreset.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Types/SlateEnums.h"
#include "Widgets/Views/SListView.h"

struct FAssetData;
struct FLiveLinkSubjectRepresentationPickerEntry;
struct FSlateBrush;

class FMenuBuilder;
class ITableRow;
class SComboButton;
class STableViewBase;

typedef TSharedPtr<FLiveLinkSubjectRepresentationPickerEntry> FLiveLinkSubjectRepresentationPickerEntryPtr;


/**
 * A widget which allows the user to enter a subject name or discover it from a drop menu.
 */
class LIVELINKEDITOR_API SLiveLinkSubjectRepresentationPicker : public SCompoundWidget
{
public:
	struct FLiveLinkSourceSubjectRole
	{
		FLiveLinkSourceSubjectRole() = default;
		explicit FLiveLinkSourceSubjectRole(FLiveLinkSubjectRepresentation InSubjectRepresentation)
			: Subject(InSubjectRepresentation.Subject), Role(InSubjectRepresentation.Role)
		{}
		explicit FLiveLinkSourceSubjectRole(FLiveLinkSubjectKey InSubjectKey)
			: Source(InSubjectKey.Source), Subject(InSubjectKey.SubjectName)
		{}
		FLiveLinkSourceSubjectRole(FGuid InSource, FLiveLinkSubjectName InSubject, TSubclassOf<ULiveLinkRole> InRole)
			: Source(InSource), Subject(InSubject), Role(InRole)
		{}

		FLiveLinkSubjectRepresentation ToSubjectRepresentation() const { return FLiveLinkSubjectRepresentation(Subject, Role); }
		FLiveLinkSubjectKey ToSubjectKey() const { return FLiveLinkSubjectKey(Source, Subject); }

		FGuid Source;
		FLiveLinkSubjectName Subject;
		TSubclassOf<ULiveLinkRole> Role;
	};

	DECLARE_DELEGATE_OneParam(FOnValueChanged, FLiveLinkSourceSubjectRole);

	SLATE_BEGIN_ARGS(SLiveLinkSubjectRepresentationPicker)
		: _ComboButtonStyle(&FCoreStyle::Get().GetWidgetStyle< FComboButtonStyle >("ComboButton"))
		, _ButtonStyle(nullptr)
		, _ForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _ContentPadding(FMargin(2.f, 0.f))
		, _HasMultipleValues(false)
		, _ShowSource(false)
		, _ShowRole(false)
		, _Font()
	{}

		/** The visual style of the combo button */
		SLATE_STYLE_ARGUMENT(FComboButtonStyle, ComboButtonStyle)

		/** The visual style of the button (overrides ComboButtonStyle) */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
	
		/** Foreground color for the picker */
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
	
		/** Content padding for the picker */
		SLATE_ATTRIBUTE(FMargin, ContentPadding)
	
		/** Attribute used to retrieve the current value. */
		SLATE_ATTRIBUTE(FLiveLinkSourceSubjectRole, Value)
	
		/** Delegate for handling when for when the current value changes. */
		SLATE_EVENT(FOnValueChanged, OnValueChanged)
	
		/** Attribute used to retrieve whether the picker has multiple values. */
		SLATE_ATTRIBUTE(bool, HasMultipleValues)

		/** Attribute used to retrieve whether the picker should show the source name. */
		SLATE_ARGUMENT(bool, ShowSource)

		/** Attribute used to retrieve whether the picker should show roles. */
		SLATE_ARGUMENT(bool, ShowRole)

		/** Sets the font used to draw the text on the button */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)

	SLATE_END_ARGS()

	/**
	 * Slate widget construction method
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Access the current value of this picker
	 */
	FLiveLinkSourceSubjectRole GetCurrentValue() const;

private:
	FText GetSourceNameValueText() const;
	FText GetSubjectNameValueText() const;
	const FSlateBrush* GetRoleIcon() const;
	FText GetRoleText() const;

	TSharedRef<SWidget> BuildMenu();
	FText GetPresetSelectedText() const;
	FSlateColor GetSelectPresetForegroundColor() const;
	FReply ClearCurrentPreset();
	bool HasCurrentPreset() const;
	
	TSharedRef<ITableRow> MakeSubjectRepListViewWidget(FLiveLinkSubjectRepresentationPickerEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnSubjectRepListSelectionChanged(FLiveLinkSubjectRepresentationPickerEntryPtr Entry, ESelectInfo::Type SelectionType);

	TSharedRef<SWidget> BuildPresetSubMenu();
	void NewPresetSelected(const FAssetData& AssetData);
	void OnComboTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);
	void SetValue(const FLiveLinkSourceSubjectRole& InValue);

	void BuildSubjectRepDataList();

private:
	TWeakObjectPtr<ULiveLinkPreset> SelectedLiveLinkPreset;
	TWeakPtr<SComboButton> PickerComboButton;
	TWeakPtr<SComboButton> SelectPresetComboButton;
	TWeakPtr<SListView<FLiveLinkSubjectRepresentationPickerEntryPtr>> SubjectListView;
	TArray<FLiveLinkSubjectRepresentationPickerEntryPtr> SubjectRepData;

	FText CachedSourceType;
	TAttribute<FLiveLinkSourceSubjectRole> ValueAttribute;
	FOnValueChanged OnValueChangedDelegate;
	TAttribute<bool> HasMultipleValuesAttribute;
	bool bShowSource;
	bool bShowRole;
};
