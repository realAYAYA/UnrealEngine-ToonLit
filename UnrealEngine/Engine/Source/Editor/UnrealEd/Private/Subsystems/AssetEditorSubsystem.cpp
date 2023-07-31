// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/AssetEditorSubsystem.h"
#include "AssetEditorMessages.h"
#include "MessageEndpoint.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "UObject/Package.h"
#include "CoreGlobals.h"
#include "AssetToolsModule.h"
#include "IMessageContext.h"
#include "LevelEditor.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Engine/MapBuildDataRegistry.h"
#include "ContentBrowserModule.h"
#include "MRUFavoritesList.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "PackageTools.h"
#include "UObject/PackageReload.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/NamePermissionList.h"
#include "StudioAnalytics.h"
#include "EditorModeRegistry.h"
#include "Tools/UEdMode.h"
#include "AssetEditorMessages.h"
#include "EditorModeManager.h"
#include "Tools/LegacyEdMode.h"
#include "ProfilingDebugging/StallDetector.h"

#include "Elements/SMInstance/SMInstanceElementData.h" // For SMInstanceElementDataUtil::SMInstanceElementsEnabled


#define LOCTEXT_NAMESPACE "AssetEditorSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogAssetEditorSubsystem, Log, All);

UAssetEditorSubsystem::UAssetEditorSubsystem()
	: Super()
	, bSavingOnShutdown(false)
	, bAutoRestoreAndDisableSaving(false)
	, bRequestRestorePreviouslyOpenAssets(false)
{
}

void UAssetEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	TickDelegate = FTickerDelegate::CreateUObject(this, &UAssetEditorSubsystem::HandleTicker);
	FTSTicker::GetCoreTicker().AddTicker(TickDelegate, 1.f);

	FCoreUObjectDelegates::OnPackageReloaded.AddUObject(this, &UAssetEditorSubsystem::HandlePackageReloaded);

	GEditor->OnEditorClose().AddUObject(this, &UAssetEditorSubsystem::OnEditorClose);
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UAssetEditorSubsystem::UnregisterEditorModes);
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UAssetEditorSubsystem::RegisterEditorModes);

	SMInstanceElementDataUtil::OnSMInstanceElementsEnabledChanged().AddUObject(this, &UAssetEditorSubsystem::OnSMInstanceElementsEnabled);
}

void UAssetEditorSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);
	GEditor->OnEditorClose().RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	SMInstanceElementDataUtil::OnSMInstanceElementsEnabledChanged().RemoveAll(this);

	// Don't attempt to report usage stats if analytics isn't available
	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> EditorUsageAttribs;
		EditorUsageAttribs.Empty(2);
		for (auto Iter = EditorUsageAnalytics.CreateConstIterator(); Iter; ++Iter)
		{
			const FAssetEditorAnalyticInfo& Data = Iter.Value();
			EditorUsageAttribs.Reset();
			EditorUsageAttribs.Emplace(TEXT("TotalDuration.Seconds"), FString::Printf(TEXT("%.1f"), Data.SumDuration.GetTotalSeconds()));
			EditorUsageAttribs.Emplace(TEXT("OpenedInstances.Count"), FString::Printf(TEXT("%d"), Data.NumTimesOpened));

			const FString EventName = FString::Printf(TEXT("Editor.Usage.%s"), *Iter.Key().ToString());
			FEngineAnalytics::GetProvider().RecordEvent(EventName, EditorUsageAttribs);
		}
	}
}

void UAssetEditorSubsystem::OnEditorClose()
{
	SaveOpenAssetEditors(true);
	TGuardValue<bool> GuardOnShutdown(bSavingOnShutdown, true);
	CloseAllAssetEditors();
}

IAssetEditorInstance* UAssetEditorSubsystem::FindEditorForAsset(UObject* Asset, bool bFocusIfOpen)
{
	const TArray<IAssetEditorInstance*> AssetEditors = FindEditorsForAsset(Asset);

	IAssetEditorInstance* const * PrimaryEditor = AssetEditors.FindByPredicate([](IAssetEditorInstance* Editor) { return Editor->IsPrimaryEditor(); });

	const bool bEditorOpen = PrimaryEditor != NULL;
	if (bEditorOpen && bFocusIfOpen)
	{
		// @todo toolkit minor: We may need to handle this differently for world-centric vs standalone.  (multiple level editors, etc)
		(*PrimaryEditor)->FocusWindow(Asset);
	}

	return bEditorOpen ? *PrimaryEditor : NULL;
}


TArray<IAssetEditorInstance*> UAssetEditorSubsystem::FindEditorsForAsset(UObject* Asset)
{
	TArray<IAssetEditorInstance*> AssetEditors;
	OpenedAssets.MultiFind(Asset, AssetEditors);
	return AssetEditors;
}

TArray<IAssetEditorInstance*> UAssetEditorSubsystem::FindEditorsForAssetAndSubObjects(UObject* Asset)
{
	TArray<IAssetEditorInstance*> EditorInstances;

	for (const TPair<FAssetEntry, IAssetEditorInstance*>& Pair : OpenedAssets)
	{
		if (Pair.Key.RawPtr == Asset || (Pair.Key.ObjectPtr.IsValid() && Pair.Key.ObjectPtr.Get()->IsIn(Asset)))
		{		
			EditorInstances.Add(Pair.Value);
		}
	}

	return EditorInstances;
}

