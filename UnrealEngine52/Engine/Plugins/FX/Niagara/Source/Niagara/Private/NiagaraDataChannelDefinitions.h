// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannelDefinitions.generated.h"

class UNiagaraDataChannel;
class UNiagaraDataChannelDefinitions;

/** Asset class defining a set of data channels that can be used for communications between Niagara Emitters and Systems. */
UCLASS()
class NIAGARA_API UNiagaraDataChannelDefinitions : public UObject
{
	GENERATED_BODY()

public:

	//UObject Interface
	virtual void PostInitProperties()override;
	virtual void PostLoad()override;
	virtual void BeginDestroy()override;
#if WITH_EDITOR
	virtual void PreEditChange(class FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface END

	UPROPERTY(EditAnywhere, Category = DataChannel, Instanced)
	TArray<TObjectPtr<UNiagaraDataChannel>> DataChannels;

	static const TArray<UNiagaraDataChannelDefinitions*>& GetDataChannelDefinitions(bool bRequired, bool bInformUser);
	static const UNiagaraDataChannel* FindDataChannel(FName ChannelName);
#if WITH_EDITORONLY_DATA
	static void OnAssetCreated(UNiagaraDataChannelDefinitions* CreatedDef);
	static void OnAssetDeleted(UNiagaraDataChannelDefinitions* DeletedDef);
	static void OnAssetRegistryLoadComplete();
#endif
private:

	static TArray<UNiagaraDataChannelDefinitions*> Definitions;

#if WITH_EDITORONLY_DATA
	FNiagaraSystemUpdateContext SysUpdateContext;

	static inline bool bAssetRegistryScanBegun = false;
	static inline bool bAssetRegistryScanComplete = false;
	static FDelegateHandle AssetRegistryOnLoadCompleteHandle;
#endif
};
