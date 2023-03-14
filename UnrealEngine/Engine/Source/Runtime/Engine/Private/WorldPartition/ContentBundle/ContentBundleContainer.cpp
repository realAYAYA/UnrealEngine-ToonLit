// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleContainer.h"

#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleClient.h"
#include "WorldPartition/ContentBundle/ContentBundleEngineSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"
#include "UObject/UObjectGlobals.h"

FContentBundleContainer::FContentBundleContainer(UWorld* WorldToInjectIn)
	: InjectedWorld(WorldToInjectIn)
{}

FContentBundleContainer::~FContentBundleContainer()
{
#if WITH_EDITOR
	if (UseEditorContentBundle())
	{
		check(GetEditorContentBundles().IsEmpty());
		return;
	}
#endif

	check(GetGameContentBundles().IsEmpty());
}

UWorld* FContentBundleContainer::GetInjectedWorld() const
{
	return InjectedWorld;
}

void FContentBundleContainer::Initialize()
{
	UE_LOG(LogContentBundle, Log, TEXT("[Container: %s] Creating new contrainer."), *GetInjectedWorld()->GetName());

#if WITH_EDITOR
	if (UseEditorContentBundle())
	{
		ContentBundlesVariant.Emplace<ContentBundleEditorArray>(ContentBundleEditorArray());
	}
	else
	{
		ContentBundlesVariant.Emplace<ContentBundleGameArray>(ContentBundleGameArray());
	}	
#endif

	InitializeContentBundlesForegisteredClients();
	RegisterContentBundleClientEvents();

#if WITH_EDITOR
	UWorldPartition* WorldPartition = GetInjectedWorld()->GetWorldPartition();
	WorldPartition->OnPreGenerateStreaming.AddRaw(this, &FContentBundleContainer::OnPreGenerateStreaming);
	WorldPartition->OnBeginCook.AddRaw(this, &FContentBundleContainer::OnBeginCook);
#endif
}

void FContentBundleContainer::Deinitialize()
{
	UE_LOG(LogContentBundle, Log, TEXT("[Container: %s] Deleting container."), *GetInjectedWorld()->GetName());

#if WITH_EDITOR
	UWorldPartition* WorldPartition = GetInjectedWorld()->GetWorldPartition();
	WorldPartition->OnPreGenerateStreaming.RemoveAll(this);
	WorldPartition->OnBeginCook.RemoveAll(this);
#endif

	UnregisterContentBundleClientEvents();
	DeinitializeContentBundles();
}

void FContentBundleContainer::AddReferencedObjects(FReferenceCollector& Collector)
{
	ForEachContentBundle([&Collector](FContentBundleBase* ContentBundle) { ContentBundle->AddReferencedObjects(Collector); });
}

uint32 FContentBundleContainer::GetNumContentBundles() const
{
#if WITH_EDITOR
	if(UseEditorContentBundle())
	{
		GetEditorContentBundles().Num();
	}
#endif

	return GetGameContentBundles().Num();
}

const TArray<TUniquePtr<FContentBundle>>& FContentBundleContainer::GetGameContentBundles() const
{
#if WITH_EDITOR
	check(!UseEditorContentBundle());
	return ContentBundlesVariant.Get<ContentBundleGameArray>();
#else
	return ContentBundles;
#endif
}

TArray<TUniquePtr<FContentBundle>>& FContentBundleContainer::GetGameContentBundles()
{
	return const_cast<TArray<TUniquePtr<FContentBundle>>&>(const_cast<const FContentBundleContainer*>(this)->GetGameContentBundles());
}

#if WITH_EDITOR

TSharedPtr<FContentBundleEditor> FContentBundleContainer::GetEditorContentBundle(const UContentBundleDescriptor* Descriptor) const
{
	for (const TSharedPtr<FContentBundleEditor>& ContentBundle : GetEditorContentBundles())
	{
		if (ContentBundle->GetDescriptor() == Descriptor)
		{
			return ContentBundle;
		}
	}

	return nullptr;
}

const TArray<TSharedPtr<FContentBundleEditor>>& FContentBundleContainer::GetEditorContentBundles() const
{
	check(UseEditorContentBundle());
	return ContentBundlesVariant.Get<ContentBundleEditorArray>();
}

TArray<TSharedPtr<FContentBundleEditor>>& FContentBundleContainer::GetEditorContentBundles()
{
	return const_cast<TArray<TSharedPtr<FContentBundleEditor>>&>(const_cast<const FContentBundleContainer*>(this)->GetEditorContentBundles());
}

bool FContentBundleContainer::UseEditorContentBundle() const
{
	bool bIsEditorEditWorld = !GetInjectedWorld()->IsGameWorld() && GetInjectedWorld()->IsEditorWorld();
	return bIsEditorEditWorld || IsRunningCookCommandlet();
}

#endif

