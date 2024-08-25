// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVUtility.h"

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "HAL/Thread.h"

#include <string.h>

THIRD_PARTY_INCLUDES_START

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma warning(push)
#pragma warning(disable : 4005)
#endif //PLATFORM_WINDOWS

#include "core/Factory.h"
#include "core/Interface.h"
#include "core/VulkanAMF.h"
#include "components/VideoEncoderVCE.h"

#if PLATFORM_WINDOWS
#pragma warning(pop)
#include "Windows/HideWindowsPlatformTypes.h"
#endif //PLATFORM_WINDOWS

THIRD_PARTY_INCLUDES_END

class AMFCODECS_API FAMF : public FAPI
{
public:
	bool bHasCompatibleGPU = true;

	FAMF();

	virtual bool IsValid() const override;

	amf::AMFFactory* GetFactory() const { return Factory; }

private:
	amf_handle DllHandle = nullptr;
	amf::AMFFactory* Factory = nullptr;
};

DECLARE_TYPEID(FAMF, AMFCODECS_API);
