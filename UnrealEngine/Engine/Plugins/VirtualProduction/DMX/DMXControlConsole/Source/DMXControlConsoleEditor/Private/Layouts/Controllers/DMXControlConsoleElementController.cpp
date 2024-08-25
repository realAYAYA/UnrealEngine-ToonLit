// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleElementController.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupController.h"
#include "Oscillators/DMXControlConsoleFloatOscillator.h"
#include "Styling/SlateTypes.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleElementController"

void UDMXControlConsoleElementController::Possess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
{
	if (!InElement)
	{
		return;
	}

	UDMXControlConsoleElementController* OldController = Cast<UDMXControlConsoleElementController>(InElement->GetElementController());
	if (OldController == this)
	{
		return;
	}
	 
	InElement->SetElementController(this);
	if (!Elements.Contains(InElement))
	{
		Elements.AddUnique(InElement);
	}
}

void UDMXControlConsoleElementController::Possess(TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements)
{
	InElements.RemoveAll([this](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			if (Element)
			{
				const UDMXControlConsoleElementController* OldController = Cast<UDMXControlConsoleElementController>(Element->GetElementController());
				return OldController == this;
			}
			return true;
		});

	if (!InElements.IsEmpty())
	{
		for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : InElements)
		{
			if (!Element)
			{
				continue;
			}

			Element->SetElementController(this);
		}

		Elements.Append(InElements);
	}
}

void UDMXControlConsoleElementController::UnPossess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
{
	if (!InElement || !Elements.Contains(InElement))
	{
		return;
	}

	InElement->SetElementController(nullptr);
	Elements.Remove(InElement);
}

void UDMXControlConsoleElementController::ClearElements()
{
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (Element)
		{
			Element->SetElementController(nullptr);
		}
	}
	Elements.Reset();
}

void UDMXControlConsoleElementController::SortElementsByStartingAddress()
{
	Algo::Sort(Elements, 
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& ItemA, const TScriptInterface<IDMXControlConsoleFaderGroupElement>& ItemB)
		{
			if (!ItemA || !ItemB)
			{
				return false;
			}

			const int32 StartingAddressA = ItemA->GetStartingAddress();
			const int32 StartingAddressB = ItemB->GetStartingAddress();

			return StartingAddressA < StartingAddressB;
		});
}

UDMXControlConsoleFaderGroupController& UDMXControlConsoleElementController::GetOwnerFaderGroupControllerChecked() const
{
	UDMXControlConsoleFaderGroupController* Outer = Cast<UDMXControlConsoleFaderGroupController>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get controller owner correctly."), *GetName());

	return *Outer;
}

int32 UDMXControlConsoleElementController::GetIndex() const
{
	const UDMXControlConsoleFaderGroupController& OwnerFaderGroupController = GetOwnerFaderGroupControllerChecked();

	const TArray<UDMXControlConsoleElementController*> ElementControllers = OwnerFaderGroupController.GetElementControllers();
	const int32 Index = ElementControllers.IndexOfByKey(this);
	return Index;
}

TArray<UDMXControlConsoleFaderBase*> UDMXControlConsoleElementController::GetFaders() const
{
	TArray<UDMXControlConsoleFaderBase*> Faders;
	Algo::TransformIf(Elements, Faders,
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			return IsValid(Cast<UDMXControlConsoleFaderBase>(Element.GetObject()));
		},
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			return Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
		});

	return Faders;
}

FString UDMXControlConsoleElementController::GenerateUserNameByElementsNames() const
{
	FString NewName = TEXT("");
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element)
		{
			continue;
		}

		const UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
		const FString ElementName = Fader ? Fader->GetFaderName() : FString();
		if (NewName.Contains(ElementName))
		{
			continue;
		}

		NewName.Append(ElementName);
		if (Elements.Last() != Element)
		{
			NewName.Append(TEXT("_"));
		}
	}

	return NewName;
}

void UDMXControlConsoleElementController::SetUserName(const FString& NewName)
{
	UserName = NewName;
}

void UDMXControlConsoleElementController::SetValue(float NewValue, bool bSyncElements)
{
	Value = FMath::Clamp(NewValue, MinValue, MaxValue);

	if (bSyncElements)
	{
		const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
		for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
		{
			if (!Fader.IsValid())
			{
				continue;
			}

			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
			const uint32 NewFaderValue = static_cast<uint32>(FMath::RoundToInt(ValueRange * Value));
			Fader->SetValue(NewFaderValue);
		}
	}
}

void UDMXControlConsoleElementController::SetMinValue(float NewMinValue, bool bSyncElements)
{
	MinValue = FMath::Clamp(NewMinValue, 0.f, MaxValue);
	Value = FMath::Clamp(Value, MinValue, MaxValue);

	if (bSyncElements)
	{
		const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
		for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
		{
			if (!Fader.IsValid())
			{
				continue;
			}

			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
			const uint32 NewFaderMinValue = static_cast<uint32>(FMath::RoundToInt((ValueRange * MinValue)));
			Fader->SetMinValue(NewFaderMinValue);
		}
	}
}

