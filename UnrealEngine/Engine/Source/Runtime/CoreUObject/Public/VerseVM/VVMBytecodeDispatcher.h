// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMBytecode.h"
#include "VVMBytecodeOps.h"
#include "VVMBytecodesAndCaptures.h"
#include "VVMLog.h"

namespace Verse
{
template <typename HandlerType>
void DispatchOps(FOp* OpsBegin, FOp* OpsEnd, HandlerType& Handler)
{
	const FOp* Op = OpsBegin;
	while (Op < OpsEnd)
	{
		switch (Op->Opcode)
		{
#define VISIT_OP(Name)                                                   \
	case EOpcode::Name:                                                  \
		Handler(*static_cast<const FOp##Name*>(Op));                     \
		Op = BitCast<const FOp*>(static_cast<const FOp##Name*>(Op) + 1); \
		break;
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
			default:
				V_DIE("Invalid opcode: %u", static_cast<FOpcodeInt>(Op->Opcode));
		};
	}
}
} // namespace Verse
#endif // WITH_VERSE_VM