int32 UAssetEditorSubsystem::CloseAllEditorsForAsset(UObject* Asset)
{
	TArray<IAssetEditorInstance*> EditorInstances = FindEditorsForAssetAndSubObjects(Asset);

	for (IAssetEditorInstance* EditorInstance : EditorInstances)
	{
		if (EditorInstance)
		{
			EditorInstance->CloseWindow();
		}
	}

	AssetEditorRequestCloseEvent.Broadcast(Asset, EAssetEditorCloseReason::CloseAllEditorsForAsset);

	return EditorInstances.Num();
}


void UAssetEditorSubsystem::RemoveAssetFromAllEditors(UObject* Asset)
{
	TArray<IAssetEditorInstance*> EditorInstances = FindEditorsForAsset(Asset);

	for (IAssetEditorInstance* EditorIter : EditorInstances)
	{
		if (EditorIter)
		{
			EditorIter->RemoveEditingAsset(Asset);
		}
	}

	AssetEditorRequestCloseEvent.Broadcast(Asset, EAssetEditorCloseReason::RemoveAssetFromAllEditors);
}


void UAssetEditorSubsystem::CloseOtherEditors(UObject* Asset, IAssetEditorInstance* OnlyEditor)
{
	TArray<UObject*> AllAssets;
	for (TMultiMap<FAssetEntry, IAssetEditorInstance*>::TIterator It(OpenedAssets); It; ++It)
	{
		IAssetEditorInstance* Editor = It.Value();
		if (Asset == It.Key().RawPtr && Editor != OnlyEditor)
		{
			Editor->CloseWindow();
		}
	}

	AssetEditorRequestCloseEvent.Broadcast(Asset, EAssetEditorCloseReason::CloseOtherEditors);
}


TArray<UObject*> UAssetEditorSubsystem::GetAllEditedAssets()
{
	TArray<UObject*> AllAssets;
	for (TMultiMap<FAssetEntry, IAssetEditorInstance*>::TIterator It(OpenedAssets); It; ++It)
	{
		UObject* Asset = It.Key().ObjectPtr.Get();
		if (Asset != nullptr)
		{
			AllAssets.AddUnique(Asset);
		}
	}
	return AllAssets;
}


void UAssetEditorSubsystem::NotifyEditorOpeningPreWidgets(const TArray< UObject* >& Assets, IAssetEditorInstance* InInstance)
{
	EditorOpeningPreWidgetsEvent.Broadcast(Assets, InInstance);
}


void UAssetEditorSubsystem::NotifyAssetOpened(UObject* Asset, IAssetEditorInstance* InInstance)
{
	if (!OpenedEditors.Contains(InInstance))
	{
		FOpenedEditorTime EditorTime;
		EditorTime.EditorName = InInstance->GetEditorName();
		EditorTime.OpenedTime = FDateTime::UtcNow();

		OpenedEditorTimes.Add(InInstance, EditorTime);
	}

	OpenedAssets.Add(Asset, InInstance);
	OpenedEditors.Add(InInstance, Asset);

	AssetOpenedInEditorEvent.Broadcast(Asset, InInstance);

	SaveOpenAssetEditors(false);
}


void UAssetEditorSubsystem::NotifyAssetsOpened(const TArray< UObject* >& Assets, IAssetEditorInstance* InInstance)
{
	for (auto AssetIter = Assets.CreateConstIterator(); AssetIter; ++AssetIter)
	{
		NotifyAssetOpened(*AssetIter, InInstance);
	}
}


void UAssetEditorSubsystem::NotifyAssetClosed(UObject* Asset, IAssetEditorInstance* InInstance)
{
	OpenedEditors.RemoveSingle(InInstance, Asset);
	OpenedAssets.RemoveSingle(Asset, InInstance);

	SaveOpenAssetEditors(false);
}


void UAssetEditorSubsystem::NotifyEditorClosed(IAssetEditorInstance* InInstance)
{
	// Remove all assets associated with the editor
	TArray<FAssetEntry> Assets;
	OpenedEditors.MultiFind(InInstance, /*out*/ Assets);
	for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
	{
		OpenedAssets.Remove(Assets[AssetIndex], InInstance);
	}

	// Remove the editor itself
	OpenedEditors.Remove(InInstance);
	FOpenedEditorTime EditorTime = OpenedEditorTimes.FindAndRemoveChecked(InInstance);

	// Record the editor open-close duration
	FAssetEditorAnalyticInfo& AnalyticsForThisAsset = EditorUsageAnalytics.FindOrAdd(EditorTime.EditorName);
	AnalyticsForThisAsset.SumDuration += FDateTime::UtcNow() - EditorTime.OpenedTime;
	AnalyticsForThisAsset.NumTimesOpened++;

	SaveOpenAssetEditors(false);
}


bool UAssetEditorSubsystem::CloseAllAssetEditors()
{
	bool bAllEditorsClosed = true;
	for (TMultiMap<IAssetEditorInstance*, FAssetEntry>::TIterator It(OpenedEditors); It; ++It)
	{
		IAssetEditorInstance* Editor = It.Key();
		if (Editor != nullptr)
		{
			if (!Editor->CloseWindow())
			{
				bAllEditorsClosed = false;
			}
		}
	}

	AssetEditorRequestCloseEvent.Broadcast(nullptr, EAssetEditorCloseReason::CloseAllAssetEditors);

	return bAllEditorsClosed;
}

