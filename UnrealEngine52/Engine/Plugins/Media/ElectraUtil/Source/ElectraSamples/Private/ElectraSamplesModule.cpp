// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraSamplesModule.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IElectraSamplesModule.h"

#include "IMediaTextureSample.h"
#include "MediaVideoDecoderOutput.h"

#define LOCTEXT_NAMESPACE "ElectraBaseModule"

DEFINE_LOG_CATEGORY(LogElectraSamples);

// -----------------------------------------------------------------------------------------------------------------------------------

class FElectraSamplesModule: public IElectraSamplesModule
{
public:
public:
	// IModuleInterface interface

	void StartupModule() override
	{
		static_assert((int32)EMediaOrientation::Original == (int32)EVideoOrientation::Original, "check alignment of both enums");
		static_assert((int32)EMediaOrientation::CW90 == (int32)EVideoOrientation::CW90, "check alignment of both enums");
	}

	void ShutdownModule() override
	{
	}

private:
};

IMPLEMENT_MODULE(FElectraSamplesModule, ElectraSamples);

#undef LOCTEXT_NAMESPACE


