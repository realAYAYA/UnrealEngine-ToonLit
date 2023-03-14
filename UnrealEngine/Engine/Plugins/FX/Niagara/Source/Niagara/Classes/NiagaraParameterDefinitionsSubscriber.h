// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "NiagaraCommon.h"
#include "NiagaraParameterDefinitionsDelegates.h"

#include "NiagaraParameterDefinitionsSubscriber.generated.h"

class UNiagaraEditorParametersAdapterBase;
class UNiagaraParameterDefinitionsBase;
class UNiagaraScriptSourceBase;


USTRUCT()
struct FParameterDefinitionsSubscription
{
public:
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	FParameterDefinitionsSubscription() 
		: Definitions(nullptr)
		, DefinitionsId_DEPRECATED()
		, CachedChangeIdHash(0)
	{};

	UPROPERTY()
	TObjectPtr<UNiagaraParameterDefinitionsBase> Definitions;

	UPROPERTY(meta = (DeprecatedProperty))
	FGuid DefinitionsId_DEPRECATED;

	UPROPERTY()
	int32 CachedChangeIdHash;
#endif
};

/** Interface for classes which subscribe to parameter definitions. */
class NIAGARA_API INiagaraParameterDefinitionsSubscriber
{
#if WITH_EDITORONLY_DATA
public:
	virtual ~INiagaraParameterDefinitionsSubscriber();

	void PostLoadDefinitionsSubscriptions();
	void CleanupDefinitionsSubscriptions();

	//~ Begin Pure Virtual Methods
	virtual const TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() const = 0;
	virtual TArray<FParameterDefinitionsSubscription>& GetParameterDefinitionsSubscriptions() = 0;

	/** Get all UNiagaraScriptSourceBase of this subscriber. */
	virtual TArray<UNiagaraScriptSourceBase*> GetAllSourceScripts() = 0;

	/** Get the path to the UObject of this subscriber. */
	virtual FString GetSourceObjectPathName() const = 0;
	//~ End Pure Virtual Methods

	/** Get All adapters to editor only script vars owned directly by this subscriber. */
	virtual TArray<UNiagaraEditorParametersAdapterBase*> GetEditorOnlyParametersAdapters() { return TArray<UNiagaraEditorParametersAdapterBase*>(); };

	/** Get all subscribers that are owned by this subscriber.
	 *  Note: Implemented for synchronizing UNiagaraSystem. UNiagaraSystem returns all UNiagaraEmitters it owns to call SynchronizeWithParameterDefinitions for each.
	 */
	virtual TArray<INiagaraParameterDefinitionsSubscriber*> GetOwnedParameterDefinitionsSubscribers() { return TArray<INiagaraParameterDefinitionsSubscriber*>(); };

	TArray<UNiagaraParameterDefinitionsBase*> GetSubscribedParameterDefinitions() const;
	bool GetIsSubscribedToParameterDefinitions(const UNiagaraParameterDefinitionsBase* Definition) const;
	UNiagaraParameterDefinitionsBase* FindSubscribedParameterDefinitionsById(const FGuid& DefinitionsId) const;

	void SubscribeToParameterDefinitions(UNiagaraParameterDefinitionsBase* NewParameterDefinitions, bool bDoNotAssertIfAlreadySubscribed = false);
	void UnsubscribeFromParameterDefinitions(const FGuid& ParameterDefinitionsToRemoveId);
	void SynchronizeWithParameterDefinitions(const FSynchronizeWithParameterDefinitionsArgs Args = FSynchronizeWithParameterDefinitionsArgs());

	FOnSubscribedParameterDefinitionsChanged& GetOnSubscribedParameterDefinitionsChangedDelegate() { return OnSubscribedParameterDefinitionsChangedDelegate; };

private:
	FOnSubscribedParameterDefinitionsChanged OnSubscribedParameterDefinitionsChangedDelegate;

	FDelegateHandle OnDeferredSyncAllNameMatchParametersHandle;

private:
	/** Update the cached change Id of specified Synchronized Subscribed Parameter Definition subscriptions so that they are not marked pending sync. */
	void MarkParameterDefinitionSubscriptionsSynchronized(TArray<FGuid> SynchronizedParameterDefinitionsIds /*= TArray<FGuid>()*/);
#endif
};