bool UAssetEditorSubsystem::IsAssetEditable(const UObject* Asset)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	if (!Asset)
	{
		return false;
	}

	if (UPackage* Package = Asset->GetPackage())
	{
		if (Package->bIsCookedForEditor)
		{
			return false;
		}

		if (!AssetToolsModule.Get().GetWritableFolderPermissionList()->PassesStartsWithFilter(Package->GetName()))
		{
			return false;
		}
	}

	return true;
}

bool UAssetEditorSubsystem::OpenEditorForAsset(UObject* Asset, const EToolkitMode::Type ToolkitMode, TSharedPtr< IToolkitHost > OpenedFromLevelEditor, const bool bShowProgressWindow)
{
	SCOPE_STALL_REPORTER(UAssetEditorSubsystem::OpenEditorForAsset, 2.0);

	const double OpenAssetStartTime = FStudioAnalytics::GetAnalyticSeconds();

	if (!Asset)
	{
		UE_LOG(LogAssetEditorSubsystem, Error, TEXT("Opening Asset editor failed because asset is null"));
		return false;
	}

	// @todo toolkit minor: When "Edit Here" happens in a different level editor from the one that an asset is already
	//    being edited within, we should decide whether to disallow "Edit Here" in that case, or to close the old asset
	//    editor and summon it in the new level editor, or to just foreground the old level editor (current behavior)

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const bool bBringToFrontIfOpen = true;

	if (UPackage* Package = Asset->GetOutermost())
	{
		// Don't open asset editors for cooked packages
		if (Package->bIsCookedForEditor)
		{
			return false;
		}

		if (!AssetToolsModule.Get().GetWritableFolderPermissionList()->PassesStartsWithFilter(Package->GetName()))
		{
			AssetToolsModule.Get().NotifyBlockedByWritableFolderFilter();
			return false;
		}
	}

	AssetEditorRequestOpenEvent.Broadcast(Asset);

	if (FindEditorForAsset(Asset, bBringToFrontIfOpen) != nullptr)
	{
		// This asset is already open in an editor! (the call to FindEditorForAsset above will bring it to the front)
		return true;
	}
	else
	{
		if (bShowProgressWindow)
		{
			GWarn->BeginSlowTask(LOCTEXT("OpenEditor", "Opening Editor..."), true);
		}
	}

	UE_LOG(LogAssetEditorSubsystem, Log, TEXT("Opening Asset editor for %s"), *Asset->GetFullName());

	TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Asset->GetClass());

	EToolkitMode::Type ActualToolkitMode = ToolkitMode;
	if (AssetTypeActions.IsValid())
	{
		if (AssetTypeActions.Pin()->ShouldForceWorldCentric())
		{
			// This asset type prefers a specific toolkit mode
			ActualToolkitMode = EToolkitMode::WorldCentric;

			if (!OpenedFromLevelEditor.IsValid())
			{
				// We don't have a level editor to spawn in world-centric mode, so we'll find one now
				// @todo sequencer: We should eventually eliminate this code (incl include dependencies) or change it to not make assumptions about a single level editor
				OpenedFromLevelEditor = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor").GetFirstLevelEditor();
			}
		}
	}

	if (ActualToolkitMode != EToolkitMode::WorldCentric && OpenedFromLevelEditor.IsValid())
	{
		// @todo toolkit minor: Kind of lame use of a static variable here to prime the new asset editor.  This was done to avoid refactoring a few dozen files for a very minor change.
		FAssetEditorToolkit::SetPreviousWorldCentricToolkitHostForNewAssetEditor(OpenedFromLevelEditor.ToSharedRef());
	}

	// Disallow opening an asset editor for classes
	bool bCanSummonSimpleAssetEditor = !Asset->IsA<UClass>();

	if (AssetTypeActions.IsValid())
	{
		TArray<UObject*> AssetsToEdit;
		AssetsToEdit.Add(Asset);

		// Some assets (like UWorlds) may be destroyed and recreated as part of opening. To protect against this, keep the path to the asset and try to re-find it if it disappeared.
		TWeakObjectPtr<UObject> WeakAsset = Asset;
		const FString AssetPath = Asset->GetPathName();

		AssetTypeActions.Pin()->OpenAssetEditor(AssetsToEdit, ActualToolkitMode == EToolkitMode::WorldCentric ? OpenedFromLevelEditor : TSharedPtr<IToolkitHost>());

		// If the Asset was destroyed, attempt to find it if it was recreated
		if (!WeakAsset.IsValid() && !AssetPath.IsEmpty())
		{
			Asset = FindObject<UObject>(nullptr, *AssetPath);
		}

		AssetEditorOpenedEvent.Broadcast(Asset);
	}
	else if (bCanSummonSimpleAssetEditor)
	{
		// No asset type actions for this asset. Just use a properties editor.
		FSimpleAssetEditor::CreateEditor(ActualToolkitMode, ActualToolkitMode == EToolkitMode::WorldCentric ? OpenedFromLevelEditor : TSharedPtr<IToolkitHost>(), Asset);
	}

	if (bShowProgressWindow)
	{
		GWarn->EndSlowTask();
	}
	// Must check Asset here in addition to at the beginning of the function, because if the asset was destroyed and recreated it might not be found correctly
	// Do not add to recently opened asset list if this is a level-associated asset like Level Blueprint or Built Data. Their naming is not compatible
	if (Asset)
	{
		if (Asset->IsAsset() && !Asset->IsA(UMapBuildDataRegistry::StaticClass()))
		{
			FString AssetPath = Asset->GetOuter()->GetPathName();
			FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
			FMainMRUFavoritesList* RecentlyOpenedAssets = CBModule.GetRecentlyOpenedAssets();
			if (RecentlyOpenedAssets && FPackageName::IsValidLongPackageName(AssetPath))
			{
				RecentlyOpenedAssets->AddMRUItem(AssetPath);
			}
		}

		const double OpenTime = FStudioAnalytics::GetAnalyticSeconds() - OpenAssetStartTime;
		FStudioAnalytics::FireEvent_Loading(TEXT("OpenAssetEditor"), OpenTime, {
			FAnalyticsEventAttribute(TEXT("AssetPath"), Asset->GetFullName()),
			FAnalyticsEventAttribute(TEXT("AssetType"), Asset->GetClass()->GetName())
		});
	}

	return true;
}


