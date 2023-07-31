// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityLibrary.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "ContentBrowserModule.h"
#include "Modules/ModuleManager.h"
#include "IContentBrowserSingleton.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorUtilitySubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Templates/SubclassOf.h"

#define LOCTEXT_NAMESPACE "BlutilityLevelEditorExtensions"

UEditorUtilityBlueprintAsyncActionBase::UEditorUtilityBlueprintAsyncActionBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEditorUtilityBlueprintAsyncActionBase::RegisterWithGameInstance(const UObject* WorldContextObject)
{
	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	EditorUtilitySubsystem->RegisterReferencedObject(this);
}

void UEditorUtilityBlueprintAsyncActionBase::SetReadyToDestroy()
{
	if (UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
	{
		EditorUtilitySubsystem->UnregisterReferencedObject(this);
	}
}

UAsyncEditorDelay::UAsyncEditorDelay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

UAsyncEditorDelay* UAsyncEditorDelay::AsyncEditorDelay(float Seconds, int32 MinimumFrames)
{
	UAsyncEditorDelay* NewTask = NewObject<UAsyncEditorDelay>();
	NewTask->Start(Seconds, MinimumFrames);

	return NewTask;
}

#endif

void UAsyncEditorDelay::Start(float InMinimumSeconds, int32 InMinimumFrames)
{
	EndFrame = GFrameCounter + InMinimumFrames;
	EndTime = FApp::GetCurrentTime() + InMinimumSeconds;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAsyncEditorDelay::HandleComplete), 0);
}

bool UAsyncEditorDelay::HandleComplete(float DeltaTime)
{
	if (FApp::GetCurrentTime() < EndTime)
	{
		return true;
	}

	if (GFrameCounter < EndFrame)
	{
		return true;
	}

	Complete.Broadcast();
	SetReadyToDestroy();
	return false;
}


UAsyncEditorWaitForGameWorld::UAsyncEditorWaitForGameWorld(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

UAsyncEditorWaitForGameWorld* UAsyncEditorWaitForGameWorld::AsyncWaitForGameWorld(int32 Index, bool Server)
{
	UAsyncEditorWaitForGameWorld* NewTask = NewObject<UAsyncEditorWaitForGameWorld>();
	NewTask->Start(Index, Server);

	return NewTask;
}

#endif

void UAsyncEditorWaitForGameWorld::Start(int32 InIndex, bool InServer)
{
	Index = InIndex;
	Server = InServer;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAsyncEditorWaitForGameWorld::OnTick), 0);
}

bool UAsyncEditorWaitForGameWorld::OnTick(float DeltaTime)
{
	if (GEditor)
	{
		int32 PIECount = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				if (UWorld* World = Context.World())
				{
					if (World->GetAuthGameMode())
					{
						// If they want the server we found it, but even if they didn't if the net mode
						// is standalone, server and client are the same, so we've found our mark
						if (Server || World->GetNetMode() == NM_Standalone)
						{
							Complete.Broadcast(World);
							SetReadyToDestroy();

							return false;
						}

						continue;
					}

					if (PIECount == Index)
					{
						Complete.Broadcast(World);
						SetReadyToDestroy();

						return false;
					}

					PIECount++;
				}
			}
		}

		return true;
	}

	Complete.Broadcast(nullptr);
	SetReadyToDestroy();

	return false;
}


UAsyncEditorOpenMapAndFocusActor::UAsyncEditorOpenMapAndFocusActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

UAsyncEditorOpenMapAndFocusActor* UAsyncEditorOpenMapAndFocusActor::AsyncEditorOpenMapAndFocusActor(FSoftObjectPath Map, FString FocusActorName)
{
	UAsyncEditorOpenMapAndFocusActor* NewTask = NewObject<UAsyncEditorOpenMapAndFocusActor>();
	NewTask->Start(Map, FocusActorName);

	return NewTask;
}

#endif