void UDMXControlConsoleElementController::SetMaxValue(float NewMaxValue, bool bSyncElements)
{
	MaxValue = FMath::Clamp(NewMaxValue, MinValue, 1.f);
	Value = FMath::Clamp(Value, MinValue, MaxValue);

	if (bSyncElements)
	{
		const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
		for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
		{
			if (!Fader.IsValid())
			{
				continue;
			}

			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
			const uint32 NewFaderMaxValue = static_cast<uint32>(FMath::RoundToInt(ValueRange * MaxValue));
			Fader->SetMaxValue(NewFaderMaxValue);
		}
	}
}

void UDMXControlConsoleElementController::ResetToDefault()
{
	if (Elements.IsEmpty())
	{
		return;
	}

	SetMinValue(0.f);
	SetMaxValue(1.f);

	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element)
		{
			continue;
		}

		UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
		if (!Fader)
		{
			continue;
		}

		Fader->ResetToDefault();

		// The controller value is the normalized value of the first valid fader
		if (Elements.IndexOfByKey(Element) == 0)
		{
			const uint8 NumBytes = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumBytes) - 1;
			const float NormalizedValue = Fader->GetValue() / ValueRange;

			SetValue(NormalizedValue);
		}
	}
}

void UDMXControlConsoleElementController::SetLocked(bool bLock)
{
	bIsLocked = bLock;

	const TArray<UDMXControlConsoleFaderBase*> Faders = GetFaders();
	for (TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader : Faders)
	{
		if (!Fader.IsValid())
		{
			continue;
		}

		Fader->Modify();
		Fader->SetLocked(bIsLocked);
	}
}

bool UDMXControlConsoleElementController::IsActive() const
{
	return GetOwnerFaderGroupControllerChecked().IsActive();
}

bool UDMXControlConsoleElementController::IsMatchingFilter() const
{
	const bool bIsAnyElementMatchingFilter = Algo::AnyOf(Elements, 
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			return Element && Element->IsMatchingFilter();
		});

	return bIsAnyElementMatchingFilter;
}

bool UDMXControlConsoleElementController::IsInActiveLayout() const
{
	UDMXControlConsoleFaderGroupController& OwnerFaderGroupController = GetOwnerFaderGroupControllerChecked();
	return OwnerFaderGroupController.IsInActiveLayout();
}

ECheckBoxState UDMXControlConsoleElementController::GetEnabledState() const
{
	const bool bAreAllElementsEnabled = Algo::AllOf(Elements,
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			const UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
			return Fader && Fader->IsEnabled();
		});

	if (bAreAllElementsEnabled)
	{
		return ECheckBoxState::Checked;
	}

	const bool bIsAnyElementEnabled = Algo::AnyOf(Elements,
		[](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			const UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
			return Fader && Fader->IsEnabled();
		});

	return bIsAnyElementEnabled ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked;
}

void UDMXControlConsoleElementController::Destroy()
{
	ClearElements();

	UDMXControlConsoleFaderGroupController& OwnerFaderGroupController = GetOwnerFaderGroupControllerChecked();

	OwnerFaderGroupController.PreEditChange(UDMXControlConsoleFaderGroupController::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroupController::GetElementControllersPropertyName()));
	OwnerFaderGroupController.DeleteElementController(this);
	OwnerFaderGroupController.PostEditChange();
}

void UDMXControlConsoleElementController::PostInitProperties()
{
	Super::PostInitProperties();

	UserName = GetName();
}

void UDMXControlConsoleElementController::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, FloatOscillatorClass))
	{
		if (UClass* StrongFloatOscillatorClass = FloatOscillatorClass.Get())
		{
			FloatOscillator = NewObject<UDMXControlConsoleFloatOscillator>(this, StrongFloatOscillatorClass, NAME_None, RF_Transactional | RF_Public);
		}
		else
		{
			FloatOscillator = nullptr;
		}
	}
}

void UDMXControlConsoleElementController::Tick(float DeltaTime)
{
	if (FloatOscillator)
	{
		const float ValueRange = MaxValue - MinValue;
		const float NewValue = FMath::Clamp(MinValue + FloatOscillator->GetNormalizedValue(DeltaTime) * ValueRange, MinValue, MaxValue);
		SetValue(NewValue);
	}
}

bool UDMXControlConsoleElementController::IsTickable() const
{
	return
		!bIsLocked &&
		FloatOscillator &&
		IsInActiveLayout();
}

TStatId UDMXControlConsoleElementController::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDMXControlConsoleElementController, STATGROUP_Tickables);
}

ETickableTickType UDMXControlConsoleElementController::GetTickableTickType() const
{
	return ETickableTickType::Conditional;
}

#undef LOCTEXT_NAMESPACE
