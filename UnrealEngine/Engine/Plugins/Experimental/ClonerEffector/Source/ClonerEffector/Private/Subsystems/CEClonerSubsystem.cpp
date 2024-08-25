// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/CEClonerSubsystem.h"

#include "Cloner/CEClonerActor.h"
#include "Cloner/Layouts/CEClonerCircleLayout.h"
#include "Cloner/Layouts/CEClonerCylinderLayout.h"
#include "Cloner/Layouts/CEClonerGridLayout.h"
#include "Cloner/Layouts/CEClonerHoneycombLayout.h"
#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "Cloner/Layouts/CEClonerLineLayout.h"
#include "Cloner/Layouts/CEClonerMeshLayout.h"
#include "Cloner/Layouts/CEClonerSphereRandomLayout.h"
#include "Cloner/Layouts/CEClonerSphereUniformLayout.h"
#include "Cloner/Layouts/CEClonerSplineLayout.h"
#include "Engine/Engine.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"

UCEClonerSubsystem::FOnCVarChanged UCEClonerSubsystem::OnCVarChangedDelegate;
#endif

UCEClonerSubsystem::FOnSubsystemInitialized UCEClonerSubsystem::OnSubsystemInitializedDelegate;

UCEClonerSubsystem* UCEClonerSubsystem::Get()
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<UCEClonerSubsystem>();
	}

	return nullptr;
}

void UCEClonerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Register them here to match old order of layout enum
	RegisterLayoutClass(UCEClonerGridLayout::StaticClass());
	RegisterLayoutClass(UCEClonerLineLayout::StaticClass());
	RegisterLayoutClass(UCEClonerCircleLayout::StaticClass());
	RegisterLayoutClass(UCEClonerCylinderLayout::StaticClass());
	RegisterLayoutClass(UCEClonerSphereUniformLayout::StaticClass());
	RegisterLayoutClass(UCEClonerHoneycombLayout::StaticClass());
	RegisterLayoutClass(UCEClonerMeshLayout::StaticClass());
	RegisterLayoutClass(UCEClonerSplineLayout::StaticClass());
	RegisterLayoutClass(UCEClonerSphereRandomLayout::StaticClass());

	// Scan for new layouts
	ScanForRegistrableClasses();

#if WITH_EDITOR
	CVarTSRShadingRejectionFlickeringPeriod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TSR.ShadingRejection.Flickering.Period"));

	if (CVarTSRShadingRejectionFlickeringPeriod)
	{
		CVarTSRShadingRejectionFlickeringPeriod->OnChangedDelegate().AddUObject(this, &UCEClonerSubsystem::OnTSRShadingRejectionFlickeringPeriodChanged);
	}
#endif

	OnSubsystemInitializedDelegate.Broadcast();
}

void UCEClonerSubsystem::Deinitialize()
{
	Super::Deinitialize();

#if WITH_EDITOR
	if (CVarTSRShadingRejectionFlickeringPeriod)
	{
		CVarTSRShadingRejectionFlickeringPeriod->OnChangedDelegate().RemoveAll(this);
	}
#endif
}

bool UCEClonerSubsystem::RegisterLayoutClass(const UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}

	if (!InClonerLayoutClass->IsChildOf(UCEClonerLayoutBase::StaticClass())
		|| InClonerLayoutClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsLayoutClassRegistered(InClonerLayoutClass))
	{
		return false;
	}

	const UCEClonerLayoutBase* CDO = InClonerLayoutClass->GetDefaultObject<UCEClonerLayoutBase>();

	if (!CDO)
	{
		return false;
	}

	// Check niagara asset is valid
	if (!CDO->IsLayoutValid())
	{
		return false;
	}

	// Does not overwrite existing layouts
	const FName LayoutName = CDO->GetLayoutName();
	if (LayoutClasses.Contains(LayoutName))
	{
		return false;
	}

	LayoutClasses.Add(LayoutName, CDO->GetClass());

	return true;
}