bool UAssetEditorSubsystem::OpenEditorForAssets_Advanced(const TArray <UObject* >& InAssets, const EToolkitMode::Type ToolkitMode, TSharedPtr< IToolkitHost > OpenedFromLevelEditor)
{
	TArray<UObject*> Assets;
	Assets.Reserve(InAssets.Num());
	int32 NumNullAssets = 0;
	for (UObject* Asset : InAssets)
	{
		if (Asset)
		{
			Assets.AddUnique(Asset);
		}
		else
		{
			++NumNullAssets;
		}
	}

	if (NumNullAssets > 1)
	{
		UE_LOG(LogAssetEditorSubsystem, Error, TEXT("Opening Asset editors failed because of null assets"));
	}
	else if (NumNullAssets > 0)
	{
		UE_LOG(LogAssetEditorSubsystem, Error, TEXT("Opening Asset editor failed because of null asset"));
	}

	if (Assets.Num() == 1)
	{
		return OpenEditorForAsset(Assets[0], ToolkitMode, OpenedFromLevelEditor);
	}
	else if (Assets.Num() > 0)
	{
		TArray<UObject*> SkipOpenAssets;
		for (UObject* Asset : Assets)
		{
			// If any of the assets are already open or the package is cooked,
			// remove them from the list of assets to open an editor for
			UPackage* Package = Asset->GetOutermost();
			if (FindEditorForAsset(Asset, true) != nullptr || (Package && Package->bIsCookedForEditor))
			{
				SkipOpenAssets.Add(Asset);
			}
		}

		// Verify that all the assets are of the same class
		bool bAssetClassesMatch = true;
		UClass* AssetClass = Assets[0]->GetClass();
		for (int32 i = 1; i < Assets.Num(); i++)
		{
			if (Assets[i]->GetClass() != AssetClass)
			{
				bAssetClassesMatch = false;
				break;
			}
		}

		// If the classes don't match or any of the selected assets are already open, just open each asset in its own editor.
		if (bAssetClassesMatch && SkipOpenAssets.Num() == 0)
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(AssetClass);

			if (AssetTypeActions.IsValid())
			{
				GWarn->BeginSlowTask(LOCTEXT("OpenEditors", "Opening Editor(s)..."), true);

				// Determine the appropriate toolkit mode for the asset type
				EToolkitMode::Type ActualToolkitMode = ToolkitMode;
				if (AssetTypeActions.Pin()->ShouldForceWorldCentric())
				{
					// This asset type prefers a specific toolkit mode
					ActualToolkitMode = EToolkitMode::WorldCentric;

					if (!OpenedFromLevelEditor.IsValid())
					{
						// We don't have a level editor to spawn in world-centric mode, so we'll find one now
						// @todo sequencer: We should eventually eliminate this code (incl include dependencies) or change it to not make assumptions about a single level editor
						OpenedFromLevelEditor = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor").GetFirstLevelEditor();
					}
				}

				if (ActualToolkitMode != EToolkitMode::WorldCentric && OpenedFromLevelEditor.IsValid())
				{
					// @todo toolkit minor: Kind of lame use of a static variable here to prime the new asset editor.  This was done to avoid refactoring a few dozen files for a very minor change.
					FAssetEditorToolkit::SetPreviousWorldCentricToolkitHostForNewAssetEditor(OpenedFromLevelEditor.ToSharedRef());
				}

				// Some assets (like UWorlds) may be destroyed and recreated as part of opening. To protect against this, keep the path to each asset and try to re-find any if they disappear.
				struct FLocalAssetInfo
				{
					TWeakObjectPtr<UObject> WeakAsset;
					FString AssetPath;

					FLocalAssetInfo(const TWeakObjectPtr<UObject>& InWeakAsset, const FString& InAssetPath)
						: WeakAsset(InWeakAsset), AssetPath(InAssetPath) {}
				};

				TArray<FLocalAssetInfo> AssetInfoList;
				AssetInfoList.Reserve(Assets.Num());
				for (UObject* Asset : Assets)
				{
					AssetInfoList.Add(FLocalAssetInfo(Asset, Asset->GetPathName()));
				}

				// How to handle multiple assets is left up to the type actions (i.e. open a single shared editor or an editor for each)
				AssetTypeActions.Pin()->OpenAssetEditor(Assets, ActualToolkitMode == EToolkitMode::WorldCentric ? OpenedFromLevelEditor : TSharedPtr<IToolkitHost>());

				// If any assets were destroyed, attempt to find them if they were recreated
				for (int32 i = 0; i < Assets.Num(); i++)
				{
					const FLocalAssetInfo& AssetInfo = AssetInfoList[i];
					UObject* Asset = Assets[i];

					if (!AssetInfo.WeakAsset.IsValid() && !AssetInfo.AssetPath.IsEmpty())
					{
						Asset = FindObject<UObject>(nullptr, *AssetInfo.AssetPath);
					}
				}

				//@todo if needed, broadcast the event for every asset. It is possible, however, that a single shared editor was opened by the AssetTypeActions, not an editor for each asset.
				/*AssetEditorOpenedEvent.Broadcast(Asset);*/

				GWarn->EndSlowTask();
			}
		}
		else
		{
			// Asset types don't match or some are already open, so just open individual editors for the unopened ones
			for (UObject* Asset : Assets)
			{
				if (!SkipOpenAssets.Contains(Asset))
				{
					OpenEditorForAsset(Asset, ToolkitMode, OpenedFromLevelEditor);
				}
			}
		}
	}

	return NumNullAssets == 0;
}

