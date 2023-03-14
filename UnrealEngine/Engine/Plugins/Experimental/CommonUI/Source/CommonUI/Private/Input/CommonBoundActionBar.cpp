// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CommonBoundActionBar.h"
#include "CommonInputSubsystem.h"
#include "Editor/WidgetCompilerLog.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Input/CommonBoundActionButtonInterface.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionRouterTypes.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonBoundActionBar)

#define LOCTEXT_NAMESPACE "CommonUI"

bool bActionBarIgnoreOptOut = false;
static FAutoConsoleVariableRef CVarActionBarIgnoreOptOut(
	TEXT("ActionBar.IgnoreOptOut"),
	bActionBarIgnoreOptOut,
	TEXT("If true, the Bound Action Bar will display bindings whether or not they are configured bDisplayInReflector"),
	ECVF_Default
);

void UCommonBoundActionBar::SetDisplayOwningPlayerActionsOnly(bool bShouldOnlyDisplayOwningPlayerActions)
{
	if (bShouldOnlyDisplayOwningPlayerActions != bDisplayOwningPlayerActionsOnly)
	{
		bDisplayOwningPlayerActionsOnly = bShouldOnlyDisplayOwningPlayerActions;
		if (!IsDesignTime())
		{
			HandleBoundActionsUpdated(true);
		}
	}
}

void UCommonBoundActionBar::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (GameInstance->GetGameViewportClient())
		{
			GameInstance->GetGameViewportClient()->OnPlayerAdded().AddUObject(this, &UCommonBoundActionBar::HandlePlayerAdded);
		}

		for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			MonitorPlayerActions(LocalPlayer);
		}

		// Establish entries (as needed) immediately upon construction
		HandleDeferredDisplayUpdate();
	}
}

void UCommonBoundActionBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	//@todo DanH: Preview some entries - this should be really easy to get out of the box without copying DynamicEntryBox
	//	-> should make a GeneratePreviewEntries util on the base that takes a class and the number to show
	//GeneratePreviewEntries(NumPreviewEntries, ActionButtonClass);
}

void UCommonBoundActionBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (const UCommonUIActionRouterBase* ActionRouter = ULocalPlayer::GetSubsystem<UCommonUIActionRouterBase>(LocalPlayer))
			{
				ActionRouter->OnBoundActionsUpdated().RemoveAll(this);
			}
		}
	}
}

#if WITH_EDITOR
void UCommonBoundActionBar::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	if (!ActionButtonClass)
	{
		CompileLog.Error(FText::Format(LOCTEXT("Error_BoundActionBar_MissingButtonClass", "{0} has no ActionButtonClass specified."), FText::FromString(GetName())));
	}
	else if (CompileLog.GetContextClass() && ActionButtonClass->IsChildOf(CompileLog.GetContextClass()))
	{
		CompileLog.Error(FText::Format(LOCTEXT("Error_BoundActionBar_RecursiveButtonClass", "{0} has a recursive ActionButtonClass specified (reference itself)."), FText::FromString(GetName())));
	}

}
#endif

void UCommonBoundActionBar::HandleBoundActionsUpdated(bool bFromOwningPlayer)
{
	if (!bIsRefreshQueued && (bFromOwningPlayer || !bDisplayOwningPlayerActionsOnly))
	{
		bIsRefreshQueued = true;

		UGameInstance* GameInstance = GetGameInstance();
		check(GameInstance);
		GameInstance->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UCommonBoundActionBar::HandleDeferredDisplayUpdate));
	}
}

