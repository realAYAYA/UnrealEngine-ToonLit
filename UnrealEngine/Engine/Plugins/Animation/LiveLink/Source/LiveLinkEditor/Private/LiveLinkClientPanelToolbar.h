// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"


class ULiveLinkPreset;
template <typename T> class TSubclassOf;

class FMenuBuilder;
class FLiveLinkClient;
class ILiveLinkSource;
class IMenu;
class SComboButton;
class SEditableTextBox;
class STextEntryPopup;
class ULiveLinkSourceFactory;
struct FAssetData;

class SLiveLinkClientPanelToolbar : public SCompoundWidget, public FGCObject
{
	SLATE_BEGIN_ARGS(SLiveLinkClientPanelToolbar){}
	SLATE_END_ARGS()


	void Construct(const FArguments& Args, FLiveLinkClient* InClient);

	//~ FEditorUndoClient interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SLiveLinkClientPanelToolbar");
	}

private:
	TSharedRef<SWidget> OnGenerateSourceMenu();
	void RetrieveFactorySourcePanel(FMenuBuilder& MenuBuilder, int32 FactoryIndex);
	void ExecuteCreateSource(int32 FactoryIndex);
	void OnSourceCreated(TSharedPtr<ILiveLinkSource> NewSource, FString ConnectionString, TSubclassOf<ULiveLinkSourceFactory> Factory);

	TSharedRef<SWidget> OnPresetGeneratePresetsMenu();
	TSharedRef<SWidget> HandleSourceSelectionComboButton();
	void HandleVirtualSourceSelection(const FGuid& InSourceGuid);
	bool IsVirtualSourceSelected(const FGuid& InSourceGuid);
	void OnSaveAsPreset();
	void OnImportPreset(const FAssetData& InPreset);
	FReply OnRevertChanges();
	bool HasLoadedLiveLinkPreset() const;

	void PopulateVirtualSubjectSourceCreationMenu(FMenuBuilder& InMenuBuilder);
	FReply OnAddVirtualSubjectSource();
	void AddVirtualSubject();


private:
	FLiveLinkClient* Client;

	TWeakPtr<IMenu> AddSubjectMenu;
	TWeakPtr<IMenu> VirtualSubjectMenu;
	TWeakPtr<STextEntryPopup> VirtualSubjectPopup;
	TWeakPtr<SEditableTextBox> VirtualSubjectSourceName;

	TWeakObjectPtr<ULiveLinkPreset> LiveLinkPreset;
	TArray<TObjectPtr<ULiveLinkSourceFactory>> Factories;
};
