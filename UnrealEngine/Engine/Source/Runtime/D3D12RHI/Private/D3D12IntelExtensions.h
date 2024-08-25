// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if INTEL_EXTENSIONS
	#define INTC_IGDEXT_D3D12 1

	#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	THIRD_PARTY_INCLUDES_START
		#include <igdext.h>
	THIRD_PARTY_INCLUDES_END
	#include "Microsoft/HideMicrosoftPlatformTypes.h"

	extern bool GDX12INTCAtomicUInt64Emulation;

	struct INTCExtensionContext;
	struct INTCExtensionInfo;

	INTCExtensionContext* CreateIntelExtensionsContext(ID3D12Device* Device, INTCExtensionInfo& INTCExtensionInfo);
	void DestroyIntelExtensionsContext(INTCExtensionContext* IntelExtensionContext);

	bool EnableIntelAtomic64Support(INTCExtensionContext* IntelExtensionContext, INTCExtensionInfo& INTCExtensionInfo);
	void EnableIntelAppDiscovery(uint32 DeviceId);

#endif //INTEL_EXTENSIONS
