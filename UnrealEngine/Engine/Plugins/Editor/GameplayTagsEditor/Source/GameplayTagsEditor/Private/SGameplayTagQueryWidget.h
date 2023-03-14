// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "GameplayTagContainer.h"
#include "PropertyHandle.h"

class IDetailsView;
struct FPropertyChangedEvent;

/** Widget allowing user to tag assets with gameplay tags */
class SGameplayTagQueryWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SGameplayTagQueryWidget )
		: _ReadOnly(false), _AutoSave(false)
	{}
		SLATE_ARGUMENT(bool, ReadOnly)		// Flag to set if the list is read only
		SLATE_ARGUMENT(bool, AutoSave)		// Flag to set if edits should be applied automatically (hides buttons)
		SLATE_EVENT(FSimpleDelegate, OnClosePreSave) // Called when "Save and Close" button clicked
		SLATE_EVENT(FSimpleDelegate, OnSaveAndClose) // Called when "Save and Close" button clicked
		SLATE_EVENT(FSimpleDelegate, OnCancel) // Called when "Close Without Saving" button clicked
		SLATE_EVENT(FSimpleDelegate, OnQueryChanged)	// Called when the user has modified the query
	SLATE_END_ARGS()

		/** Simple struct holding a tag query and its owner for generic re-use of the widget */
	struct FEditableGameplayTagQueryDatum
	{
		/** Constructor */
		FEditableGameplayTagQueryDatum(class UObject* InOwnerObj, struct FGameplayTagQuery* InTagQuery, FString* InTagExportText=nullptr)
			: TagQueryOwner(InOwnerObj)
			, TagQuery(InTagQuery)
			, TagQueryExportText(InTagExportText)
		{}

		/** Owning UObject of the query being edited */
		TWeakObjectPtr<class UObject> TagQueryOwner;

		/** Tag query to edit */
		struct FGameplayTagQuery* TagQuery;

		/** The export text for FGameplayTagQuery, useful in some circumstances */
		FString* TagQueryExportText;
	};

	/** Construct the actual widget */
	void Construct(const FArguments& InArgs, const TArray<FEditableGameplayTagQueryDatum>& EditableTagQueries, const TSharedPtr<IPropertyHandle> InTagQueryPropertyHandle);

	virtual ~SGameplayTagQueryWidget() override;

private:

	void OnGetCategoriesMetaFromPropertyHandle(TSharedPtr<IPropertyHandle> PropertyHandle, FString& MetaString);

	/* Flag to set if the list is read only*/
	uint32 bReadOnly : 1;

	/** If true, query will be written immediately on all changes. Otherwise, will only be written on user prompt (via the buttons). */
	uint32 bAutoSave : 1;

	/** Containers to modify */
	TArray<FEditableGameplayTagQueryDatum> TagQueries;

	/** Property that holds the tag query */
	TSharedPtr<IPropertyHandle> TagQueryPropertyHandle;

	/** Called when "Save and Close" is clicked before we save the data. */
	FSimpleDelegate OnClosePreSave;

	/** Called when "Save and Close" is clicked after we have saved the data. */
	FSimpleDelegate OnSaveAndClose;

	/** Called when the user has modified the query */
	FSimpleDelegate OnQueryChanged;

	/** Called when "Close Without Saving" is clicked */
	FSimpleDelegate OnCancel;

	/** Properties Tab */
	TSharedPtr<class IDetailsView> Details;

	static class UEditableGameplayTagQuery* CreateEditableQuery(FGameplayTagQuery& Q);
	TWeakObjectPtr<class UEditableGameplayTagQuery> EditableQuery;

	/** Called when the user clicks the "Save and Close" button */
	FReply OnSaveAndCloseClicked();

	/** Called when the user clicks the "Close Without Saving" button */
	FReply OnCancelClicked() const;

	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Controls visibility of the "Save and Close" button */
	EVisibility GetSaveAndCloseButtonVisibility() const;
	/** Controls visibility of the "Close Without Saving" button */
	EVisibility GetCancelButtonVisibility() const;

	void SaveToTagQuery();
};