void FContentBundleContainer::ForEachContentBundle(TFunctionRef<void(FContentBundleBase*)> Func) const
{
#if WITH_EDITOR
	if (UseEditorContentBundle())
	{
		for (const TSharedPtr<FContentBundleEditor>& ContentBundle : GetEditorContentBundles())
		{
			Func(ContentBundle.Get());
		}
		return;
	}
#endif

	for (const TUniquePtr<FContentBundle>& ContentBundle : GetGameContentBundles())
	{
		Func(ContentBundle.Get());
	}
}

void FContentBundleContainer::ForEachContentBundleBreakable(TFunctionRef<bool(FContentBundleBase*)> Func) const
{
#if WITH_EDITOR
	if (UseEditorContentBundle())
	{
		for (const TSharedPtr<FContentBundleEditor>& ContentBundle : GetEditorContentBundles())
		{
			if (!Func(ContentBundle.Get()))
			{
				return;
			}
		}
		return;
	}
#endif

	for (const TUniquePtr<FContentBundle>& ContentBundle : GetGameContentBundles())
	{
		if (!Func(ContentBundle.Get()))
		{
			return;
		}
	}
}

FContentBundleBase* FContentBundleContainer::GetContentBundle(const FContentBundleClient& ContentBundleClient)
{
	FContentBundleBase* ContentBundlePtr = nullptr;
	ForEachContentBundleBreakable([&ContentBundleClient, &ContentBundlePtr] (FContentBundleBase* ContentBundle) 
	{
		if (ContentBundle->GetDescriptor() == ContentBundleClient.GetDescriptor())
		{
			ContentBundlePtr = ContentBundle;
			return false;
		}
		return true;
	});
	
	if (ContentBundlePtr != nullptr)
	{
		return ContentBundlePtr;
	}

	return nullptr;
}

FContentBundleBase& FContentBundleContainer::InitializeContentBundle(TSharedPtr<FContentBundleClient>& ContentBundleClient)
{
	check(InjectedWorld->GetWorldPartition()->IsInitialized());
	check(GetContentBundle(*ContentBundleClient) == nullptr);

	FContentBundleBase* ContentBundle = nullptr;

	UE_LOG(LogContentBundle, Log, TEXT("[Container: %s] Creating new ContentBundle %s from client %s with state %s."), 
		*GetInjectedWorld()->GetName(), *ContentBundleClient->GetDescriptor()->GetDisplayName(), *ContentBundleClient->GetDisplayName(), *UEnum::GetDisplayValueAsText(ContentBundleClient->GetState()).ToString());

#if WITH_EDITOR
	if (UseEditorContentBundle())
	{
		ContentBundle = GetEditorContentBundles().Emplace_GetRef(MakeShared<FContentBundleEditor>(ContentBundleClient, GetInjectedWorld())).Get();
	}
	else
	{
		ContentBundle = GetGameContentBundles().Emplace_GetRef(MakeUnique<FContentBundle>(ContentBundleClient, GetInjectedWorld())).Get();
	}
#else
	 ContentBundle = GetGameContentBundles().Emplace_GetRef(MakeUnique<FContentBundle>(ContentBundleClient, GetInjectedWorld())).Get();
#endif

	 ContentBundle->Initialize();
	 return *ContentBundle;
}

void FContentBundleContainer::DeinitializeContentBundle(FContentBundleBase& ContentBundle)
{
	ContentBundle.Uninitialize();

#if WITH_EDITOR
	if (UseEditorContentBundle())
	{
		uint32 Index = GetEditorContentBundles().IndexOfByPredicate([&ContentBundle](const TSharedPtr<FContentBundleEditor>& Other) { return Other.Get() == &ContentBundle; });
		check(Index != INDEX_NONE);
		GetEditorContentBundles().RemoveAtSwap(Index);
		return;
	}
#endif

	uint32 Index = GetGameContentBundles().IndexOfByPredicate([&ContentBundle](const TUniquePtr<FContentBundle>& Other) { return Other.Get() == &ContentBundle; });
	GetGameContentBundles().RemoveAtSwap(Index);
}

void FContentBundleContainer::InjectContentBundle(FContentBundleBase& ContentBundle)
{
	ContentBundle.InjectContent();
}

void FContentBundleContainer::RemoveContentBundle(FContentBundleBase& ContentBundle)
{
	ContentBundle.RemoveContent();
}

void FContentBundleContainer::OnContentBundleClientRegistered(TSharedPtr<FContentBundleClient>& ContentBundleClient)
{
	InitializeContentBundle(ContentBundleClient);
}

void FContentBundleContainer::OnContentBundleClientUnregistered(FContentBundleClient& ContentBundleClient)
{
	if (FContentBundleBase* ContentBundle = GetContentBundle(ContentBundleClient))
	{
		DeinitializeContentBundle(*ContentBundle);
	}
}

