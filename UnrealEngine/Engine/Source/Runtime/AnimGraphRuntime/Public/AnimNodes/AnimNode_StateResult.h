// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNode_Root.h"
#include "AnimNode_StateResult.generated.h"

// Root node of an state machine state (sink node).
// We dont use AnimNode_Root to let us distinguish these nodes in the property list at link time.
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_StateResult : public FAnimNode_Root
{
	GENERATED_USTRUCT_BODY()

	/** Used to upgrade old FAnimNode_Roots to FAnimNode_StateResult */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

#if WITH_EDITORONLY_DATA
protected:

	/** The index of the state this node belongs to. Filled in during the owning state machine's compilation. */
	UPROPERTY(meta = (FoldProperty))
	int32 StateIndex = -1;
#endif

public:
#if WITH_EDITORONLY_DATA
	void SetStateIndex(int32 InStateIndex) { StateIndex = InStateIndex; }
#endif

	int32 GetStateIndex() const;
};

template<>
struct TStructOpsTypeTraits<FAnimNode_StateResult>
	: public TStructOpsTypeTraitsBase2<FAnimNode_StateResult>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
