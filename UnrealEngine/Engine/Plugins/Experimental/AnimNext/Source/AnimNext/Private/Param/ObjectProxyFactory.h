// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/IParameterSourceFactory.h"
#include "Features/IModularFeature.h"
#include "Param/ExternalParameterRegistry.h"
#include "UObject/ObjectKey.h"

class AActor;
class UActorComponent;
class UAnimNextConfig;

namespace UE::AnimNext
{
	struct FObjectAccessor;
	struct FClassProxy;
	class FModule;
}

namespace UE::AnimNext
{

using FObjectAccessorFunction = TUniqueFunction<UObject*(const FExternalParameterContext&)>;

// Factory for object proxies that supply 'external' parameters
class FObjectProxyFactory : public IParameterSourceFactory
{
	friend class ::UAnimNextConfig;
	friend class FModule;

	// Register built-in accessors and modular feature
	static void Init();

	// Unregister built-in accessors and modular feature
	static void Destroy();

	// Refresh built-in accessors and modular feature
	static void Refresh();

	// Clear internal data
	void Reset();
	
	// IParameterSourceFactory interface
	virtual void ForEachSource(TFunctionRef<void(FName)> InFunction) const override;
	virtual TUniquePtr<IParameterSource> CreateParameterSource(const FExternalParameterContext& InContext, FName InSourceName, TConstArrayView<FName> InRequiredParameters) const override;
#if WITH_EDITOR
	virtual bool FindParameterInfo(FName InParameterName, FParameterInfo& OutInfo) const override;
	virtual void ForEachParameter(FName InSourceName, TFunctionRef<void(FName, const FParameterInfo&)> InFunction) const override;
#endif

	// Register an accessor function to be called on initialization to retrieve an object of a specified class given the object held by the schedule's
	// entry (e.g. InContextObject is of type UAnimNextComponent, class is of type USkeletalMeshComponent).
	// Should only be called from the game thread
	// @param	InAccessorName	The name of the accessor.
	// @param	InTargetClass	The desired class of the object that will be returned by the accessor function
	// @param	InFunction		The accessor function that will be called.
	void RegisterObjectAccessor(FName InAccessorName, const UClass* InTargetClass, FObjectAccessorFunction&& InFunction);

	// Register an accessor function to be called on initialization to retrieve an object of a specified class given the object held by the schedule's
	// entry (e.g. InContextObject is of type UAnimNextComponent, class is of type USkeletalMeshComponent). If the object's class matches known types,
	// then a built-in accessor function will be used. If the class does not match a known type, then the function will log a warning.
	// Should only be called from the game thread
	// @param	InAccessorName	The name of the accessor.
	// @param	InTargetClass	The desired class of the object that will be returned by the accessor function
	void RegisterObjectAccessor(FName InAccessorName, const UClass* InTargetClass);
	
	// Helper wrapper for RegisterObjectAccessor. Assumes that the accessed object is the owning actor of the context object.
	void RegisterActorAccessor(FName InAccessorName, TSubclassOf<AActor> InTargetClass);

	// Helper wrapper for RegisterObjectAccessor. Assumes that the accessed object is an actor component on the same actor as the context object.
	void RegisterActorComponentAccessor(FName InAccessorName, TSubclassOf<UActorComponent> InTargetClass);

	// Unregister an accessor function that was previously registered to RegisterObjectAccessor
	// Should only be called from the game thread
	// @param	InAccessorName	The name of the accessor.
	void UnregisterObjectAccessor(FName InAccessorName);
	
	TSharedRef<FClassProxy> FindOrCreateClassProxy(const UClass* InClass);

	TSharedPtr<FObjectAccessor> FindObjectAccessor(FName InAccessorName) const;

	// Map of classes -> proxy
	TMap<TObjectKey<UClass>, TSharedPtr<FClassProxy>> ClassMap;

	// Map of accessor name -> accessor
	TMap<FName, TSharedPtr<FObjectAccessor>> ObjectAccessors;

	// Map of parameter name -> accessor
	TMap<FName, TWeakPtr<FObjectAccessor>> ParameterMap;

	// Detect concurrent access for ObjectAccessors
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(ObjectAccessorsAccessDetector);
};

}