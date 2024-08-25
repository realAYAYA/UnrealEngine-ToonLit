// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMBytecode.h"
#include "VerseVM/VVMBytecodeOps.h"

namespace
{
struct FOpInfo
{
	const char* Name;
};
constexpr FOpInfo Ops[] = {
#define VISIT_OP(Name) {#Name},
	VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
};
} // namespace

const char* Verse::ToString(Verse::EOpcode Opcode)
{
	return Ops[static_cast<size_t>(Opcode)].Name;
}
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)