void UCommonBoundActionBar::HandleDeferredDisplayUpdate()
{
	bIsRefreshQueued = false;

	ResetInternal();

	const UGameInstance* GameInstance = GetGameInstance();
	check(GameInstance);
	const ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();

	// Sort the player list so our owner is at the end
	TArray<ULocalPlayer*> SortedPlayers = GameInstance->GetLocalPlayers();
	SortedPlayers.StableSort(
		[&OwningLocalPlayer](const ULocalPlayer& PlayerA, const ULocalPlayer& PlayerB)
		{
			return &PlayerA != OwningLocalPlayer;
		});

	for (const ULocalPlayer* LocalPlayer : SortedPlayers)
	{
		if (LocalPlayer == OwningLocalPlayer || !bDisplayOwningPlayerActionsOnly)
		{
			if (IsEntryClassValid(ActionButtonClass))
			{
				if (const UCommonUIActionRouterBase* ActionRouter = ULocalPlayer::GetSubsystem<UCommonUIActionRouterBase>(LocalPlayer))
				{
					const UCommonInputSubsystem& InputSubsystem = ActionRouter->GetInputSubsystem();
					const ECommonInputType PlayerInputType = InputSubsystem.GetCurrentInputType();
					const FName& PlayerGamepadName = InputSubsystem.GetCurrentGamepadName();

					TSet<FName> AcceptedBindings;
					TArray<FUIActionBindingHandle> FilteredBindings = ActionRouter->GatherActiveBindings().FilterByPredicate([PlayerInputType, PlayerGamepadName, &AcceptedBindings](const FUIActionBindingHandle& Handle) mutable
						{
							if (TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle))
							{
								if (!Binding->bDisplayInActionBar && !bActionBarIgnoreOptOut)
								{
									return false;
								}

								if (FCommonInputActionDataBase* LegacyData = Binding->GetLegacyInputActionData())
								{
									if (!LegacyData->CanDisplayInReflector(PlayerInputType, PlayerGamepadName))
									{
										return false;
									}
								}
								else
								{
									return false; //@todo(josh.gross) - allow non-legacy bindings
								}

								bool bAlreadyAccepted = false;
								AcceptedBindings.Add(Binding->ActionName, &bAlreadyAccepted);
								return !bAlreadyAccepted;
							}

							return false;
						});

					//Force Virtual_Back to one end of the list so Back actions are always consistent.
					//Otherwise, order within a node is controlled by order of add/remove.
					Algo::Sort(FilteredBindings, [PlayerInputType, PlayerGamepadName](const FUIActionBindingHandle& A, const FUIActionBindingHandle& B)
					{
						TSharedPtr<FUIActionBinding> BindingA = FUIActionBinding::FindBinding(A);
						TSharedPtr<FUIActionBinding> BindingB = FUIActionBinding::FindBinding(B);

						if (ensureMsgf((BindingA && BindingB), TEXT("The array filter above should enforce that there are no null bindings")))
						{
							FCommonInputActionDataBase* LegacyDataA = BindingA->GetLegacyInputActionData();
							FCommonInputActionDataBase* LegacyDataB = BindingB->GetLegacyInputActionData();
							
							if (ensureMsgf((LegacyDataA && LegacyDataB), TEXT("Non-legacy bindings not displayed yet -- array filter enforces they are not included")))
							{
								FKey KeyA = LegacyDataA->GetInputTypeInfo(PlayerInputType, PlayerGamepadName).GetKey();
								FKey KeyB = LegacyDataB->GetInputTypeInfo(PlayerInputType, PlayerGamepadName).GetKey();
							
								// Fallback back to keyboard key when there is no key for touch
								if (PlayerInputType == ECommonInputType::Touch)
								{
									if (!KeyA.IsValid())
									{
										KeyA = LegacyDataA->GetInputTypeInfo(ECommonInputType::MouseAndKeyboard, PlayerGamepadName).GetKey();
									}

									if (!KeyB.IsValid())
									{
										KeyB = LegacyDataB->GetInputTypeInfo(ECommonInputType::MouseAndKeyboard, PlayerGamepadName).GetKey();
									}
								}

								bool bAIsBack = (KeyA == EKeys::Virtual_Back || KeyA == EKeys::Escape || KeyA == EKeys::Android_Back);
								bool bBIsBack = (KeyB == EKeys::Virtual_Back || KeyB == EKeys::Escape || KeyB == EKeys::Android_Back);
							
								if (bAIsBack && !bBIsBack)
								{
									return false;
								}
								else if (LegacyDataA->NavBarPriority != LegacyDataB->NavBarPriority)
								{
									return LegacyDataA->NavBarPriority < LegacyDataB->NavBarPriority;
								}
							}

							return GetTypeHash(BindingA->Handle) < GetTypeHash(BindingB->Handle);
						}

						return true; // A < B by default
					});


					for (FUIActionBindingHandle BindingHandle : FilteredBindings)
					{
						ICommonBoundActionButtonInterface* ActionButton = Cast<ICommonBoundActionButtonInterface>(CreateEntryInternal(ActionButtonClass));
						if (ensure(ActionButton))
						{
							ActionButton->SetRepresentedAction(BindingHandle);
						}
					}
				}
			}
		}
	}
}

void UCommonBoundActionBar::HandlePlayerAdded(int32 PlayerIdx)
{
	const ULocalPlayer* NewPlayer = GetGameInstance()->GetLocalPlayerByIndex(PlayerIdx);
	MonitorPlayerActions(NewPlayer);
	HandleBoundActionsUpdated(NewPlayer == GetOwningLocalPlayer());
}

void UCommonBoundActionBar::MonitorPlayerActions(const ULocalPlayer* NewPlayer)
{
	if (const UCommonUIActionRouterBase* ActionRouter = ULocalPlayer::GetSubsystem<UCommonUIActionRouterBase>(NewPlayer))
	{
		ActionRouter->OnBoundActionsUpdated().AddUObject(this, &UCommonBoundActionBar::HandleBoundActionsUpdated, NewPlayer == GetOwningLocalPlayer());
	}
}

#undef LOCTEXT_NAMESPACE