bool UAssetEditorSubsystem::OpenEditorForAssets(const TArray<UObject*>& Assets)
{
	return OpenEditorForAssets_Advanced(Assets, EToolkitMode::Standalone, TSharedPtr<IToolkitHost>());
}

void UAssetEditorSubsystem::HandleRequestOpenAssetMessage(const FAssetEditorRequestOpenAsset& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	OpenEditorForAsset(Message.AssetName);
}

void UAssetEditorSubsystem::OpenEditorForAsset(const FSoftObjectPath& AssetPath)
{
	OpenEditorForAsset(AssetPath.ToString());
}

void UAssetEditorSubsystem::OpenEditorForAsset(const FString& AssetPathName)
{
	// An asset needs loading
	UPackage* Package = LoadPackage(NULL, *AssetPathName, LOAD_NoRedirects);

	if (Package)
	{
		Package->FullyLoad();

		FString AssetName = FPaths::GetBaseFilename(AssetPathName);
		UObject* Object = FindObject<UObject>(Package, *AssetName);

		if (Object != NULL)
		{
			OpenEditorForAsset(Object);
		}
	}
}

bool UAssetEditorSubsystem::HandleTicker(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UAssetEditorSubsystem_HandleTicker);

	if (bRequestRestorePreviouslyOpenAssets)
	{
		RestorePreviouslyOpenAssets();
		bRequestRestorePreviouslyOpenAssets = false;
	}

	return true;
}

void UAssetEditorSubsystem::RequestRestorePreviouslyOpenAssets()
{
	// We defer the restore so that we guarantee that it happens once initialization is complete
	bRequestRestorePreviouslyOpenAssets = true;
}

UEdMode* UAssetEditorSubsystem::CreateEditorModeWithToolsOwner(FEditorModeID ModeID, FEditorModeTools& Owner)
{
	FRegisteredModeInfo* ScriptableMode = EditorModes.Find(ModeID);
	if (ScriptableMode && ScriptableMode->ModeClass.IsValid())
	{
		UEdMode* Instance = NewObject<UEdMode>(GetTransientPackage(), ScriptableMode->ModeClass.Get());
		Instance->Owner = &Owner;
		Instance->Initialize();

		return Instance;
	}

	// If we couldn't find a valid UEdMode based class, attempt to make a UEdMode wrapped FEdMode
	FEditorModeInfo LegacyModeInfo;
	if (FindEditorModeInfo(ModeID, LegacyModeInfo))
	{
		ULegacyEdModeWrapper* LegacyEditorMode = NewObject<ULegacyEdModeWrapper>(GetTransientPackage());
		if (LegacyEditorMode->CreateLegacyMode(ModeID, Owner))
		{
			LegacyEditorMode->Initialize();
			return LegacyEditorMode;
		}
	}

	return nullptr;
}

bool UAssetEditorSubsystem::FindEditorModeInfo(const FEditorModeID& InModeID, FEditorModeInfo& OutModeInfo) const
{
	if (!IsEditorModeAllowed(InModeID))
	{
		return false;
	}
	
	const TSharedRef<IEditorModeFactory>* ModeFactory = FEditorModeRegistry::Get().GetFactoryMap().Find(InModeID);
	if (ModeFactory)
	{
		OutModeInfo = (*ModeFactory)->GetModeInfo();
		return true;
	}

	if (!EditorModes.Contains(InModeID))
	{
		return false;
	}

	OutModeInfo = EditorModes[InModeID].ModeInfo;
	return true;
}

TArray<FEditorModeInfo> UAssetEditorSubsystem::GetEditorModeInfoOrderedByPriority() const
{
	TArray<FEditorModeInfo> ModeInfoArray;

	for (const auto& Pair : FEditorModeRegistry::Get().GetFactoryMap())
	{
		FEditorModeInfo ModeInfo = Pair.Value->GetModeInfo();
		if (IsEditorModeAllowed(ModeInfo.ID))
		{
			ModeInfoArray.Add(MoveTemp(ModeInfo));
		}
	}
	for (const auto& EditorMode : EditorModes)
	{
		const FEditorModeInfo& ModeInfo = EditorMode.Value.ModeInfo;
		if (IsEditorModeAllowed(ModeInfo.ID))
		{
			ModeInfoArray.Add(ModeInfo);
		}
	}

	ModeInfoArray.Sort([](const FEditorModeInfo& A, const FEditorModeInfo& B) {
		return A.PriorityOrder < B.PriorityOrder;
	});

	return ModeInfoArray;
}




