// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelDefinitions.h"
#include "NiagaraWorldManager.h"
#include "NiagaraDataChannel.h"
#include "DataInterface/NiagaraDataInterfaceDataChannelWrite.h"
#include "DataInterface/NiagaraDataInterfaceDataChannelRead.h"
#include "DataInterface/NiagaraDataInterfaceDataChannelSpawn.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "NiagaraDataChannelDefinitions"

TArray<UNiagaraDataChannelDefinitions*> UNiagaraDataChannelDefinitions::Definitions;


void UNiagaraDataChannelDefinitions::PostInitProperties()
{
	Super::PostInitProperties();
}

void UNiagaraDataChannelDefinitions::PostLoad()
{
	Super::PostLoad();

	if(Definitions.AddUnique(this) != INDEX_NONE)
	{
		FNiagaraWorldManager::ForAllWorldManagers(
			[&](FNiagaraWorldManager& WorldMan)
			{
				for(UNiagaraDataChannel* DataChannel : DataChannels)
				{
					WorldMan.GetDataChannelManager().InitDataChannel(DataChannel, false);
				}
			});
	}
}

void UNiagaraDataChannelDefinitions::BeginDestroy()
{
	Super::BeginDestroy();

	if(Definitions.Remove(this) != INDEX_NONE)
	{
		FNiagaraWorldManager::ForAllWorldManagers(
			[&](FNiagaraWorldManager& WorldMan)
			{
				for (UNiagaraDataChannel* DataChannel : DataChannels)
				{
					WorldMan.GetDataChannelManager().RemoveDataChannel(DataChannel->GetChannelName());
				}
			});
	}
}

#if WITH_EDITOR

void UNiagaraDataChannelDefinitions::PreEditChange( FProperty* PropertyAboutToChange )
{
	Super::PreEditChange(PropertyAboutToChange);

	//Belt and braces refreshing for data channels on edit. Can probably be a bit more targeted than this.	
	SysUpdateContext.SetDestroySystemSim(true);
	SysUpdateContext.SetDestroyOnAdd(true);

	FNiagaraWorldManager::ForAllWorldManagers(
		[&](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.WaitForAsyncWork();
			for (const UNiagaraDataChannel* DataChannel : DataChannels)
			{
				if (DataChannel)
				{
					WorldMan.GetDataChannelManager().RemoveDataChannel(DataChannel->GetChannelName());

					//Re-initialize any UNiagaraSystems that are using this channel.
					auto AddSystemsForDIUsingChannel = [&](auto* DI)
					{
						UNiagaraSystem* System = DI ? DI->template GetTypedOuter<UNiagaraSystem>() : nullptr;
						if (DI && System && DI->Channel.ChannelName == DataChannel->GetChannelName())
						{
							SysUpdateContext.Add(System, true);
						}
					};

					//TODO: Originally I iterated over systems and used ForEachDataInterface to check for DataChannel Usage. However ForEachDataInterface skips some important DIs so that needs to be updated before that approach will work.
					for (TObjectIterator<UNiagaraDataInterfaceDataChannelSpawn> It; It; ++It) { AddSystemsForDIUsingChannel(*It); }
					for (TObjectIterator<UNiagaraDataInterfaceDataChannelRead> It; It; ++It) { AddSystemsForDIUsingChannel(*It); }
					for (TObjectIterator<UNiagaraDataInterfaceDataChannelWrite> It; It; ++It) { AddSystemsForDIUsingChannel(*It); }
				}
			}
		});

	//Ensure we don't have any inflight commands that may refernce compiled data in the channel.
	FlushRenderingCommands();
}

void UNiagaraDataChannelDefinitions::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FNiagaraWorldManager::ForAllWorldManagers(
		[&](FNiagaraWorldManager& WorldMan)
		{			
			for (const UNiagaraDataChannel* DataChannel : DataChannels)
			{
				if (DataChannel)
				{
					WorldMan.GetDataChannelManager().InitDataChannel(DataChannel, true);

					//Re-initialize any UNiagaraSystems that are using this channel.
					auto AddSystemsForDIUsingChannel = [&](auto* DI)
					{
						UNiagaraSystem* System = DI ? DI->template GetTypedOuter<UNiagaraSystem>() : nullptr;
						if (DI && System && DI->Channel.ChannelName == DataChannel->GetChannelName())
						{
							SysUpdateContext.Add(System, true);
						}
					};

					//TODO: Originally I iterated over systems and used ForEachDataInterface to check for DataChannel Usage. However ForEachDataInterface skips some important DIs so that needs to be updated before that approach will work.
					for (TObjectIterator<UNiagaraDataInterfaceDataChannelSpawn> It; It; ++It) { AddSystemsForDIUsingChannel(*It); }
					for (TObjectIterator<UNiagaraDataInterfaceDataChannelRead> It; It; ++It) { AddSystemsForDIUsingChannel(*It); }
					for (TObjectIterator<UNiagaraDataInterfaceDataChannelWrite> It; It; ++It) { AddSystemsForDIUsingChannel(*It); }
				}
			}
		});

	SysUpdateContext.CommitUpdate();
}

