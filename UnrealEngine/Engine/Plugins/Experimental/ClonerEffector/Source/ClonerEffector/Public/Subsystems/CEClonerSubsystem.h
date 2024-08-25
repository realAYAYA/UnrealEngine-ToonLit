// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CEClonerSubsystem.generated.h"

UCLASS(MinimalAPI)
class UCEClonerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static inline constexpr int32 NoFlicker = 1;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnCVarChanged)
	static FOnCVarChanged& OnCVarChanged()
	{
		return OnCVarChangedDelegate;
	}
#endif

	DECLARE_MULTICAST_DELEGATE(FOnSubsystemInitialized)
	static FOnSubsystemInitialized& OnSubsystemInitialized()
	{
		 return OnSubsystemInitializedDelegate;
	}

	/** Get this subsystem instance */
	CLONEREFFECTOR_API static UCEClonerSubsystem* Get();

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	CLONEREFFECTOR_API bool RegisterLayoutClass(const UClass* InClonerLayoutClass);
	CLONEREFFECTOR_API bool UnregisterLayoutClass(const UClass* InClonerLayoutClass);
	CLONEREFFECTOR_API bool IsLayoutClassRegistered(const UClass* InClonerLayoutClass);

	DECLARE_DELEGATE_RetVal_OneParam(TArray<AActor*> /** Children */, FOnGetOrderedActors, const AActor* /** InParent */)
	CLONEREFFECTOR_API void RegisterCustomActorResolver(FOnGetOrderedActors InCustomResolver);
	CLONEREFFECTOR_API void UnregisterCustomActorResolver();
	FOnGetOrderedActors& GetCustomActorResolver();

	/** Get available cloner layout names to use in dropdown */
	TArray<FName> GetLayoutNames() const;

	/** Based on a layout class, find layout name */
	FName FindLayoutName(TSubclassOf<UCEClonerLayoutBase> InLayoutClass) const;

	/** Creates a new layout instance for a cloner actor */
	UCEClonerLayoutBase* CreateNewLayout(FName InLayoutName, ACEClonerActor* InClonerActor);

#if WITH_EDITOR
	void EnableNoFlicker();
	void DisableNoFlicker();
	bool IsNoFlickerEnabled() const;

	void OnTSRShadingRejectionFlickeringPeriodChanged(IConsoleVariable* InCVar) const;
#endif

protected:
#if WITH_EDITOR
	static FOnCVarChanged OnCVarChangedDelegate;
#endif

	CLONEREFFECTOR_API static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	void ScanForRegistrableClasses();

	/** Linking name to the layout class */
	UPROPERTY()
	TMap<FName, TSubclassOf<UCEClonerLayoutBase>> LayoutClasses;

	/** Used to gather ordered actors based on parent */
	FOnGetOrderedActors ActorResolver;

#if WITH_EDITOR
	/** Allows to reduce ghosting artifacts when moving clone instances */
	IConsoleVariable* CVarTSRShadingRejectionFlickeringPeriod = nullptr;

	/** Previous value to restore it when disabled */
	TOptional<int32> PreviousCVarValue;
#endif
};