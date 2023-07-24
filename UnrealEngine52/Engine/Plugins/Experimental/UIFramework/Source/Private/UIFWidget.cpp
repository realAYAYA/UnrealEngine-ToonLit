// Copyright Epic Games, Inc. All Rights Reserved.

#include "UIFWidget.h"
//#include "UIFManagerSubsystem.h"

#include "Blueprint/UserWidget.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Net/UnrealNetwork.h"
#include "Templates/NonNullPointer.h"
#include "Types/UIFWidgetTree.h"
#include "Types/UIFWidgetOwner.h"
#include "Types/UIFWidgetTreeOwner.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UIFWidget)


void UUIFrameworkWidget::ForceNetUpdate()
{
	if (AActor* OwnerActor = Cast<AActor>(GetOuter()))
	{
		OwnerActor->ForceNetUpdate();
	}
}

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
	if (AActor* OwnerActor = Cast<AActor>(GetOuter()))
	{
		return OwnerActor->GetFunctionCallspace(Function, Stack);
	}
	return Super::GetFunctionCallspace(Function, Stack);
}

bool UUIFrameworkWidget::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	bool bProcessed = false;
	AActor* OwnerActor = Cast<AActor>(GetOuter());
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
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsEnabled, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Visibility, Params);

	Params.Condition = COND_InitialOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UUIFrameworkWidget, WidgetClass, Params);
}

FUIFrameworkWidgetTree* UUIFrameworkWidget::GetWidgetTree() const
{
	return WidgetTreeOwner ? &WidgetTreeOwner->GetWidgetTree() : nullptr;
}

void UUIFrameworkWidget::LocalAddChild(FUIFrameworkWidgetId ChildId)
{
	// By default we should remove the widget from its previous parent.
	//Adding a widget to a new slot will automatically remove it from its previous parent.
	if (FUIFrameworkWidgetTree* WidgetTree = GetWidgetTree())
	{
		if (UUIFrameworkWidget* Widget = WidgetTree->FindWidgetById(ChildId))
		{
			if (UWidget* UMGWidget = Widget->LocalGetUMGWidget())
			{
				UMGWidget->RemoveFromParent();
			}
		}
	}
}

void UUIFrameworkWidget::LocalCreateUMGWidget(TNonNullPtr<IUIFrameworkWidgetTreeOwner> InOwner)
{
	WidgetTreeOwner = InOwner;
	if (UClass* Class = WidgetClass.Get())
	{
		if (Class->IsChildOf(UUserWidget::StaticClass()))
		{
			FUIFrameworkWidgetOwner UserWidgetOwner = WidgetTreeOwner->GetWidgetOwner();
			if (UserWidgetOwner.PlayerController)
			{
				LocalUMGWidget = CreateWidget(UserWidgetOwner.PlayerController, Class);
			}
			else if (UserWidgetOwner.GameInstance)
			{
				LocalUMGWidget = CreateWidget(UserWidgetOwner.GameInstance, Class);
			}
			else if (UserWidgetOwner.World)
			{
				LocalUMGWidget = CreateWidget(UserWidgetOwner.World, Class);
			}
			else
			{
				ensureAlwaysMsgf(false, TEXT("There are no valid UserWidget owner."));
			}
		}
		else
		{
			check(Class->IsChildOf(UWidget::StaticClass()));
			LocalUMGWidget = NewObject<UWidget>(this, Class, FName(), RF_Transient);
		}
		LocalUMGWidget->SetIsEnabled(bIsEnabled);
		LocalUMGWidget->SetVisibility(Visibility);
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
	WidgetTreeOwner = nullptr;
}


ESlateVisibility UUIFrameworkWidget::GetVisibility() const
{
	return Visibility;
}

void UUIFrameworkWidget::SetVisibility(ESlateVisibility InVisibility)
{
	if (Visibility != InVisibility)
	{
		Visibility = InVisibility;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Visibility, this);
		ForceNetUpdate();
	}
}

bool UUIFrameworkWidget::IsEnabled() const
{
	return bIsEnabled;
}

void UUIFrameworkWidget::SetEnabled(bool bInIsEnabled)
{
	if (bIsEnabled != bInIsEnabled)
	{
		bIsEnabled = bInIsEnabled;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsEnabled, this);
		ForceNetUpdate();
	}
}

void UUIFrameworkWidget::OnRep_IsEnabled()
{
	if (LocalUMGWidget)
	{
		LocalUMGWidget->SetIsEnabled(bIsEnabled);
	}
}

void UUIFrameworkWidget::OnRep_Visibility()
{
	if (LocalUMGWidget)
	{
		LocalUMGWidget->SetVisibility(Visibility);
	}
}
