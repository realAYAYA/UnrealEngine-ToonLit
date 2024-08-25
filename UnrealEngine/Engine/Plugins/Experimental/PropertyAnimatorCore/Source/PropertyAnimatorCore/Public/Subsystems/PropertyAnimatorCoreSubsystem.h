// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "Properties/PropertyAnimatorCoreData.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "PropertyAnimatorCoreSubsystem.generated.h"

class UFunction;
class UPropertyAnimatorCoreConverterBase;
class UPropertyAnimatorCoreBase;
class UPropertyAnimatorCoreHandlerBase;
class UPropertyAnimatorCorePresetBase;
class UPropertyAnimatorCoreResolver;
class UPropertyAnimatorCoreTimeSourceBase;

/** This subsystem handle all property animators */
UCLASS()
class UPropertyAnimatorCoreSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	friend class UPropertyAnimatorCoreComponent;

public:
	/** Get this subsystem instance */
	PROPERTYANIMATORCORE_API static UPropertyAnimatorCoreSubsystem* Get();

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/** Register the property controller class to allow its usage */
	PROPERTYANIMATORCORE_API bool RegisterAnimatorClass(const UClass* InPropertyControllerClass);

	/** Unregister the property controller class to disallow its usage */
	PROPERTYANIMATORCORE_API bool UnregisterAnimatorClass(const UClass* InPropertyControllerClass);

	/** Checks if the property controller class is already registered */
	PROPERTYANIMATORCORE_API bool IsAnimatorClassRegistered(const UClass* InPropertyControllerClass) const;

	/** Gets the animator CDO registered from the class */
	UPropertyAnimatorCoreBase* GetAnimatorRegistered(const UClass* InAnimatorClass) const;

	/** Returns true if any controller is able to control that property or nested otherwise false */
	PROPERTYANIMATORCORE_API bool IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData, bool bInCheckNestedProperties = true) const;

	/** Find all animators linked to the property */
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCoreBase*> GetPropertyLinkedAnimators(const FPropertyAnimatorCoreData& InPropertyData) const;

	/** Returns a set of existing property controller objects in owner that supports that property */
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCoreBase*> GetExistingAnimators(const FPropertyAnimatorCoreData& InPropertyData) const;
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCoreBase*> GetExistingAnimators(const AActor* InActor) const;

	/** Returns a set of property controller CDO that supports that property */
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCoreBase*> GetAvailableAnimators(const FPropertyAnimatorCoreData* InPropertyData) const;
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCoreBase*> GetAvailableAnimators() const;

	/** Register the property handler class to allow its usage */
	PROPERTYANIMATORCORE_API bool RegisterHandlerClass(const UClass* InHandlerClass);

	/** Unregister the property handler class to disallow its usage */
	PROPERTYANIMATORCORE_API bool UnregisterHandlerClass(const UClass* InHandlerClass);

	/** Checks if the property handler class is already registered */
	PROPERTYANIMATORCORE_API bool IsHandlerClassRegistered(const UClass* InHandlerClass) const;

	/** Gets a property handler for this property */
	UPropertyAnimatorCoreHandlerBase* GetHandler(const FPropertyAnimatorCoreData& InPropertyData) const;

	/** Register a resolver for custom properties */
	PROPERTYANIMATORCORE_API bool RegisterResolverClass(const UClass* InResolverClass);

	/** Unregister a resolver */
	PROPERTYANIMATORCORE_API bool UnregisterResolverClass(const UClass* InResolverClass);

	/** Is this resolver registered */
	PROPERTYANIMATORCORE_API bool IsResolverClassRegistered(const UClass* InResolverClass) const;

	/** Get all resolvable properties for an object property */
	PROPERTYANIMATORCORE_API void GetResolvableProperties(const FPropertyAnimatorCoreData& InPropertyData, TSet<FPropertyAnimatorCoreData>& OutProperties) const;

	/** Register a time source class to control clock for animators */
	PROPERTYANIMATORCORE_API bool RegisterTimeSourceClass(UClass* InTimeSourceClass);

	/** Unregister a time source class */
	PROPERTYANIMATORCORE_API bool UnregisterTimeSourceClass(UClass* InTimeSourceClass);

	/** Check time source class is registered */
	PROPERTYANIMATORCORE_API bool IsTimeSourceClassRegistered(UClass* InTimeSourceClass) const;

	/** Get all time sources available */
	TArray<FName> GetTimeSourceNames() const;

	/** Get a registered time source using its name */
	UPropertyAnimatorCoreTimeSourceBase* GetTimeSource(FName InTimeSourceName) const;

	/** Create a new time source for an animator */
	UPropertyAnimatorCoreTimeSourceBase* CreateNewTimeSource(FName InTimeSourceName, UPropertyAnimatorCoreBase* InAnimator);

	/** Register a preset class */
	PROPERTYANIMATORCORE_API bool RegisterPresetClass(const UClass* InPresetClass);

	/** Unregister a preset class */
	PROPERTYANIMATORCORE_API bool UnregisterPresetClass(const UClass* InPresetClass);

	/** Is this preset class registered */
	PROPERTYANIMATORCORE_API bool IsPresetClassRegistered(const UClass* InPresetClass) const;

	/** Gets all supported presets for a specific animator and actor */
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCorePresetBase*> GetSupportedPresets(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator) const;

	/** Get all registered preset available */
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCorePresetBase*> GetAvailablePresets() const;

	PROPERTYANIMATORCORE_API bool RegisterSetterResolver(FName InPropertyName, TFunction<UFunction*(const UObject*)>&& InFunction);

	PROPERTYANIMATORCORE_API bool UnregisterSetterResolver(FName InPropertyName);

	PROPERTYANIMATORCORE_API bool IsSetterResolverRegistered(FName InPropertyName) const;

	UFunction* ResolveSetter(FName InPropertyName, const UObject* InOwner);

	/** Register a converter class */
	PROPERTYANIMATORCORE_API bool RegisterConverterClass(const UClass* InConverterClass);

	/** Unregister a converter class */
	PROPERTYANIMATORCORE_API bool UnregisterConverterClass(const UClass* InConverterClass);

	/** Is this converter class registered */
	PROPERTYANIMATORCORE_API bool IsConverterClassRegistered(const UClass* InConverterClass);

	/** Checks if any converter supports the type conversion */
	PROPERTYANIMATORCORE_API bool IsConversionSupported(const FPropertyBagPropertyDesc& InFromProperty, const FPropertyBagPropertyDesc& InToProperty);

	/** Finds suitable converters for a type conversion */
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCoreConverterBase*> GetSupportedConverters(const FPropertyBagPropertyDesc& InFromProperty, const FPropertyBagPropertyDesc& InToProperty) const;

	/** Create an animator of specific class for an actor */
	PROPERTYANIMATORCORE_API UPropertyAnimatorCoreBase* CreateAnimator(AActor* InActor, const UClass* InAnimatorClass, UPropertyAnimatorCorePresetBase* InPreset = nullptr, bool bInTransact = false) const;

	/** Create animators of specific class for actors */
	PROPERTYANIMATORCORE_API TSet<UPropertyAnimatorCoreBase*> CreateAnimators(const TSet<AActor*>& InActors, const UClass* InAnimatorClass, UPropertyAnimatorCorePresetBase* InPreset = nullptr, bool bInTransact = false) const;

	/** Removes a animator bound to an owner */
	PROPERTYANIMATORCORE_API bool RemoveAnimator(UPropertyAnimatorCoreBase* InAnimator, bool bInTransact = false) const;

	/** Removes animators from their owner */
	PROPERTYANIMATORCORE_API bool RemoveAnimators(const TSet<UPropertyAnimatorCoreBase*> InAnimators, bool bInTransact = false) const;

	/** Apply a preset on an existing animator */
	PROPERTYANIMATORCORE_API bool ApplyAnimatorPreset(UPropertyAnimatorCoreBase* InAnimator, UPropertyAnimatorCorePresetBase* InPreset, bool bInTransact = false);

	/** Unapply a preset from an existing animator */
	PROPERTYANIMATORCORE_API bool UnapplyAnimatorPreset(UPropertyAnimatorCoreBase* InAnimator, UPropertyAnimatorCorePresetBase* InPreset, bool bInTransact = false);

	/** Link a property to an existing animator */
	PROPERTYANIMATORCORE_API bool LinkAnimatorProperty(UPropertyAnimatorCoreBase* InAnimator, FPropertyAnimatorCoreData& InProperty, bool bInTransact = false);
	PROPERTYANIMATORCORE_API bool LinkAnimatorProperties(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties, bool bInTransact = false);

	/** Unlink a property from an existing animator */
	PROPERTYANIMATORCORE_API bool UnlinkAnimatorProperty(UPropertyAnimatorCoreBase* InAnimator, FPropertyAnimatorCoreData& InProperty, bool bInTransact = false);
	PROPERTYANIMATORCORE_API bool UnlinkAnimatorProperties(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties, bool bInTransact = false);

	/** Set the enabled state of animators attached to actors, will disable state globally on the component */
	PROPERTYANIMATORCORE_API void SetActorAnimatorsEnabled(const TSet<AActor*>& InActors, bool bInEnabled, bool bInTransact = false);

	/** Set the enabled state of animators in a world, will disable state globally on the component */
	PROPERTYANIMATORCORE_API void SetLevelAnimatorsEnabled(const UWorld* InWorld, bool bInEnabled, bool bInTransact = false);

	/** Set the enabled state of animators provided */
	PROPERTYANIMATORCORE_API void SetAnimatorsEnabled(const TSet<UPropertyAnimatorCoreBase*>& InAnimators, bool bInEnabled, bool bInTransact = false);