void UAssetEditorSubsystem::RegisterUAssetEditor(UAssetEditor* NewAssetEditor)
{
	OwnedAssetEditors.Add(NewAssetEditor);
}

void UAssetEditorSubsystem::UnregisterUAssetEditor(UAssetEditor* RemovedAssetEditor)
{
	OwnedAssetEditors.Remove(RemovedAssetEditor);
}

FRegisteredModesChangedEvent& UAssetEditorSubsystem::OnEditorModesChanged()
{
	return OnEditorModesChangedEvent;
}

FOnModeRegistered& UAssetEditorSubsystem::OnEditorModeRegistered()
{
	return OnEditorModeRegisteredEvent;
}

FOnModeUnregistered& UAssetEditorSubsystem::OnEditorModeUnregistered()
{
	return OnEditorModeUnregisteredEvent;
}

void UAssetEditorSubsystem::RestorePreviouslyOpenAssets()
{
	TArray<FString> OpenAssets;
	GConfig->GetArray(TEXT("AssetEditorSubsystem"), TEXT("OpenAssetsAtExit"), OpenAssets, GEditorPerProjectIni);

	bool bCleanShutdown = true;
	GConfig->GetBool(TEXT("AssetEditorSubsystem"), TEXT("CleanShutdown"), bCleanShutdown, GEditorPerProjectIni);

	SaveOpenAssetEditors(false);

	if (OpenAssets.Num() > 0)
	{
		if (bCleanShutdown)
		{
			// Do we have permission to automatically re-open the assets, or should we ask?
			const bool bAutoRestore = GetDefault<UEditorLoadingSavingSettings>()->bRestoreOpenAssetTabsOnRestart || bAutoRestoreAndDisableSaving;

			if (bAutoRestore)
			{
				// Pretend that we showed the notification and that the user clicked "Restore Now"
				OpenEditorsForAssets(OpenAssets);
			}
			else
			{
				// Has this notification previously been suppressed by the user?
				bool bSuppressNotification = false;
				GConfig->GetBool(TEXT("AssetEditorSubsystem"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);

				if (!bSuppressNotification)
				{
					// Ask the user; this doesn't block so will reopen the assets later
					SpawnRestorePreviouslyOpenAssetsNotification(bCleanShutdown, OpenAssets);
				}
			}
		}
		else
		{
			// If we crashed, we always ask regardless of what the user previously said
			SpawnRestorePreviouslyOpenAssetsNotification(bCleanShutdown, OpenAssets);
		}
	}
}

void UAssetEditorSubsystem::SetAutoRestoreAndDisableSaving(const bool bInAutoRestoreAndDisableSaving)
{
	bAutoRestoreAndDisableSaving = bInAutoRestoreAndDisableSaving;

	// Disable any pending request to avoid trying to restore previously opened assets twice
	bRequestRestorePreviouslyOpenAssets = false;
}

void UAssetEditorSubsystem::SpawnRestorePreviouslyOpenAssetsNotification(const bool bCleanShutdown, const TArray<FString>& AssetsToOpen)
{
	/** Utility functions for notifications */
	struct Local
	{
		static ECheckBoxState GetDontAskAgainCheckBoxState()
		{
			bool bSuppressNotification = false;
			GConfig->GetBool(TEXT("AssetEditorSubsystem"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);
			return bSuppressNotification ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		static void OnDontAskAgainCheckBoxStateChanged(ECheckBoxState NewState)
		{
			const bool bSuppressNotification = (NewState == ECheckBoxState::Checked);
			GConfig->SetBool(TEXT("AssetEditorSubsystem"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);
		}
	};

	FText NotificationMessage = bCleanShutdown
		? LOCTEXT("ReopenAssetEditorsAfterClose", "{0} asset {0}|plural(one=editor was,other=editors were) open when the editor was last closed. Would you like to re-open them?")
		: LOCTEXT("ReopenAssetEditorsAfterCrash", "{0} asset {0}|plural(one=editor was,other=editors were) open when the editor quit unexpectedly. Would you like to re-open them?");
	NotificationMessage = FText::Format(NotificationMessage, AssetsToOpen.Num());

	FNotificationInfo Info = FNotificationInfo(NotificationMessage);

	// Add the buttons
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("ReopenAssetEditors_Confirm", "Open"),
		FText(),
		FSimpleDelegate::CreateUObject(this, &UAssetEditorSubsystem::OnConfirmRestorePreviouslyOpenAssets, AssetsToOpen),
		SNotificationItem::CS_None
	));
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("ReopenAssetEditors_Cancel", "Cancel"),
		FText(),
		FSimpleDelegate::CreateUObject(this, &UAssetEditorSubsystem::OnCancelRestorePreviouslyOpenAssets),
		SNotificationItem::CS_None
	));

	// We will let the notification expire automatically after 10 seconds
	Info.bFireAndForget = true;
	Info.ExpireDuration = 20.0f;

	// We want the auto-save to be subtle
	Info.bUseLargeFont = false;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = false;

	// Only let the user suppress the non-crash version
	if (bCleanShutdown)
	{
		Info.CheckBoxState = TAttribute<ECheckBoxState>::Create(&Local::GetDontAskAgainCheckBoxState);
		Info.CheckBoxStateChanged = FOnCheckStateChanged::CreateStatic(&Local::OnDontAskAgainCheckBoxStateChanged);
		Info.CheckBoxText = NSLOCTEXT("ModalDialogs", "DefaultCheckBoxMessage", "Don't show this again");
	}

	// Close any existing notification
	TSharedPtr<SNotificationItem> RestorePreviouslyOpenAssetsNotification = RestorePreviouslyOpenAssetsNotificationPtr.Pin();
	if (RestorePreviouslyOpenAssetsNotification.IsValid())
	{
		RestorePreviouslyOpenAssetsNotification->ExpireAndFadeout();
	}

	RestorePreviouslyOpenAssetsNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
}

