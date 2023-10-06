// Copyright Epic Games, Inc. All Rights Reserved.

#include <OpenXRBlueprintFunctionLibrary.h>
#include <OpenXRHMD.h>

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenXRBlueprintFunctionLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogUOpenXRBlueprintFunctionLibrary, Log, All);

UOpenXRBlueprintFunctionLibrary::UOpenXRBlueprintFunctionLibrary(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UOpenXRBlueprintFunctionLibrary::SetEnvironmentBlendMode(int32 NewBlendMode) 
{
	FOpenXRHMD* OpenXRHMD = GetOpenXRHMD();
	if (OpenXRHMD) 
	{
		OpenXRHMD->SetEnvironmentBlendMode((XrEnvironmentBlendMode) NewBlendMode);
	}
}

FOpenXRHMD* UOpenXRBlueprintFunctionLibrary::GetOpenXRHMD()
{
	static FName SystemName(TEXT("OpenXR"));
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName))
	{
		return static_cast<FOpenXRHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}
