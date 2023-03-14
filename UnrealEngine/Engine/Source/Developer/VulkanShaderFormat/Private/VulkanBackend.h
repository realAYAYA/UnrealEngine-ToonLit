// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "hlslcc.h"
#include "LanguageSpec.h"
#include "VulkanCommon.h"

// Generates Vulkan compliant code from IR tokens
#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif // __GNUC__

struct FVulkanBindingTable
{
	struct FBinding
	{
		FBinding();
		FBinding(const char* InName, int32 InVirtualIndex, EVulkanBindingType::EType InType, int8 InSubType);

		char		Name[256];
		int32		VirtualIndex = -1;
		EVulkanBindingType::EType	Type;
		int8		SubType;	// HLSL CC subtype, PACKED_TYPENAME_HIGHP and etc
	};
		
	FVulkanBindingTable(EHlslShaderFrequency ShaderStage) : Stage(ShaderStage) {}

	int32 RegisterBinding(const char* InName, const char* BlockName, EVulkanBindingType::EType Type);

	const TArray<FBinding>& GetBindings() const
	{
		check(bSorted);
		return Bindings;
	}

	void SortBindings();

	int32 GetRealBindingIndex(int32 InVirtualIndex) const
	{
		for (int32 Index = 0; Index < Bindings.Num(); ++Index)
		{
			if (Bindings[Index].VirtualIndex == InVirtualIndex)
			{
				return Index;
			}
		}

		return -1;
	}

	uint32 InputAttachmentsMask = 0;

private:
	// Previous implementation supported bindings only for textures.
	// However, layout(binding=%d) must be also used for uniform buffers.

	EHlslShaderFrequency Stage;
	TArray<FBinding> Bindings;

	bool bSorted = false;
};

// InputAttachments
// 0 - reserved for depth input, 1-8 for color
extern const char* VULKAN_SUBPASS_FETCH[9];
extern const char* VULKAN_SUBPASS_FETCH_VAR[9];
extern const TCHAR* VULKAN_SUBPASS_FETCH_VAR_W[9];

#ifdef __GNUC__
#pragma GCC visibility pop
#endif // __GNUC__
