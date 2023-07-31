// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputLibrary.h"

#include "Engine/Engine.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputModule.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputLibrary)

void UEnhancedInputLibrary::ForEachSubsystem(TFunctionRef<void(IEnhancedInputSubsystemInterface*)> SubsystemPredicate)
{
	// TODO: World subsystem for enhanced input, so that you bind to actions without an owning player controller
	// This is useful for widgets, main menu situations, or just stuff that isn't dependant on a player.

	// Players
	for (TObjectIterator<UEnhancedInputLocalPlayerSubsystem> It; It; ++It)
	{
		SubsystemPredicate(Cast<IEnhancedInputSubsystemInterface>(*It));
	}
}

void UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(const UInputMappingContext* Context, bool bForceImmediately)
{
	ForEachSubsystem([Context, bForceImmediately](IEnhancedInputSubsystemInterface* Subsystem) 
		{
			check(Subsystem);
			if (Subsystem && Subsystem->HasMappingContext(Context))
			{
				FModifyContextOptions Options {};
				Options.bForceImmediately = bForceImmediately;
				Subsystem->RequestRebuildControlMappings(Options);
			}
		});
}

FInputActionValue UEnhancedInputLibrary::GetBoundActionValue(AActor* Actor, const UInputAction* Action)
{
	if (Actor && Action)
	{
		UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Actor->InputComponent);
		return EIC ? EIC->GetBoundActionValue(Action) : FInputActionValue(Action->ValueType, FVector::ZeroVector);
	}
	else
	{
		if (!Actor)
		{
			UE_LOG(LogEnhancedInput, Error, TEXT("UEnhancedInputLibrary::GetBoundActionValue was called with an invalid Actor!"));
		}
		
		if (!Action)
		{
			UE_LOG(LogEnhancedInput, Error, TEXT("UEnhancedInputLibrary::GetBoundActionValue was called with an invalid Action!"));
		}		
		ensureMsgf(false, TEXT("Invalid GetBoundActionValue call. Check logs for details!"));
	}
	return FInputActionValue();
}


void UEnhancedInputLibrary::BreakInputActionValue(FInputActionValue InActionValue, double& X, double& Y, double& Z, EInputActionValueType& Type)
{
	FVector AsAxis3D = InActionValue.Get<FInputActionValue::Axis3D>();
	X = AsAxis3D.X;
	Y = AsAxis3D.Y;
	Z = AsAxis3D.Z;
	Type = InActionValue.GetValueType();
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FInputActionValue UEnhancedInputLibrary::MakeInputActionValue(double X, double Y, double Z, const FInputActionValue& MatchValueType)
{
	return FInputActionValue(MatchValueType.GetValueType(), FVector(X, Y, Z));
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FInputActionValue UEnhancedInputLibrary::MakeInputActionValueOfType(double X, double Y, double Z, const EInputActionValueType ValueType)
{
	return FInputActionValue(ValueType, FVector(X, Y, Z));
}

// FInputActionValue type conversions

bool UEnhancedInputLibrary::Conv_InputActionValueToBool(FInputActionValue InValue)
{
	return InValue.Get<bool>();
}

double UEnhancedInputLibrary::Conv_InputActionValueToAxis1D(FInputActionValue InValue)
{
	return static_cast<double>(InValue.Get<FInputActionValue::Axis1D>());
}

FVector2D UEnhancedInputLibrary::Conv_InputActionValueToAxis2D(FInputActionValue InValue)
{
	return InValue.Get<FInputActionValue::Axis2D>();
}

FVector UEnhancedInputLibrary::Conv_InputActionValueToAxis3D(FInputActionValue InValue)
{
	return InValue.Get<FInputActionValue::Axis3D>();
}

FString UEnhancedInputLibrary::Conv_InputActionValueToString(FInputActionValue ActionValue)
{
	return ActionValue.ToString();
}

