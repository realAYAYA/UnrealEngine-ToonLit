// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpirvCommon.h"
#include "hlslcc.h"


struct FVulkanSpirv : FSpirv
{
	struct FEntry
	{
		FEntry() = default;
		FEntry(const FString& InName, int32 InBinding)
			: Name(InName)
			, Binding(InBinding)
		{
		}
		FString Name;
		int32 Binding = -1;

		uint32 DescriptorSet = UINT32_MAX;

		// Index into the Spirv Word containing the descriptor set decoration
		uint32 WordDescriptorSetIndex = UINT32_MAX;

		// Index into the Spirv Word containing the binding index decoration
		uint32 WordBindingIndex = UINT32_MAX;
	};

	TArray<FEntry> ReflectionInfo;
	uint32 CRC = 0;
	const ANSICHAR* EntryPointName = nullptr;

	int32 FindBinding(const FString& Name, bool bOuter = false) const
	{
		for (const FEntry& Entry : ReflectionInfo)
		{
			if (Entry.Name == Name)
			{
				if (Entry.Binding == -1 && !bOuter)
				{
					// Try the outer group variable; eg 
					// layout(set=0,binding=0) buffer  CulledObjectBounds_BUFFER { vec4 CulledObjectBounds[]; };
					FString OuterName = Name;
					OuterName += TEXT("_BUFFER");
					return FindBinding(OuterName, true);
				}

				return Entry.Binding;
			}
		}

		if (!bOuter)
		{
			for (const FEntry& Entry : ReflectionInfo)
			{
				if (Entry.Name.StartsWith(Name) && Entry.Name.Len() > Name.Len() && Entry.Name[Name.Len()] == (TCHAR)'.')
				{
					// Try the outer group variable; eg 
					// layout(set=0, binding=0) buffer  MainAndPostPassPersistentStates_BUFFER { FPersistentState MainAndPostPassPersistentStates[]; };
					FString OuterName = Name;
					OuterName += TEXT("_BUFFER");
					return FindBinding(OuterName, true);
				}
			}
		}

		return -1;
	}

	const FEntry* GetEntryByBindingIndex(int32 BindingIndex) const
	{
		for (int32 Index = 0; Index < ReflectionInfo.Num(); ++Index)
		{
			if (ReflectionInfo[Index].Binding == BindingIndex)
			{
				return &ReflectionInfo[Index];
			}
		}

		return nullptr;
	}

	FEntry* GetEntry(const FString& Name)
	{
		for (int32 Index = 0; Index < ReflectionInfo.Num(); ++Index)
		{
			if (ReflectionInfo[Index].Name == Name)
			{
				return &ReflectionInfo[Index];
			}
		}

		return nullptr;
	}

	FEntry const* GetEntry(const FString& Name) const
	{
		for (int32 Index = 0; Index < ReflectionInfo.Num(); ++Index)
		{
			if (ReflectionInfo[Index].Name == Name)
			{
				return &ReflectionInfo[Index];
			}
		}

		return nullptr;
	}
};

// Updates all reflection entries in the specified SPIR-V module.
extern void PatchSpirvReflectionEntries(FVulkanSpirv& OutSpirv);
