// Copyright Epic Games, Inc. All Rights Reserved.

#include "UIFWidget.h"
//#include "UIFManagerSubsystem.h"
#include "UIFPlayerComponent.h"

#include "Blueprint/UserWidget.h"

#include "Engine/ActorChannel.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Types/UIFWidgetTree.h"


/**
 *
 */
int32 UUIFrameworkWidget::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject) || !IsSupportedForNetworking())
	{
		// This handles absorbing authority/cosmetic
		return GEngine->GetGlobalFunctionCallspace(Function, this, Stack);
	}
	if (AActor* OwnerActor = GetPlayerComponent() ? GetPlayerComponent()->GetOwner() : nullptr)
	{
		return OwnerActor->GetFunctionCallspace(Function, Stack);
	}
	return Super::GetFunctionCallspace(Function, Stack);
}

bool UUIFrameworkWidget::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	bool bProcessed = false;
	AActor* OwnerActor = GetPlayerComponent() ? GetPlayerComponent()->GetOwner() : nullptr;
	FWorldContext* const Context = OwnerActor ? GEngine->GetWorldContextFromWorld(OwnerActor->GetWorld()) : nullptr;
	if (Context)
	{
		for (FNamedNetDriver& Driver : Context->ActiveNetDrivers)
		{
			if (Driver.NetDriver && Driver.NetDriver->ShouldReplicateFunction(OwnerActor, Function))
			{
				Driver.NetDriver->ProcessRemoteFunction(OwnerActor, Function, Parameters, OutParms, Stack, this);
				bProcessed = true;
			}
		}
	}

	return bProcessed;
}

void UUIFrameworkWidget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Id, Params);
}

void UUIFrameworkWidget::AuthoritySetParent(UUIFrameworkPlayerComponent* NewOwner, FUIFrameworkParentWidget NewParent)
{
	const bool bDifferentOwner = NewOwner != OwnerPlayerComponent;
	if (OwnerPlayerComponent)
	{
		ensure(OwnerPlayerComponent == NewOwner);
		NewOwner = OwnerPlayerComponent;
		NewParent = FUIFrameworkParentWidget();
	}

	if (AuthorityParent.IsParentValid())
	{
		if (AuthorityParent.IsWidget())
		{
			AuthorityParent.AsWidget()->AuthorityRemoveChild(this);
		}
		else
		{
			check(AuthorityParent.IsPlayerComponent());
			AuthorityParent.AsPlayerComponent()->AuthorityRemoveChild(this);
		}
	}

	AuthorityParent = NewParent;
	OwnerPlayerComponent = NewOwner;

	if (AuthorityParent.IsParentValid() && OwnerPlayerComponent)
	{
		if (AuthorityParent.IsWidget())
		{
			OwnerPlayerComponent->GetWidgetTree().AddWidget(AuthorityParent.AsWidget(), this);
		}
		else
		{
			check(AuthorityParent.IsPlayerComponent());
			OwnerPlayerComponent->GetWidgetTree().AddRoot(this);
		}
	}
	else if (OwnerPlayerComponent)
	{
		OwnerPlayerComponent->GetWidgetTree().RemoveWidget(this);
	}

	if (bDifferentOwner)
	{
		SetParentPlayerOwnerRecursive();
	}
}

void UUIFrameworkWidget::SetParentPlayerOwnerRecursive()
{
	UUIFrameworkWidget* Self = this;
	AuthorityForEachChildren([Self](UUIFrameworkWidget* Child)
		{
			if (Child != nullptr)
			{
				check(Child->AuthorityGetParent().IsWidget() && Child->AuthorityGetParent().AsWidget() == Self);
				Child->OwnerPlayerComponent = Self->OwnerPlayerComponent;
				Child->AuthorityParent = FUIFrameworkParentWidget(Self);
				Child->SetParentPlayerOwnerRecursive();
			}
		});
}

void UUIFrameworkWidget::LocalAddChild(FUIFrameworkWidgetId ChildId)
{
	// By default we should remove the widget from its previous parent.
	//Adding a widget to a new slot will automaticly remove it from its previous parent.
	if (OwnerPlayerComponent)
	{
		if (UUIFrameworkWidget* Widget = OwnerPlayerComponent->GetWidgetTree().FindWidgetById(ChildId))
		{
			if (UWidget* UMGWidget = Widget->LocalGetUMGWidget())
			{
				UMGWidget->RemoveFromParent();
			}
		}
	}
}

void UUIFrameworkWidget::LocalCreateUMGWidget(UUIFrameworkPlayerComponent* InOwner)
{
	OwnerPlayerComponent = InOwner;
	if (UClass* Class = WidgetClass.Get())
	{
		if (Class->IsChildOf(UUserWidget::StaticClass()))
		{
			LocalUMGWidget = CreateWidget(OwnerPlayerComponent->GetPlayerController(), Class);
		}
		else
		{
			check(Class->IsChildOf(UWidget::StaticClass()));
			LocalUMGWidget = NewObject<UWidget>(this, Class, FName(), RF_Transient);
		}
		LocalOnUMGWidgetCreated();
	}
}

void UUIFrameworkWidget::LocalDestroyUMGWidget()
{
	if (LocalUMGWidget)
	{
		LocalUMGWidget->RemoveFromParent();
		LocalUMGWidget->ReleaseSlateResources(true);
	}
	LocalUMGWidget = nullptr;
	OwnerPlayerComponent = nullptr;
}