bool UCEClonerSubsystem::UnregisterLayoutClass(const UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}

	for (TMap<FName, TSubclassOf<UCEClonerLayoutBase>>::TIterator It(LayoutClasses); It; ++It)
	{
		if (It->Value.Get() == InClonerLayoutClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool UCEClonerSubsystem::IsLayoutClassRegistered(const UClass* InClonerLayoutClass)
{
	if (!IsValid(InClonerLayoutClass))
	{
		return false;
	}

	for (const TPair<FName, TSubclassOf<UCEClonerLayoutBase>>& LayoutClassPair : LayoutClasses)
	{
		if (LayoutClassPair.Value.Get() == InClonerLayoutClass)
		{
			return true;
		}
	}

	return false;
}

void UCEClonerSubsystem::RegisterCustomActorResolver(FOnGetOrderedActors InCustomResolver)
{
	ActorResolver = InCustomResolver;
}

void UCEClonerSubsystem::UnregisterCustomActorResolver()
{
	ActorResolver = FOnGetOrderedActors();
}

UCEClonerSubsystem::FOnGetOrderedActors& UCEClonerSubsystem::GetCustomActorResolver()
{
	return ActorResolver;
}

TArray<FName> UCEClonerSubsystem::GetLayoutNames() const
{
	TArray<FName> LayoutNames;
	LayoutClasses.GenerateKeyArray(LayoutNames);
	return LayoutNames;
}

FName UCEClonerSubsystem::FindLayoutName(TSubclassOf<UCEClonerLayoutBase> InLayoutClass) const
{
	if (const FName* Key = LayoutClasses.FindKey(InLayoutClass))
	{
		return *Key;
	}

	return NAME_None;
}

UCEClonerLayoutBase* UCEClonerSubsystem::CreateNewLayout(FName InLayoutName, ACEClonerActor* InClonerActor)
{
	if (!IsValid(InClonerActor))
	{
		return nullptr;
	}

	TSubclassOf<UCEClonerLayoutBase> const* LayoutClass = LayoutClasses.Find(InLayoutName);

	if (!LayoutClass)
	{
		return nullptr;
	}

	return NewObject<UCEClonerLayoutBase>(InClonerActor, LayoutClass->Get());
}

void UCEClonerSubsystem::ScanForRegistrableClasses()
{
	for (const UClass* const Class : TObjectRange<UClass>())
	{
		RegisterLayoutClass(Class);
	}
}

#if WITH_EDITOR
void UCEClonerSubsystem::EnableNoFlicker()
{
	if (IsNoFlickerEnabled())
	{
		return;
	}

	PreviousCVarValue = CVarTSRShadingRejectionFlickeringPeriod->GetInt();
	CVarTSRShadingRejectionFlickeringPeriod->Set(NoFlicker);
}

void UCEClonerSubsystem::DisableNoFlicker()
{
	if (!IsNoFlickerEnabled())
	{
		return;
	}

	if (PreviousCVarValue.IsSet())
	{
		CVarTSRShadingRejectionFlickeringPeriod->Set(PreviousCVarValue.GetValue());
	}
	else
	{
		CVarTSRShadingRejectionFlickeringPeriod->Set(*CVarTSRShadingRejectionFlickeringPeriod->GetDefaultValue());
	}
}

bool UCEClonerSubsystem::IsNoFlickerEnabled() const
{
	if (!CVarTSRShadingRejectionFlickeringPeriod)
	{
		return false;
	}

	return CVarTSRShadingRejectionFlickeringPeriod->GetInt() == NoFlicker;
}

void UCEClonerSubsystem::OnTSRShadingRejectionFlickeringPeriodChanged(IConsoleVariable* InCVar) const
{
	if (InCVar == CVarTSRShadingRejectionFlickeringPeriod)
	{
		OnCVarChangedDelegate.Broadcast();
	}
}
#endif