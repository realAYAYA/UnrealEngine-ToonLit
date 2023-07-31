// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpirvCommon.h"

SHADERCOMPILERCOMMON_API void FindOffsetToSpirvEntryPoint(const FSpirv& Spirv, const ANSICHAR* EntryPointName, uint32& OutWordOffsetToEntryPoint, uint32& OutWordOffsetToMainName)
{
	// Iterate over all SPIR-V instructions until we have what we need
	for (FSpirvConstIterator Iter = Spirv.begin(); Iter != Spirv.end(); ++Iter)
	{
		switch (Iter.Opcode())
		{
		case SpvOpEntryPoint:
		{
			// Check if we found our entry point.
			// With RayTracing, there can be multiple entry point declarations in a single SPIR-V module.
			const ANSICHAR* Name = Iter.OperandAsString(3);
			if (FCStringAnsi::Strcmp(Name, EntryPointName) == 0)
			{
				// Return word offset to OpEntryPoint instruction
				check(OutWordOffsetToEntryPoint == 0);
				OutWordOffsetToEntryPoint = Spirv.GetWordOffset(Iter, 3);
			}
		}
		break;

		case SpvOpName:
		{
			const ANSICHAR* Name = Iter.OperandAsString(2);
			if (FCStringAnsi::Strcmp(Name, EntryPointName) == 0)
			{
				// Return word offset to OpName instruction that refers to the main entry point
				check(OutWordOffsetToMainName == 0);
				OutWordOffsetToMainName = Spirv.GetWordOffset(Iter, 2);
			}
		}
		break;

		case SpvOpDecorate:
		case SpvOpMemberDecorate:
		case SpvOpFunction:
		{
			// With the first annotation, type declaration, or function declaration,
			// there can't be any more entry point or debug instructions (i.e. OpEntryPoint and OpName).
			// However, only the OpFunction is guaranteed to appear.
			return;
		}
		}
	}
}

static const ANSICHAR* GSpirvPlaceholderEntryPointName = "main_00000000_00000000";

static void RenameFixedSizeSpirvString(FSpirv& Spirv, uint32 WordOffsetToString, uint32 CRC)
{
	char* TargetString = reinterpret_cast<char*>(Spirv.Data.GetData() + WordOffsetToString);
	check(!FCStringAnsi::Strcmp(TargetString, GSpirvPlaceholderEntryPointName));
	const uint32 SpirvByteSize = static_cast<uint32>(Spirv.GetByteSize());
	FCStringAnsi::Sprintf(TargetString, "main_%0.8x_%0.8x", SpirvByteSize, CRC);
};

SHADERCOMPILERCOMMON_API const ANSICHAR* PatchSpirvEntryPointWithCRC(FSpirv& Spirv, uint32& OutCRC)
{
	// Find offsets to entry point strings and generate CRC over the module
	uint32 OffsetToEntryPoint = 0, OffsetToMainName = 0;
	FindOffsetToSpirvEntryPoint(Spirv, GSpirvPlaceholderEntryPointName, OffsetToEntryPoint, OffsetToMainName);
	OutCRC = FCrc::MemCrc32(Spirv.GetByteData(), Spirv.GetByteSize());

	// Patch the (optional) entry point name decoration; this can be stripped out by some optimization passes
	RenameFixedSizeSpirvString(Spirv, OffsetToEntryPoint, OutCRC);
	if (OffsetToMainName != 0)
	{
		RenameFixedSizeSpirvString(Spirv, OffsetToMainName, OutCRC);
	}

	return reinterpret_cast<const ANSICHAR*>(Spirv.Data.GetData() + OffsetToEntryPoint);
}
