// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "IStructureDetailsView.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "PropertyEditorDelegates.h"
#include "SlateFwd.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakFieldPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class FBlueprintEditor;
class FProperty;
class FStructOnScope;
class IDetailsView;
class SBorder;
class SDockTab;
class SMyBlueprint;
class SWidget;
class UObject;
struct FGeometry;
struct FPropertyChangedEvent;

typedef TSet<class UObject*> FInspectorSelectionSet;

//////////////////////////////////////////////////////////////////////////
// SKismetInspector

/** Widget that shows properties and tools related to the selected node(s) */
class KISMET_API SKismetInspector : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SKismetInspector )
		: _ShowPublicViewControl(false)
		, _HideNameArea(true)
		, _SetNotifyHook(true)
		, _ShowTitleArea(false)
		, _ShowLocalVariables(false)
		{}

		SLATE_ARGUMENT(TWeakPtr<FBlueprintEditor>, Kismet2)
		SLATE_ARGUMENT(TWeakPtr<SMyBlueprint>, MyBlueprintWidget)
		SLATE_ATTRIBUTE( bool, ShowPublicViewControl )
		SLATE_ARGUMENT( bool, HideNameArea )
		SLATE_ARGUMENT( FIsPropertyEditingEnabled, IsPropertyEditingEnabledDelegate )
		SLATE_ARGUMENT( FOnFinishedChangingProperties::FDelegate, OnFinishedChangingProperties )
		SLATE_ARGUMENT( FName, ViewIdentifier)
		SLATE_ARGUMENT( bool, SetNotifyHook)
		SLATE_ARGUMENT( bool, ShowTitleArea)
		SLATE_ARGUMENT( bool, ShowLocalVariables)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);

	/** Options for ShowDetails */
	struct FShowDetailsOptions
	{
		FText ForcedTitle;
		bool bForceRefresh;
		bool bShowComponents;
		bool bHideFilterArea;

		FShowDetailsOptions()
			:ForcedTitle()
			,bForceRefresh(false)
			,bShowComponents(true)
			,bHideFilterArea(false)
		{}

		FShowDetailsOptions(const FText& InForcedTitle, bool bInForceRefresh = false)
			:ForcedTitle(InForcedTitle)
			,bForceRefresh(bInForceRefresh)
			,bShowComponents(true)
			,bHideFilterArea(false)
		{}
	};

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	/** Update the inspector window to show information on the supplied object */
	void ShowDetailsForSingleObject(UObject* Object, const FShowDetailsOptions& Options = FShowDetailsOptions());

	/** Update the inspector window to show information on the supplied objects */
	void ShowDetailsForObjects(const TArray<UObject*>& PropertyObjects, const FShowDetailsOptions& Options = FShowDetailsOptions());

	/** Update the inspector window to show single struct. This invalidates ShowDetailsForObjects */
	void ShowSingleStruct(TSharedPtr<FStructOnScope> InStructToDisplay);

	/** Used to control visibility of a property in the property window */
	bool IsPropertyVisible( const struct FPropertyAndParent& PropertyAndParent ) const;

	/** Turns on or off details customization for components */
	void EnableComponentDetailsCustomization(bool bEnable);

	TSharedPtr<class IDetailsView> GetPropertyView() const { return PropertyView; }

	void SetOwnerTab(TSharedRef<SDockTab> Tab);
	TSharedPtr<SDockTab> GetOwnerTab() const;

	/** @return true if the object is in the selection set. */
	bool IsSelected(UObject* Object) const;

	/** returns the list of selected objects */
	const TArray< TWeakObjectPtr<UObject> >& GetSelectedObjects() const;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

