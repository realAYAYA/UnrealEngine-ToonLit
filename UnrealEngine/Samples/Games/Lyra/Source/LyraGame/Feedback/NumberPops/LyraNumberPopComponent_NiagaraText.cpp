// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraNumberPopComponent_NiagaraText.h"

#include "Containers/UnrealString.h"
#include "Feedback/NumberPops/LyraNumberPopComponent.h"
#include "GameFramework/Actor.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "LyraDamagePopStyleNiagara.h"
#include "LyraLogChannels.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Misc/AssertionMacros.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Trace/Detail/Channel.h"
#include "UObject/ObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraNumberPopComponent_NiagaraText)

ULyraNumberPopComponent_NiagaraText::ULyraNumberPopComponent_NiagaraText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{


}

void ULyraNumberPopComponent_NiagaraText::AddNumberPop(const FLyraNumberPopRequest& NewRequest)
{
	int32 LocalDamage = NewRequest.NumberToDisplay;

	//Change Damage to negative to differentiate Critial vs Normal hit
	if (NewRequest.bIsCriticalDamage)
	{
		LocalDamage *= -1;
	}

	//Add a NiagaraComponent if we don't already have one
	if (!NiagaraComp)
	{
		NiagaraComp = NewObject<UNiagaraComponent>(GetOwner());
		if (Style != nullptr)
		{
			NiagaraComp->SetAsset(Style->TextNiagara);
			NiagaraComp->bAutoActivate = false;
			
		}
		NiagaraComp->SetupAttachment(nullptr);
		check(NiagaraComp);
		NiagaraComp->RegisterComponent();
	}


	NiagaraComp->Activate(false);
	NiagaraComp->SetWorldLocation(NewRequest.WorldLocation);

	UE_LOG(LogLyra, Log, TEXT("DamageHit location : %s"), *(NewRequest.WorldLocation.ToString()));
	//Add Damage information to the current Niagara list - Damage informations are packed inside a FVector4 where XYZ = Position, W = Damage
	TArray<FVector4> DamageList = UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayVector4(NiagaraComp, Style->NiagaraArrayName);
	DamageList.Add(FVector4(NewRequest.WorldLocation.X, NewRequest.WorldLocation.Y, NewRequest.WorldLocation.Z, LocalDamage));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(NiagaraComp, Style->NiagaraArrayName, DamageList);
	
}

