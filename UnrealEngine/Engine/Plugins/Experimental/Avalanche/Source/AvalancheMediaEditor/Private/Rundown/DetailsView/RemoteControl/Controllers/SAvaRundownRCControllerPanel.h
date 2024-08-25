// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FAvaRundownEditor;
class FAvaRundownManagedInstance;
class FAvaRundownRCControllerItem;
class IPropertyRowGenerator;
class ITableRow;
class SAvaRundownRCControllerPanel;
class SHeaderRow;
class STableViewBase;
class UAvaRundown;
class URCVirtualPropertyBase;
class URemoteControlPreset;
struct FAvaPlayableRemoteControlValue;
struct FAvaRundownPage;
struct FPropertyChangedEvent;
template <typename ItemType> class SListView;

using FAvaRundownRCControllerItemPtr = TSharedPtr<FAvaRundownRCControllerItem>;

DECLARE_MULTICAST_DELEGATE_TwoParams(FAvaRundownRCControllerHeaderRowExtensionDelegate, TSharedRef<SAvaRundownRCControllerPanel> Panel, 
	TSharedRef<SHeaderRow>& HeaderRow)
DECLARE_DELEGATE_ThreeParams(FAvaRundownRCControllerTableRowExtensionDelegate, TSharedRef<SAvaRundownRCControllerPanel> Panel, 
	TSharedRef<const FAvaRundownRCControllerItem> ItemPtr, TSharedPtr<SWidget>& CurrentWidget)

class SAvaRundownRCControllerPanel : public SCompoundWidget
{
public:
	static const FName ControllerColumnName;
	static const FName ValueColumnName;

	static FAvaRundownRCControllerHeaderRowExtensionDelegate& GetHeaderRowExtensionDelegate() { return HeaderRowExtensionDelegate; }
	static TArray<FAvaRundownRCControllerTableRowExtensionDelegate>& GetTableRowExtensionDelegates(FName InExtensionName);

	SLATE_BEGIN_ARGS(SAvaRundownRCControllerPanel) {}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaRundownEditor>& InRundownEditor);
	
	bool HasRemoteControlPreset(const URemoteControlPreset* InPreset) const;

	void OnPageSelectionChanged(const TArray<int32>& InSelectedPageIds);
	
	void Refresh(const TArray<int32>& InSelectedPageIds);
	
	TSharedRef<ITableRow> OnGenerateControllerRow(FAvaRundownRCControllerItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);

private:
	static FAvaRundownRCControllerHeaderRowExtensionDelegate HeaderRowExtensionDelegate;
	static TMap<FName, TArray<FAvaRundownRCControllerTableRowExtensionDelegate>> TableRowExtensionDelegates;

	void UpdatePropertyRowGenerators(int32 InNumGenerators);
	
	void RefreshForManagedInstance(int32 InInstanceIndex, const FAvaRundownManagedInstance& InManagedInstance, const FAvaRundownPage& InPage);
	
	/** Update the given pages remote control values from the defaults then refresh the widget. */
	void UpdateDefaultValuesAndRefresh(const TArray<int32>& InSelectedPageIds);

	void OnRemoteControlControllerAdded(URemoteControlPreset* InPreset, const FName NewControllerName, const FGuid& InControllerId) { UpdateDefaultValuesAndRefresh({ActivePageId}); }
	void OnRemoteControlControllerRemoved(URemoteControlPreset* InPreset, const FGuid& InControllerId) { UpdateDefaultValuesAndRefresh({ActivePageId}); }
	void OnRemoteControlControllerRenamed(URemoteControlPreset* InPreset, const FName InOldLabel, const FName InNewLabel) { UpdateDefaultValuesAndRefresh({ActivePageId}); }
	void OnRemoteControlControllerModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedControllerIds);
	void BindRemoteControlDelegates(URemoteControlPreset* InPreset);

	const FAvaPlayableRemoteControlValue* GetSelectedPageControllerValue(const URCVirtualPropertyBase* InController) const;
	bool SetSelectedPageControllerValue(const URCVirtualPropertyBase* InController, const FAvaPlayableRemoteControlValue& InValue) const;
	void UpdatePageSummary(bool bInIsPresetChanged);

	UAvaRundown* GetRundown() const;
	const FAvaRundownPage& GetActivePage(const UAvaRundown* InRundown) const;
	const FAvaRundownPage& GetActivePage() const { return GetActivePage(GetRundown()); }
	FAvaRundownPage& GetActivePageMutable(UAvaRundown* InRundown) const;
	FAvaRundownPage& GetActivePageMutable() const { return GetActivePageMutable(GetRundown()); }

	TWeakPtr<FAvaRundownEditor> RundownEditorWeak;

	class FPropertyRowGeneratorWrapper : public FNotifyHook
	{
	public:
		TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
		TWeakObjectPtr<URemoteControlPreset> PresetWeak;
		SAvaRundownRCControllerPanel* ParentPanel = nullptr;

		FPropertyRowGeneratorWrapper(SAvaRundownRCControllerPanel* InParentPanel);
		virtual ~FPropertyRowGeneratorWrapper();

		//~ Begin FNotifyHook
		virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged) override;
		//~ End FNotifyHook
	};
	
	TArray<TUniquePtr<FPropertyRowGeneratorWrapper>> PropertyRowGenerators;

	TArray<TSharedPtr<FAvaRundownManagedInstance>> ManagedInstances;
	
	TSharedPtr<SListView<FAvaRundownRCControllerItemPtr>> ControllerContainer;
	
	TArray<FAvaRundownRCControllerItemPtr> ControllerItems;

	int32 ActivePageId = -1;
};
