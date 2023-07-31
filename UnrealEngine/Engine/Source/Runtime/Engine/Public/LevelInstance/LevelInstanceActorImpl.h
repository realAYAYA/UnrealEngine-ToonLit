// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Templates/UnrealTemplate.h"

/**
 * Provides base implementation for AActor classes implementing the ILevelInstanceInterface
 *
 * Should be used as a member of the AActor class implementing ILevelInstanceInterface
 *
 * @see ILevelInstanceInterface, ALevelInstance
 */
class ENGINE_API FLevelInstanceActorImpl
{
private:
	ILevelInstanceInterface* LevelInstance;
	FLevelInstanceID LevelInstanceID;

#if WITH_EDITOR
	FLevelInstanceID CachedLevelInstanceID;
	bool bCachedIsTemporarilyHiddenInEditor;
	bool bGuardLoadUnload;
public:
	TSoftObjectPtr<UWorld> CachedWorldAsset;
#endif
public:
	// Exists only to support 'FVTableHelper' Actor constructors
	FLevelInstanceActorImpl()
		: FLevelInstanceActorImpl(nullptr)
	{
	}

	FLevelInstanceActorImpl(ILevelInstanceInterface* InLevelInstance)
		: LevelInstance(InLevelInstance)
#if WITH_EDITOR
		, bCachedIsTemporarilyHiddenInEditor(false)
		, bGuardLoadUnload(false)
#endif
	{
	}

	virtual ~FLevelInstanceActorImpl()
	{
	}

	virtual void RegisterLevelInstance();
	virtual void UnregisterLevelInstance();

	/**
	 * Begin ILevelInstanceInterface Implementaion 
	 * 
	 */
	const FLevelInstanceID& GetLevelInstanceID() const;
	bool HasValidLevelInstanceID() const;
	virtual bool IsLoadingEnabled() const;
	virtual void OnLevelInstanceLoaded();

	/**
	 * Begin AActor Implementation
	 *
	 * Actor implementing ILevelInstanceInterface should be overriding most of those methods and forward the call their member FLevelInstanceActorImpl
	 * 
	 * @see ALevelInstance as an example
	 */
#if WITH_EDITOR
	virtual void PreEditUndo(TFunctionRef<void()> SuperCall);
	virtual void PostEditUndo(TFunctionRef<void()> SuperCall);
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation, TFunctionRef<void(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)> SuperCall);
	virtual void PostEditImport(TFunctionRef<void()> SuperCall);
	virtual bool CanEditChange(const FProperty* Property) const;
	virtual void PreEditChange(FProperty* Property, bool bWorldAssetChange, TFunctionRef<void(FProperty*)> SuperCall);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent, bool bWorldAssetChange, TFunction<void(FPropertyChangedEvent&)> SuperCall);
	virtual bool CanDeleteSelectedActor(FText& OutReason) const;
	virtual void SetIsTemporarilyHiddenInEditor(bool bIsHidden, TFunctionRef<void(bool)> SuperCall);
	virtual bool SetIsHiddenEdLayer(bool bIsHiddenEdLayer, TFunctionRef<bool(bool)> SuperCall);
	virtual void EditorGetUnderlyingActors(TSet<AActor*>& OutUnderlyingActors) const;
	virtual bool IsLockLocation() const;
	virtual bool GetBounds(FBox& OutBounds) const;
	virtual void PushSelectionToProxies();
	virtual void PushLevelInstanceEditingStateToProxies(bool bInEditingState);
	virtual void CheckForErrors();
	// End AActor Implementation
private:
	virtual void PostEditUndoInternal();
#endif
};