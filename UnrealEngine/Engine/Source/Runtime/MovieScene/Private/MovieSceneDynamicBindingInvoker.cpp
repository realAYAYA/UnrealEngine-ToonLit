// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneDynamicBindingInvoker.h"

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneDynamicBinding.h"
#include "UObject/UnrealType.h"

FMovieSceneDynamicBindingResolveResult FMovieSceneDynamicBindingInvoker::ResolveDynamicBinding(IMovieScenePlayer& Player, UMovieSceneSequence* Sequence, const FMovieSceneSequenceID& SequenceID, const FGuid& InGuid, const FMovieSceneDynamicBinding& DynamicBinding)
{
	if (!ensure(Sequence))
	{
		// Sequence is somehow null... fallback to default behavior.
		return FMovieSceneDynamicBindingResolveResult();
	}

	UFunction* DynamicBindingFunc = DynamicBinding.Function.Get();
	if (!DynamicBindingFunc)
	{
		// No dynamic binding, fallback to default behavior.
		return FMovieSceneDynamicBindingResolveResult();
	}

	UObject* DirectorInstance = Player.GetEvaluationTemplate().GetOrCreateDirectorInstance(SequenceID, Player);
	if (!DirectorInstance)
	{
#if !NO_LOGGING
		UE_LOG(LogMovieScene, Warning, 
				TEXT("%s: Failed to resolve dynamic binding '%s' because no director instance was available."), 
				*Sequence->GetName(), *DynamicBindingFunc->GetName());
#endif
		// Fallback to default behavior.
		return FMovieSceneDynamicBindingResolveResult();
	}

#if WITH_EDITOR
	const static FName NAME_CallInEditor(TEXT("CallInEditor"));

	UWorld* World = DirectorInstance->GetWorld();
	const bool bIsGameWorld = World && World->IsGameWorld();

	if (!bIsGameWorld && !DynamicBindingFunc->HasMetaData(NAME_CallInEditor))
	{
		UE_LOG(LogMovieScene, Verbose,
				TEXT("%s: Refusing to resolve dynamic binding '%s' in editor world because function '%s' has 'Call in Editor' set to false."),
				*Sequence->GetName(), *LexToString(InGuid), *DynamicBindingFunc->GetName());
		// Fallback to default behavior.
		return FMovieSceneDynamicBindingResolveResult();
	}
#endif // WITH_EDITOR

	UE_LOG(LogMovieScene, VeryVerbose,
			TEXT("%s: Resolving dynamic binding '%s' with function '%s'."),
			*Sequence->GetName(), *LexToString(InGuid), *DynamicBindingFunc->GetName());

	FMovieSceneDynamicBindingResolveParams ResolveParams;
	ResolveParams.ObjectBindingID = InGuid;
	ResolveParams.Sequence = Sequence;
	ResolveParams.RootSequence = Player.GetEvaluationTemplate().GetRootSequence();
	FMovieSceneDynamicBindingResolveResult Result = InvokeDynamicBinding(DirectorInstance, DynamicBinding, ResolveParams);

	return Result;
}

FMovieSceneDynamicBindingResolveResult FMovieSceneDynamicBindingInvoker::InvokeDynamicBinding(UObject* DirectorInstance, const FMovieSceneDynamicBinding& DynamicBinding, const FMovieSceneDynamicBindingResolveParams& ResolveParams)
{
	// Parse all function parameters.
	UFunction* DynamicBindingFunc = DynamicBinding.Function.Get();
	check(DynamicBindingFunc);
	uint8* Parameters = (uint8*)FMemory_Alloca(DynamicBindingFunc->ParmsSize + DynamicBindingFunc->MinAlignment);
	Parameters = Align(Parameters, DynamicBindingFunc->MinAlignment);

	// Initialize parameters.
	FMemory::Memzero(Parameters, DynamicBindingFunc->ParmsSize);

	FStructProperty* ReturnProp = nullptr;
	for (TFieldIterator<FProperty> It(DynamicBindingFunc); It; ++It)
	{
		FProperty* LocalProp = *It;
		checkSlow(LocalProp);
		if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			LocalProp->InitializeValue_InContainer(Parameters);
		}

		if (LocalProp->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			ensureMsgf(ReturnProp == nullptr,
					TEXT("Found more than one return parameter in dynamic binding resolver function!"));
			ReturnProp = CastFieldChecked<FStructProperty>(LocalProp);
		}
	}
	
	// Set the resolve parameter struct if we need to pass it to the function.
	if (FProperty* ResolveParamsProp = DynamicBinding.ResolveParamsProperty.Get())
	{
		ResolveParamsProp->SetValue_InContainer(Parameters, &ResolveParams);
	}

	// Invoke the function.
	DirectorInstance->ProcessEvent(DynamicBindingFunc, Parameters);

	// Grab the result value.
	FMovieSceneDynamicBindingResolveResult Result;
	if (ensureMsgf(ReturnProp != nullptr && ReturnProp->Struct == FMovieSceneDynamicBindingResolveResult::StaticStruct(),
			TEXT("The dynamic binding resolver function has no return value of type FMovieSceneDynamicBindingResolveResult")))
	{
		ReturnProp->GetValue_InContainer(Parameters, static_cast<void*>(&Result));
	}

	// Destroy parameters.
	for (TFieldIterator<FProperty> It(DynamicBindingFunc); It; ++It)
	{
		It->DestroyValue_InContainer(Parameters);
	}

	return Result;
}