void UAssetEditorSubsystem::OnConfirmRestorePreviouslyOpenAssets(TArray<FString> AssetsToOpen)
{
	// Close any existing notification
	TSharedPtr<SNotificationItem> RestorePreviouslyOpenAssetsNotification = RestorePreviouslyOpenAssetsNotificationPtr.Pin();
	if (RestorePreviouslyOpenAssetsNotification.IsValid())
	{
		RestorePreviouslyOpenAssetsNotification->SetExpireDuration(0.0f);
		RestorePreviouslyOpenAssetsNotification->SetFadeOutDuration(0.5f);
		RestorePreviouslyOpenAssetsNotification->ExpireAndFadeout();

		// If the user suppressed the notification for future sessions, make sure this is reflected in their settings
		// Note: We do that inside this if statement so that we only do it if we were showing a UI they could interact with
		bool bSuppressNotification = false;
		GConfig->GetBool(TEXT("AssetEditorSubsystem"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);
		UEditorLoadingSavingSettings& Settings = *GetMutableDefault<UEditorLoadingSavingSettings>();
		Settings.bRestoreOpenAssetTabsOnRestart = bSuppressNotification;
		Settings.PostEditChange();

		// we do this inside the condition so that it can only be done once. 
		OpenEditorsForAssets(AssetsToOpen);

	}
}

void UAssetEditorSubsystem::OnCancelRestorePreviouslyOpenAssets()
{
	// Close any existing notification
	TSharedPtr<SNotificationItem> RestorePreviouslyOpenAssetsNotification = RestorePreviouslyOpenAssetsNotificationPtr.Pin();
	if (RestorePreviouslyOpenAssetsNotification.IsValid())
	{
		RestorePreviouslyOpenAssetsNotification->SetExpireDuration(0.0f);
		RestorePreviouslyOpenAssetsNotification->SetFadeOutDuration(0.5f);
		RestorePreviouslyOpenAssetsNotification->ExpireAndFadeout();
	}
}

void UAssetEditorSubsystem::SaveOpenAssetEditors(const bool bOnShutdown)
{
	if (!bSavingOnShutdown && !bAutoRestoreAndDisableSaving)
	{
		TArray<FString> OpenAssets;
		if (bOnShutdown || !FPlatformMisc::IsDebuggerPresent())
		{
			for (const TPair<IAssetEditorInstance*, FAssetEntry>& EditorPair : OpenedEditors)
			{
				IAssetEditorInstance* Editor = EditorPair.Key;
				if (Editor != nullptr)
				{
					UObject* EditedObject = EditorPair.Value.ObjectPtr.Get();
					if (EditedObject != nullptr)
					{
						// only record assets that have a valid saved package
						UPackage* Package = EditedObject->GetOutermost();
						if (Package != nullptr && Package->GetFileSize() != 0)
						{
							OpenAssets.Add(EditedObject->GetPathName());
						}
					}
				}
			}
		}
		GConfig->SetArray(TEXT("AssetEditorSubsystem"), TEXT("OpenAssetsAtExit"), OpenAssets, GEditorPerProjectIni);
		GConfig->SetBool(TEXT("AssetEditorSubsystem"), TEXT("CleanShutdown"), bOnShutdown, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}
}

void UAssetEditorSubsystem::SaveOpenAssetEditors(const bool bOnShutdown, const bool bCancelIfDebugger)
{
	SaveOpenAssetEditors(bOnShutdown);
}

void UAssetEditorSubsystem::HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
{
	static TArray<TWeakObjectPtr<UObject>> PendingAssetsToOpen;

	if (InPackageReloadPhase == EPackageReloadPhase::PrePackageFixup)
	{
		/** Call close for all old assets even if not open, so global callback will go off */
		TArray<UObject*> ObjectsToClose;
		const TMap<UObject*, UObject*>& RepointedMap = InPackageReloadedEvent->GetRepointedObjects();

		for (const TPair<UObject*, UObject*>& RepointPair : RepointedMap)
		{
			if (RepointPair.Key->IsAsset())
			{
				ObjectsToClose.Add(RepointPair.Key);
			}
		}

		/** Look for replacement for assets that are open now so we can reopen */
		for (TPair<FAssetEntry, IAssetEditorInstance*>& AssetEditorPair : OpenedAssets)
		{
			UObject* NewAsset = nullptr;
			if (InPackageReloadedEvent->GetRepointedObject(AssetEditorPair.Key.ObjectPtr.Get(), NewAsset))
			{
				if (NewAsset)
				{
					PendingAssetsToOpen.AddUnique(NewAsset);
				}

				UObject* OldAsset = AssetEditorPair.Key.RawPtr; // Not validating the asset here since we'd want to close editors for garbage collected assets
				ObjectsToClose.AddUnique(OldAsset);

				if (AssetEditorPair.Key.ObjectPtr.IsValid())
				{
					// The asset being reloaded might have other assets that depend on it. Find the list of
					// external referencers to this asset and mark them to be closed and reopened as well.
					TArray<FReferencerInformation> AssetInternalReferencers, AssetExternalReferencers;
					AssetEditorPair.Key.ObjectPtr.Get()->RetrieveReferencers(&AssetInternalReferencers, &AssetExternalReferencers);
					for (const FReferencerInformation& Ref : AssetExternalReferencers)
					{
						ObjectsToClose.AddUnique(Ref.Referencer);

						if (!FindEditorsForAssetAndSubObjects(Ref.Referencer).IsEmpty())
						{
							PendingAssetsToOpen.AddUnique(Ref.Referencer);
						}
					}
				}
			}
		}

		int32 NumAssetEditorsClosed = 0;
		for (UObject* OldAsset : ObjectsToClose)
		{
			NumAssetEditorsClosed += CloseAllEditorsForAsset(OldAsset);
		}

		if (NumAssetEditorsClosed > 0)
		{
			// Closing asset editors might have have left objects pending GC that still reference the asset we're about to reload
			// Run a GC now to ensure those are cleaned up before the fix-up phase happens
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

	}

	if (InPackageReloadPhase == EPackageReloadPhase::PostBatchPostGC)
	{
		for (TWeakObjectPtr<UObject>& NewAsset : PendingAssetsToOpen)
		{
			if (NewAsset.IsValid())
			{
				OpenEditorForAsset(NewAsset.Get());
			}
		}
		PendingAssetsToOpen.Reset();
	}
}

void UAssetEditorSubsystem::OpenEditorsForAssets(const TArray<FSoftObjectPath>& AssetsToOpen)
{
	for (const FSoftObjectPath& AssetName : AssetsToOpen)
	{
		OpenEditorForAsset(AssetName);
	}
}

void UAssetEditorSubsystem::OpenEditorsForAssets(const TArray<FString>& AssetsToOpen)
{
	for (const FString& AssetName : AssetsToOpen)
	{
		OpenEditorForAsset(AssetName);
	}
}

void UAssetEditorSubsystem::OpenEditorsForAssets(const TArray<FName>& AssetsToOpen)
{
	for (const FName& AssetName : AssetsToOpen)
	{
		OpenEditorForAsset(AssetName.ToString());
	}
}

void UAssetEditorSubsystem::RegisterEditorModes()
{
	for (FThreadSafeObjectIterator EditorModeIter(UEdMode::StaticClass()); EditorModeIter; ++EditorModeIter)
	{
		UEdMode* EditorMode = Cast<UEdMode>(*EditorModeIter);
		UClass* ModeClass = EditorMode->GetClass();
		if (ModeClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Interface))
		{
			continue;
		}

		FEditorModeInfo EditorModeInfo = EditorMode->GetModeInfo();

		if (EditorModes.Contains(EditorModeInfo.ID))
		{
			TWeakObjectPtr<UClass> RegisteredClass = EditorModes[EditorModeInfo.ID].ModeClass;
			UE_LOG(
				LogAssetEditorSubsystem,
				Warning,
				TEXT("UAssetEditorSubsystem::RegisterEditorModes : Attempting to initialize duplicate mode with name '%s'. Conflicting classes: '%s' and '%s'."),
				*EditorModeInfo.ID.ToString(),
				*ModeClass->GetName(),
				*RegisteredClass.Get()->GetName()
			);
			continue;
		}

		EditorModes.Add(
			EditorModeInfo.ID,
			FRegisteredModeInfo{ ModeClass, EditorModeInfo }
		);

		OnEditorModeRegisteredEvent.Broadcast(EditorModeInfo.ID);
	}

	// Initialize Legacy FEditorModes
	FEditorModeRegistry::Get().Initialize();

	OnEditorModesChangedEvent.Broadcast();
}

void UAssetEditorSubsystem::UnregisterEditorModes()
{
	FEditorModeRegistry::Get().Shutdown();

	for (const auto& RegisteredMode : EditorModes)
	{
		OnEditorModeUnregisteredEvent.Broadcast(RegisteredMode.Value.ModeInfo.ID);
	}
	OnEditorModesChangedEvent.Broadcast();
	EditorModes.Empty();
}

void UAssetEditorSubsystem::OnSMInstanceElementsEnabled()
{
	// Let the modes know that SM instance elements may have been enabled or disabled and update state accordingly
	OnEditorModesChanged().Broadcast();
}

FNamePermissionList& UAssetEditorSubsystem::GetAllowedEditorModes()
{
	return AllowedEditorModes;
}

bool UAssetEditorSubsystem::IsEditorModeAllowed(const FName ModeId) const
{
	return AllowedEditorModes.PassesFilter(ModeId);
}

#undef LOCTEXT_NAMESPACE
