// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEnumerator.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VEnumerator);
DEFINE_TRIVIAL_VISIT_REFERENCES(VEnumerator);
TGlobalTrivialEmergentTypePtr<&VEnumerator::StaticCppClassInfo> VEnumerator::GlobalTrivialEmergentType;

uint32 VEnumerator::GetTypeHashImpl()
{
	return PointerHash(this);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