protected:
	/** Delegate to change state of animators in a world */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAnimatorsSetEnabled, const UWorld* /** InAnimatorWorld */, bool /** bInAnimatorEnabled */, bool /** bInTransact */)
	static FOnAnimatorsSetEnabled OnAnimatorsSetEnabledDelegate;

	/**
	 * Scan for children of each of the following classes and registers their CDO:
	 * 1. UPropertyAnimatorCoreBase
	 * 2. UPropertyAnimatorCoreHandlerBase
	 * 3. UPropertyAnimatorCoreResolver
	 * 4. UPropertyAnimatorCoreTimeSourceBase
	 * 5. UPropertyAnimatorCorePresetBase
	 * 6. UPropertyAnimatorCoreConverterBase
	 */
	void RegisterAnimatorClasses();

	/** Time sources available to use with animators */
	UPROPERTY()
	TSet<TWeakObjectPtr<UPropertyAnimatorCoreTimeSourceBase>> TimeSourcesWeak;

	/** Animators available to link properties to */
	UPROPERTY()
	TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>> AnimatorsWeak;

	/** Handlers are used to set/get same type properties and reuse logic */
	UPROPERTY()
	TSet<TWeakObjectPtr<UPropertyAnimatorCoreHandlerBase>> HandlersWeak;

	/** Resolvers find properties to let user control them when they are unreachable/hidden */
	UPROPERTY()
	TSet<TWeakObjectPtr<UPropertyAnimatorCoreResolver>> ResolversWeak;

	/** Presets available to apply on animator */
	UPROPERTY()
	TSet<TWeakObjectPtr<UPropertyAnimatorCorePresetBase>> PresetsWeak;

	/** Converters available to transform a type to another type */
	UPROPERTY()
	TSet<TWeakObjectPtr<UPropertyAnimatorCoreConverterBase>> ConvertersWeak;

	/** Some property and their setter cannot be identified automatically, use manual setter resolvers */
	TMap<FName, TFunction<UFunction*(const UObject*)>> SetterResolvers;
};
