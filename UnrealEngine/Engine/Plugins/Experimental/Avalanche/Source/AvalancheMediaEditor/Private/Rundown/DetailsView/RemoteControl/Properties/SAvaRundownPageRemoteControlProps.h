// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/ITableRow.h"

class FAvaRundownEditor;
class FAvaRundownManagedInstance;
class FAvaRundownRCPropertyItem;
class FName;
class ITableRow;
class SAvaRundownPageRemoteControlProps;
class STableViewBase;
class SWidget;
class URemoteControlPreset;
struct FAvaPlayableRemoteControlValue;
struct FAvaRundownPage;
struct FGuid;
struct FRemoteControlEntity;

using FAvaRundownRCPropertyItemPtr = TSharedPtr<FAvaRundownRCPropertyItem>;

DECLARE_MULTICAST_DELEGATE_TwoParams(FAvaRundownRCPropertyHeaderRowExtensionDelegate, TSharedRef<SAvaRundownPageRemoteControlProps> Panel,
	TSharedRef<SHeaderRow>& HeaderRow)
DECLARE_DELEGATE_ThreeParams(FAvaRundownRCPropertyTableRowExtensionDelegate, TSharedRef<SAvaRundownPageRemoteControlProps> Panel,
	TSharedRef<const FAvaRundownRCPropertyItem> ItemPtr, TSharedPtr<SWidget>& CurrentWidget)

/**
 * The page props implementation for remote control fields.
 */
class SAvaRundownPageRemoteControlProps : public SCompoundWidget
{
public:
	static const FName PropertyColumnName;
	static const FName ValueColumnName;

	static FAvaRundownRCPropertyHeaderRowExtensionDelegate& GetHeaderRowExtensionDelegate() { return HeaderRowExtensionDelegate; }
	static TArray<FAvaRundownRCPropertyTableRowExtensionDelegate>& GetTableRowExtensionDelegates(FName InExtensionName);

	SLATE_BEGIN_ARGS(SAvaRundownPageRemoteControlProps) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FAvaRundownEditor> InRundownEditor);
	virtual ~SAvaRundownPageRemoteControlProps() override;

	/** Update the current page's remote control values from the defaults then refresh the widget. */
	void UpdateDefaultValuesAndRefresh(const TArray<int32>& InSelectedPageIds);

	/** Refreshes the content of this widget. */
	void Refresh(const TArray<int32>& InSelectedPageIds);

private:
	static FAvaRundownRCPropertyHeaderRowExtensionDelegate HeaderRowExtensionDelegate;
	static TMap<FName, TArray<FAvaRundownRCPropertyTableRowExtensionDelegate>> TableRowExtensionDelegates;

	void OnRemoteControlEntitiesExposed(URemoteControlPreset* InPreset, const FGuid& InEntityId) { UpdateDefaultValuesAndRefresh({GetActivePageId()}); }
	void OnRemoteControlEntitiesUnexposed(URemoteControlPreset* InPreset, const FGuid& InEntityId) { UpdateDefaultValuesAndRefresh({GetActivePageId()}); }
	void OnRemoteControlEntitiesUpdated(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedEntities) { UpdateDefaultValuesAndRefresh({GetActivePageId()}); }
	void OnRemoteControlExposedPropertiesModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedProperties);
	void OnRemoteControlControllerModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedControllerIds);

	void BindRemoteControlDelegates(URemoteControlPreset* InPreset);

	bool HasRemoteControlPreset(const URemoteControlPreset* InPreset) const;

	/** Returns the currently selected page if 1 page is currently selected, returns nullptr otherwise. */
	FAvaRundownPage* GetActivePage() const;

	/** Returns the currently selected page id if 1 page is currently selected, returns InvalidPageId otherwise. */
	int32 GetActivePageId() const { return ActivePageId; }

	/**
	 * Get Selected Page's entity value corresponding to the given entity (using entity Id to match).
	 * @return pointer to page's entity value, null if not found.
	 */
	const FAvaPlayableRemoteControlValue* GetSelectedPageEntityValue(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity) const;

	/**
	 * Set (or add) Selected Page's entity value corresponding to the given entity (using entity Id to match).
	 * @return true if it succeeded, false otherwise.
	 */
	bool SetSelectedPageEntityValue(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity, const FAvaPlayableRemoteControlValue& InValue) const;

	TSharedRef<ITableRow> OnGenerateControllerRow(FAvaRundownRCPropertyItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	void RefreshTable(const TSet<FGuid>& InEntityIds = TSet<FGuid>());

	TWeakPtr<FAvaRundownEditor> RundownEditorWeak;
	
	TArray<TSharedPtr<FAvaRundownManagedInstance>> ManagedInstances;

	/** The widget that lists the property rows. */
	TSharedPtr<SListView<FAvaRundownRCPropertyItemPtr>> PropertyContainer;
	
	/** The data used to back the properties container list view. */
	TArray<FAvaRundownRCPropertyItemPtr> PropertyItems;

	int32 ActivePageId = -1;
};