#endif

#if WITH_EDITORONLY_DATA

FDelegateHandle UNiagaraDataChannelDefinitions::AssetRegistryOnLoadCompleteHandle;

void UNiagaraDataChannelDefinitions::OnAssetCreated(UNiagaraDataChannelDefinitions* CreatedDef)
{
	check(Definitions.Contains(CreatedDef) == false);
	CreatedDef->AddToRoot();
	Definitions.Add(CreatedDef);
	FNiagaraWorldManager::ForAllWorldManagers(
		[&](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.WaitForAsyncWork();
			for (UNiagaraDataChannel* DataChannel : CreatedDef->DataChannels)
			{
				WorldMan.GetDataChannelManager().InitDataChannel(DataChannel, true);
			}
		});
}

void UNiagaraDataChannelDefinitions::OnAssetDeleted(UNiagaraDataChannelDefinitions* DeletedDef)
{
	check(Definitions.Contains(DeletedDef));
	DeletedDef->RemoveFromRoot();
	FNiagaraWorldManager::ForAllWorldManagers(
		[&](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.WaitForAsyncWork();
			for (UNiagaraDataChannel* DataChannel : DeletedDef->DataChannels)
			{
				WorldMan.GetDataChannelManager().RemoveDataChannel(DataChannel->GetChannelName());
			}
		});
}

void UNiagaraDataChannelDefinitions::OnAssetRegistryLoadComplete()
{
	check(IsInGameThread());

	bAssetRegistryScanComplete = true;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.OnFilesLoaded().Remove(AssetRegistryOnLoadCompleteHandle);

	check(AssetRegistry.IsLoadingAssets() == false);

	TArray<FAssetData> AllDataChannelDefinitions;
	AssetRegistry.GetAssetsByClass(UNiagaraDataChannelDefinitions::StaticClass()->GetClassPathName(), AllDataChannelDefinitions);

	for (FAssetData& DataChannelDef : AllDataChannelDefinitions)
	{
		UNiagaraDataChannelDefinitions* NewDefs = Cast<UNiagaraDataChannelDefinitions>(DataChannelDef.GetAsset());
		NewDefs->AddToRoot();
		Definitions.AddUnique(NewDefs);

		FNiagaraWorldManager::ForAllWorldManagers(
			[&](FNiagaraWorldManager& WorldMan)
			{
				for (UNiagaraDataChannel* DataChannel : NewDefs->DataChannels)
				{
					WorldMan.GetDataChannelManager().InitDataChannel(DataChannel, false);
				}
			});
	}
}

#endif

const TArray<UNiagaraDataChannelDefinitions*>& UNiagaraDataChannelDefinitions::GetDataChannelDefinitions(bool bRequired, bool bInformUser)
{
	check(IsInGameThread());

	//In the editor we'll discovery all definitions from the asset registry for use in the UI.
	//At runtime, definitions are added as they are loaded by refs in UNiagaraSystems.
#if WITH_EDITORONLY_DATA
	if (bAssetRegistryScanBegun == false)
	{
		bAssetRegistryScanBegun = true;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		if (AssetRegistry.IsLoadingAssets())
		{
			AssetRegistryOnLoadCompleteHandle = AssetRegistry.OnFilesLoaded().AddStatic(&UNiagaraDataChannelDefinitions::OnAssetRegistryLoadComplete);
			if (bRequired)
			{
				AssetRegistry.WaitForCompletion();
			}
		}
		else
		{
			OnAssetRegistryLoadComplete();
		}		
	}

	if(bAssetRegistryScanComplete == false && bInformUser)
	{
		// Open a dialog asking the user to wait while assets are being discovered.
		// Todo. A better approach would be to customize all locations waiting for this scan to complete and inform the user inline. 
		// Then have the niagara UI refresh on scan completion.
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AssetsStillScanningError", "Asset Registry is still scanning files. Some lists such as those for Data Channels may be incommplete until the Asset Registry scan is complete."));
	}
#endif

	return Definitions;
}

const UNiagaraDataChannel* UNiagaraDataChannelDefinitions::FindDataChannel(FName ChannelName)
{
	for (const UNiagaraDataChannelDefinitions* DataChannelDef : GetDataChannelDefinitions(true, false))
	{
		for (UNiagaraDataChannel* DataChannel : DataChannelDef->DataChannels)
		{
			if (DataChannel->GetChannelName() == ChannelName)
			{
				return DataChannel;
			}
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE