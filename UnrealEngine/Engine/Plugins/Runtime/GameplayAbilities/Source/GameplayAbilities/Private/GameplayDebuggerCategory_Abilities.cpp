// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerCategory_Abilities.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Engine/Canvas.h"


FGameplayDebuggerCategory_Abilities::FGameplayDebuggerCategory_Abilities()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Abilities::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Abilities());
}

void FGameplayDebuggerCategory_Abilities::FRepData::Serialize(FArchive& Ar)
{
	Ar << OwnedTags;

	int32 NumAbilities = Abilities.Num();
	Ar << NumAbilities;
	if (Ar.IsLoading())
	{
		Abilities.SetNum(NumAbilities);
	}

	for (int32 Idx = 0; Idx < NumAbilities; Idx++)
	{
		Ar << Abilities[Idx].Ability;
		Ar << Abilities[Idx].Source;
		Ar << Abilities[Idx].Level;
		Ar << Abilities[Idx].bIsActive;
	}

	int32 NumGE = GameplayEffects.Num();
	Ar << NumGE;
	if (Ar.IsLoading())
	{
		GameplayEffects.SetNum(NumGE);
	}

	for (int32 Idx = 0; Idx < NumGE; Idx++)
	{
		Ar << GameplayEffects[Idx].Effect;
		Ar << GameplayEffects[Idx].Context;
		Ar << GameplayEffects[Idx].Duration;
		Ar << GameplayEffects[Idx].Period;
		Ar << GameplayEffects[Idx].Stacks;
		Ar << GameplayEffects[Idx].Level;
	}
}

void FGameplayDebuggerCategory_Abilities::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	UAbilitySystemComponent* AbilityComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DebugActor);
	if (AbilityComp)
	{
		static FGameplayTagContainer OwnerTags;
		OwnerTags.Reset();
		AbilityComp->GetOwnedGameplayTags(OwnerTags);
		DataPack.OwnedTags = OwnerTags.ToStringSimple();

		TArray<FGameplayEffectSpec> ActiveEffectSpecs;
		AbilityComp->GetAllActiveGameplayEffectSpecs(ActiveEffectSpecs);
		for (int32 Idx = 0; Idx < ActiveEffectSpecs.Num(); Idx++)
		{
			const FGameplayEffectSpec& EffectSpec = ActiveEffectSpecs[Idx];
			FRepData::FGameplayEffectDebug ItemData;

			ItemData.Effect = EffectSpec.ToSimpleString();
			ItemData.Effect.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Effect.RemoveFromEnd(TEXT("_C"));

			ItemData.Context = EffectSpec.GetContext().ToString();
			ItemData.Duration = EffectSpec.GetDuration();
			ItemData.Period = EffectSpec.GetPeriod();
			ItemData.Stacks = EffectSpec.StackCount;
			ItemData.Level = EffectSpec.GetLevel();

			DataPack.GameplayEffects.Add(ItemData);
		}

		const TArray<FGameplayAbilitySpec>& AbilitySpecs = AbilityComp->GetActivatableAbilities();
		for (int32 Idx = 0; Idx < AbilitySpecs.Num(); Idx++)
		{
			const FGameplayAbilitySpec& AbilitySpec = AbilitySpecs[Idx];
			FRepData::FGameplayAbilityDebug ItemData;

			ItemData.Ability = GetNameSafe(AbilitySpec.Ability);
			ItemData.Ability.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Ability.RemoveFromEnd(TEXT("_C"));

			ItemData.Source = GetNameSafe(AbilitySpec.SourceObject.Get());
			ItemData.Source.RemoveFromStart(DEFAULT_OBJECT_PREFIX);

			ItemData.Level = AbilitySpec.Level;
			ItemData.bIsActive = AbilitySpec.IsActive();

			DataPack.Abilities.Add(ItemData);
		}
	}
}

bool FGameplayDebuggerCategory_Abilities::WrapStringAccordingToViewport(const FString& StrIn, FString& StrOut, FGameplayDebuggerCanvasContext& CanvasContext, float ViewportWitdh)
{
	if (!StrIn.IsEmpty())
	{
		// Clamp the Width
		ViewportWitdh = FMath::Max(ViewportWitdh, 10.0f);

		float StrWidth = 0.0f, StrHeight = 0.0f;
		// Calculate the length(in pixel) of the tags
		CanvasContext.MeasureString(StrIn, StrWidth, StrHeight);

		int32 SubDivision = FMath::CeilToInt(StrWidth / ViewportWitdh);
		if (SubDivision > 1)
		{
			// Copy the string
			StrOut = StrIn;
			const int32 Step = StrOut.Len() / SubDivision;
			// Start sub divide if needed
			for (int32 i = SubDivision - 1; i > 0; --i)
			{
				// Insert Line Feed
				StrOut.InsertAt(i * Step - 1, '\n');
			}
			return true;
		}
	}
	// No need to wrap the text 
	return false;
}

void FGameplayDebuggerCategory_Abilities::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FVector2D ViewPortSize;
	GEngine->GameViewport->GetViewportSize( /*out*/ViewPortSize);

	const float BackgroundPadding = 5.0f;
	const FVector2D BackgroundSize(ViewPortSize.X - 2 * BackgroundPadding, ViewPortSize.Y);
	const FLinearColor BackgroundColor(0.1f, 0.1f, 0.1f, 0.8f);

	// Draw a transparent background so that the text is easier to look at
	FCanvasTileItem Background(FVector2D(0.0f, 0.0f), BackgroundSize, BackgroundColor);
	Background.BlendMode = SE_BLEND_Translucent;
	CanvasContext.DrawItem(Background, CanvasContext.DefaultX - BackgroundPadding, CanvasContext.DefaultY - BackgroundPadding);

	FString WrappedOwnedTagsStr;
	// If need to wrap string, use the wrapped string, else use the DataPack one, avoid string copying.
	const FString& OwnedTagsRef = WrapStringAccordingToViewport(DataPack.OwnedTags, WrappedOwnedTagsStr, CanvasContext, BackgroundSize.X) ? WrappedOwnedTagsStr : DataPack.OwnedTags;

	CanvasContext.Printf(TEXT("Owned Tags: \n{yellow}%s"), *OwnedTagsRef);

	AActor* LocalDebugActor = FindLocalDebugActor();
	UAbilitySystemComponent* AbilityComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(LocalDebugActor);
	if (AbilityComp)
	{
		static FGameplayTagContainer OwnerTags;
		OwnerTags.Reset();
		AbilityComp->GetOwnedGameplayTags(OwnerTags);

		TArray<FString> OwnerTagsStrArray = OwnerTags.ToStringsMaxLen(1024);
		FString OwnerTagsStr = OwnerTagsStrArray.Num() > 0 ? OwnerTagsStrArray[0] : TEXT("");
		FString WrappedOwnerTagsStr;

		const FString& OwnerTagsStrRef = WrapStringAccordingToViewport(OwnerTagsStr, WrappedOwnerTagsStr, CanvasContext, BackgroundSize.X) ? WrappedOwnerTagsStr : OwnerTagsStr;
		CanvasContext.Printf(TEXT("Local Tags: \n{cyan}%s"), *OwnerTagsStrRef);
	}

	CanvasContext.Printf(TEXT("Gameplay Effects: {yellow}%d"), DataPack.GameplayEffects.Num());
	for (int32 Idx = 0; Idx < DataPack.GameplayEffects.Num(); Idx++)
	{
		const FRepData::FGameplayEffectDebug& ItemData = DataPack.GameplayEffects[Idx];

		FString Desc = FString::Printf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}duration:{white}"), *ItemData.Effect, *ItemData.Context);
		Desc += (ItemData.Duration > 0.0f) ? FString::Printf(TEXT("%.2f"), ItemData.Duration) : FString(TEXT("INF"));

		if (ItemData.Period > 0.0f)
		{
			Desc += FString::Printf(TEXT(" {grey}period:{white}%.2f"), ItemData.Period);
		}

		if (ItemData.Stacks > 1)
		{
			Desc += FString::Printf(TEXT(" {grey}stacks:{white}%d"), ItemData.Stacks);
		}

		if (ItemData.Level > 1.0f)
		{
			Desc += FString::Printf(TEXT(" {grey}level:{white}%.2f"), ItemData.Level);
		}

		CanvasContext.Print(Desc);
	}

	CanvasContext.Printf(TEXT("Gameplay Abilities: {yellow}%d"), DataPack.Abilities.Num());
	int32 HalfNum = FMath::CeilToInt(DataPack.Abilities.Num() / 2.f);
	for (int32 Idx = 0; Idx < HalfNum; Idx++)
	{
		if (2 * Idx + 1 < DataPack.Abilities.Num())
		{
			const FRepData::FGameplayAbilityDebug* ItemData[2] = { &DataPack.Abilities[2 * Idx], &DataPack.Abilities[2 * Idx + 1] };
			FString Abilities[2];
			float WidthSum = 0.0f;
			for (size_t j = 0; j < 2; ++j)
			{
				Abilities[j] = FString::Printf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}level:{white}%d {grey}active:{white}%s"),
					*ItemData[j]->Ability, *ItemData[j]->Source, ItemData[j]->Level, ItemData[j]->bIsActive ? TEXT("YES") : TEXT("NO"));
				float TempWidth = 0.0f; 
				float TempHeight = 0.0f;
				CanvasContext.MeasureString(Abilities[j], TempWidth, TempHeight);
				WidthSum += TempWidth;
			}

			if (WidthSum < BackgroundSize.X)
			{
				CanvasContext.Print(Abilities[0].Append(Abilities[1]));
			}
			else
			{
				CanvasContext.Print(Abilities[0]);
				CanvasContext.Print(Abilities[1]);
			}
		}
		else
		{
			const FRepData::FGameplayAbilityDebug& ItemData = DataPack.Abilities[2 * Idx];
			// Only display one
			CanvasContext.Printf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}level:{white}%d {grey}active:{white}%s"),
				*ItemData.Ability, *ItemData.Source, ItemData.Level, ItemData.bIsActive ? TEXT("YES") : TEXT("NO"));
		}
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
