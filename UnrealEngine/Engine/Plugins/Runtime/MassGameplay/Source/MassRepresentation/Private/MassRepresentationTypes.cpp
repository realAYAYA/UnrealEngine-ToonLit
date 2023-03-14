// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassRepresentationTypes.h"
#include "MassRepresentationUtils.h"
#include "MassCommandBuffer.h"

DEFINE_LOG_CATEGORY(LogMassRepresentation);

namespace UE::MassRepresentation
{
void PushSwapTagsCommand(FMassCommandBuffer& CommandBuffer, const FMassEntityHandle Entity, const EMassVisibility PrevVisibility, const EMassVisibility NewVisibility)
{
#define CASE_SWAP_TAGS(OldVisibility, NewVisibility) \
	case NewVisibility: \
		CommandBuffer.SwapTags<TMassVisibilityTagForLevel<OldVisibility>::FTag, TMassVisibilityTagForLevel<NewVisibility>::FTag>(Entity); \
		break

#define CASE_ADD_TAG(NewVisibility) \
case NewVisibility: \
	CommandBuffer.AddTag<TMassVisibilityTagForLevel<NewVisibility>::FTag>(Entity); \
	break

#define DEFAULT_REMOVE_TAG(OldVisibility) \
case EMassVisibility::Max: /* fall through on purpose */ \
default: \
	CommandBuffer.RemoveTag<TMassVisibilityTagForLevel<OldVisibility>::FTag>(Entity); \
	break

	check(PrevVisibility != NewVisibility);

	switch (PrevVisibility)
	{
	case EMassVisibility::CanBeSeen:
		switch (NewVisibility)
		{
		CASE_SWAP_TAGS(EMassVisibility::CanBeSeen, EMassVisibility::CulledByFrustum);
		CASE_SWAP_TAGS(EMassVisibility::CanBeSeen, EMassVisibility::CulledByDistance);
		DEFAULT_REMOVE_TAG(EMassVisibility::CanBeSeen);
		}
		break;
	case EMassVisibility::CulledByFrustum:
		switch (NewVisibility)
		{
		CASE_SWAP_TAGS(EMassVisibility::CulledByFrustum, EMassVisibility::CanBeSeen);
		CASE_SWAP_TAGS(EMassVisibility::CulledByFrustum, EMassVisibility::CulledByDistance);
		DEFAULT_REMOVE_TAG(EMassVisibility::CulledByFrustum);
		}
		break;
	case EMassVisibility::CulledByDistance:
		switch (NewVisibility)
		{
		CASE_SWAP_TAGS(EMassVisibility::CulledByDistance, EMassVisibility::CanBeSeen);
		CASE_SWAP_TAGS(EMassVisibility::CulledByDistance, EMassVisibility::CulledByFrustum);
		DEFAULT_REMOVE_TAG(EMassVisibility::CulledByDistance);
		}
		break;
	case EMassVisibility::Max:
		switch (NewVisibility)
		{
		CASE_ADD_TAG(EMassVisibility::CanBeSeen);
		CASE_ADD_TAG(EMassVisibility::CulledByFrustum);
		CASE_ADD_TAG(EMassVisibility::CulledByDistance);
		default:
			checkf(false, TEXT("Unsupported Visibility types!"));
			break;
		}
		break;
	default:
		checkf(false, TEXT("Unsupported Visibility type!"));
		break;
	}

#undef CASE_SWAP_TAGS
}
} // UE::MassRepresentation