void FContentBundleContainer::OnContentBundleClientContentInjectionRequested(FContentBundleClient& ContentBundleClient)
{
	if (FContentBundleBase* ContentBundle = GetContentBundle(ContentBundleClient))
	{
		InjectContentBundle(*ContentBundle);
	}
}

void FContentBundleContainer::OnContentBundleClientContentRemovalRequested(FContentBundleClient& ContentBundleClient)
{
	if (FContentBundleBase* ContentBundle = GetContentBundle(ContentBundleClient))
	{
		RemoveContentBundle(*ContentBundle);
	}
}

void FContentBundleContainer::RegisterContentBundleClientEvents()
{
	UContentBundleEngineSubsystem* ContentBundleEngineSubsystem = UContentBundleEngineSubsystem::Get();
	ContentBundleEngineSubsystem->OnContentBundleClientRegistered.AddRaw(this, &FContentBundleContainer::OnContentBundleClientRegistered);
	ContentBundleEngineSubsystem->OnContentBundleClientUnregistered.AddRaw(this, &FContentBundleContainer::OnContentBundleClientUnregistered);
	ContentBundleEngineSubsystem->OnContentBundleClientRequestedContentInjection.AddRaw(this, &FContentBundleContainer::OnContentBundleClientContentInjectionRequested);
	ContentBundleEngineSubsystem->OnContentBundleClientRequestedContentRemoval.AddRaw(this, &FContentBundleContainer::OnContentBundleClientContentRemovalRequested);
}

void FContentBundleContainer::UnregisterContentBundleClientEvents()
{
	UContentBundleEngineSubsystem* ContentBundleEngineSubsystem = UContentBundleEngineSubsystem::Get();
	ContentBundleEngineSubsystem->OnContentBundleClientRegistered.RemoveAll(this);
	ContentBundleEngineSubsystem->OnContentBundleClientUnregistered.RemoveAll(this);
	ContentBundleEngineSubsystem->OnContentBundleClientRequestedContentInjection.RemoveAll(this);
	ContentBundleEngineSubsystem->OnContentBundleClientRequestedContentRemoval.RemoveAll(this);
}

bool FContentBundleContainer::ShouldInjectClientContent(const TSharedPtr<FContentBundleClient>& ContentBundleClient) const
{
	return ContentBundleClient->GetState() == EContentBundleClientState::ContentInjectionRequested;
}

void FContentBundleContainer::InitializeContentBundlesForegisteredClients()
{
	UContentBundleEngineSubsystem* ContentBundleEngineSubsystem = UContentBundleEngineSubsystem::Get();
	TArrayView<TSharedPtr<FContentBundleClient>> ContentBundleClients = ContentBundleEngineSubsystem->GetContentBundleClients();

	if (!ContentBundleClients.IsEmpty())
	{
		UE_LOG(LogContentBundle, Log, TEXT("[Container: %s] Begin initializing ContentBundles from %u registered clients."), *GetInjectedWorld()->GetName(), ContentBundleClients.Num());

		for (TSharedPtr<FContentBundleClient>& ContentBundleClient : ContentBundleClients)
		{
			FContentBundleBase& ContentBundle = InitializeContentBundle(ContentBundleClient);

			if (ShouldInjectClientContent(ContentBundleClient))
			{
				InjectContentBundle(ContentBundle);
			}
		}

		UE_LOG(LogContentBundle, Log, TEXT("[Container: %s] End initializing ContentBundles."), *GetInjectedWorld()->GetName());
	}
}

void FContentBundleContainer::DeinitializeContentBundles()
{
	ForEachContentBundle([](FContentBundleBase* ContentBundle) { ContentBundle->Uninitialize(); });

#if WITH_EDITOR
	if (UseEditorContentBundle())
	{
		GetEditorContentBundles().Empty();
		return;
	}
#endif

	GetGameContentBundles().Empty();
}

#if WITH_EDITOR

void FContentBundleContainer::OnPreGenerateStreaming(TArray<FString>* OutPackageToGenerate)
{
	UE_LOG(LogContentBundle, Log, TEXT("[Container: %s] Generating Streaming for %u Content Bundles."), *GetInjectedWorld()->GetName(), GetEditorContentBundles().Num());

	if (IsRunningCookCommandlet())
	{
		// Cooking for plugin is not in yet.
		// Avoid calling OnPreGenerateStreaming from the cook package splitter.
		// It is not the correct flow & It outputs an error in the build log.
		return;
	}

	GetInjectedWorld()->ContentBundleManager->GetPIEDuplicateHelper()->Clear();

	for (TSharedPtr<FContentBundleEditor>& ContentBundle : GetEditorContentBundles())
	{
		ContentBundle->GenerateStreaming(OutPackageToGenerate);
	}
}

void FContentBundleContainer::OnBeginCook(IWorldPartitionCookPackageContext& CookContext)
{
	for (TSharedPtr<FContentBundleEditor>& ContentBundle : GetEditorContentBundles())
	{
		ContentBundle->OnBeginCook(CookContext);
	}
}

#endif