void UAsyncEditorOpenMapAndFocusActor::Start(FSoftObjectPath InMap, FString InFocusActorName)
{
	Map = InMap;
	FocusActorName = InFocusActorName;

	AddToRoot();

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
	UKismetSystemLibrary::ExecuteConsoleCommand(World, FString::Printf(TEXT("Automate.OpenMapAndFocusActor %s %s"), *InMap.ToString(), *InFocusActorName));

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAsyncEditorOpenMapAndFocusActor::OnTick), 0);
}

bool UAsyncEditorOpenMapAndFocusActor::OnTick(float DeltaTime)
{
	RemoveFromRoot();

	Complete.Broadcast();
	SetReadyToDestroy();

	return false;
}

UEditorUtilityLibrary::UEditorUtilityLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

TArray<AActor*> UEditorUtilityLibrary::GetSelectionSet()
{
	TArray<AActor*> Result;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			Result.Add(Actor);
		}
	}

	return Result;
}

void UEditorUtilityLibrary::GetSelectionBounds(FVector& Origin, FVector& BoxExtent, float& SphereRadius)
{
	bool bFirstItem = true;

	FBoxSphereBounds Extents;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			if (bFirstItem)
			{
				Extents = Actor->GetRootComponent()->Bounds;
			}
			else
			{
				Extents = Extents + Actor->GetRootComponent()->Bounds;
			}

			bFirstItem = false;
		}
	}

	Origin = Extents.Origin;
	BoxExtent = Extents.BoxExtent;
	SphereRadius = (float)Extents.SphereRadius; // TODO: LWC: should be double, but need to deprecate function and replace for old C++ references to continue working.
}

TArray<UObject*> UEditorUtilityLibrary::GetSelectedAssets()
{
	//@TODO: Blocking load, no slow dialog
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UObject*> Result;
	for (FAssetData& AssetData : SelectedAssets)
	{
		Result.Add(AssetData.GetAsset());
	}

	return Result;
}

TArray<UClass*> UEditorUtilityLibrary::GetSelectedBlueprintClasses()
{
	//@TODO: Blocking load, no slow dialog
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UClass*> Result;
	for (FAssetData& AssetData : SelectedAssets)
	{
		if (TSubclassOf<UBlueprint> AssetClass = AssetData.GetClass())
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				Result.Add(Blueprint->GeneratedClass);
			}
		}
	}

	return Result;
}

TArray<FAssetData> UEditorUtilityLibrary::GetSelectedAssetData()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	return SelectedAssets;
}

void UEditorUtilityLibrary::RenameAsset(UObject* Asset, const FString& NewName)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	TArray<FAssetRenameData> AssetsAndNames;
	const FString PackagePath = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
	new (AssetsAndNames) FAssetRenameData(Asset, PackagePath, NewName);

	AssetToolsModule.Get().RenameAssetsWithDialog(AssetsAndNames);
}

AActor* UEditorUtilityLibrary::GetActorReference(FString PathToActor)
{
#if WITH_EDITOR
	return Cast<AActor>(StaticFindObject(AActor::StaticClass(), GEditor->GetEditorWorldContext().World(), *PathToActor, false));
#else
	return nullptr;
#endif //WITH_EDITOR
}

bool UEditorUtilityLibrary::GetCurrentContentBrowserPath(FString& OutPath)
{
	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowser.GetCurrentPath();
	if (CurrentPath.HasInternalPath())
	{
		OutPath = CurrentPath.GetInternalPathString();
		return !OutPath.IsEmpty();
	}
	else
	{
		return false;
	}
}

TArray<FString> UEditorUtilityLibrary::GetSelectedFolderPaths()
{
	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	TArray<FString> Paths;
	ContentBrowser.GetSelectedFolders(Paths);
	return Paths;
}

void UEditorUtilityLibrary::SyncBrowserToFolders(const TArray<FString>& FolderList)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToFolders( FolderList, false, true );
}

#endif

#undef LOCTEXT_NAMESPACE