protected:
	/** Update the inspector window to show information on the supplied objects */
	void UpdateFromObjects(const TArray<UObject*>& PropertyObjects, struct FKismetSelectionInfo& SelectionInfo, const FShowDetailsOptions& Options);

	/** Add this property and all its child properties to SelectedObjectProperties */
	void AddPropertiesRecursive(FProperty* Property);

	/** Pointer back to the kismet 2 tool that owns us */
	TWeakPtr<FBlueprintEditor> BlueprintEditorPtr;

	/** The tab that owns this details view. */
	TWeakPtr<SDockTab> OwnerTab;

	/** String used as the title above the property window */
	FText PropertyViewTitle;

	/** Should we currently show the property view */
	bool bShowInspectorPropertyView;

	/** Should we currently show components */
	bool bShowComponents;

	/** State of CheckBox representing whether to show only the public variables*/
	ECheckBoxState	PublicViewState;

	/** Property viewing widget */
	TSharedPtr<class IDetailsView> PropertyView;

	/** Selected objects for this detail view */
	TArray< TWeakObjectPtr<UObject> > SelectedObjects;

	/** The widget used to edit the names of properties */
	TSharedPtr<class SNameEditingWidget> EditNameWidget;

	/** Border widget that wraps a dynamic context-sensitive widget for editing objects that the property window is displaying */
	TSharedPtr<SBorder> ContextualEditingBorderWidget;

	/** If true show the public view control */
	TAttribute<bool> bShowPublicView;

	/** If true show the kismet inspector title widget */
	bool bShowTitleArea;

	/** Component details customization enabled. */
	bool bComponenetDetailsCustomizationEnabled;

	/** Set of object properties that should be visible */
	TSet< TWeakFieldPtr<FProperty> > SelectedObjectProperties;
	
	/** User defined delegate for IsPropertyEditingEnabled: */
	FIsPropertyEditingEnabled IsPropertyEditingEnabledDelegate;

	/** User defined delegate for OnFinishedChangingProperties */
	FOnFinishedChangingProperties::FDelegate UserOnFinishedChangingProperties;

	/** When TRUE, the Kismet inspector needs to refresh the details view on Tick */
	bool bRefreshOnTick;

	/** Holds the property objects that need to be displayed by the inspector starting on the next tick */
	TArray<TObjectPtr<UObject>> RefreshPropertyObjects;

	/** Details options that are used by the inspector on the next refresh. */
	FShowDetailsOptions RefreshOptions;

	/** Struct to preview */
	TSharedPtr<FStructOnScope> StructToDisplay;

	/* Sturct Detail View */
	TSharedPtr<class IStructureDetailsView> StructureDetailsView;

	/** Update the inspector window to show information on the single struct */
	void UpdateFromSingleStruct(const TSharedPtr<FStructOnScope>& InStructToDisplay);

	/** Is struct view property read only */
	bool IsStructViewPropertyReadOnly(const struct FPropertyAndParent& PropertyAndParent) const;

protected:
	/** Show properties of the selected object */
	void SetPropertyWindowContents(TArray<UObject*> Objects);

	/** Returns whether the property view be visible */
	EVisibility GetPropertyViewVisibility() const;

	/** Returns whether the properties in the view should be editable */
	bool IsPropertyEditingEnabled() const;

	/** Returns whether the warning about an inherited component not being editable should be visible */
	EVisibility GetInheritedBlueprintComponentWarningVisibility() const;

	/** Opens the parent blueprint when the hyperlink in the warning is clicked */
	void OnInheritedBlueprintComponentWarningHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);

	/**
	 * Generates a widget that is used to edit the specified object array contextually.  This widget
	 * will be displayed along with a property view in the level editor
	 */
	TSharedRef<SWidget> MakeContextualEditingWidget(struct FKismetSelectionInfo& SelectionInfo, const FShowDetailsOptions& Options);

	/**
	 * Generates the text for the title in the contextual editing widget
	 */
	FText GetContextualEditingWidgetTitle() const;

	ECheckBoxState GetPublicViewCheckboxState() const;
	void SetPublicViewCheckboxState(ECheckBoxState InIsChecked);

	bool IsAnyParentOrContainerSelected(const FPropertyAndParent& PropertyAndParent) const;

	/** Callback invoked after a value change on the selected object(s) */
	void OnFinishedChangingProperties(const FPropertyChangedEvent& InPropertyChangedEvent);

	/** Auto-import any namespaces associated with a property's value into the current editor context */
	void ImportNamespacesForPropertyValue(const FProperty* InProperty) const;
};
