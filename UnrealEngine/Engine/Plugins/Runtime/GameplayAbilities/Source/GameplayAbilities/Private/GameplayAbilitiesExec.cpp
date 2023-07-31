// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "AbilitySystemGlobals.h"
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

struct FGameplayAbilitiesExec : public FSelfRegisteringExec
{
	FGameplayAbilitiesExec()
	{
	}

	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface
};

FGameplayAbilitiesExec GameplayAbilitiesExecInstance;

bool FGameplayAbilitiesExec::Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (Inworld == NULL)
	{
		return false;
	}

	bool bHandled = false;

	UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

	if (FParse::Command(&Cmd, TEXT("ToggleIgnoreAbilitySystemCooldowns")))
	{
		AbilitySystemGlobals.ToggleIgnoreAbilitySystemCooldowns();
		bHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ToggleIgnoreAbilitySystemCosts")))
	{
		AbilitySystemGlobals.ToggleIgnoreAbilitySystemCosts();
		bHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ListPlayerAbilities")))
	{
		AbilitySystemGlobals.ListPlayerAbilities();
		bHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ServerActivatePlayerAbility")))
	{
		AbilitySystemGlobals.ServerActivatePlayerAbility(FParse::Token(Cmd, false));
		bHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ServerEndPlayerAbility")))
	{
		AbilitySystemGlobals.ServerEndPlayerAbility(FParse::Token(Cmd, false));
		bHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ServerCancelPlayerAbility")))
	{
		AbilitySystemGlobals.ServerCancelPlayerAbility(FParse::Token(Cmd, false));
		bHandled = true;
	}
	
	return bHandled;
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
