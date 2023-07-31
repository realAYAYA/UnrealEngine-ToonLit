// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassTranslator.generated.h"


UENUM()
enum class EMassTranslationDirection : uint8
{
	None = 0,
	InitializationOnly = 1,
	ActorToMass = 1 << 1,
	MassToActor = 1 << 2,
	BothWays = ActorToMass | MassToActor
};
ENUM_CLASS_FLAGS(EMassTranslationDirection);

/** 
 *  A class that's responsible for translation between UObjects and Mass. A translator knows how to initialize 
 *  fragments related to the UClass that the given translator cares about. It can also be used at runtime to 
 *  copy values from UObjects to fragments and back.
 */
UCLASS(abstract)
class MASSSPAWNER_API UMassTranslator : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UMassTranslator();
public:
	/** Fetches the FMassTag-derived types required by this Translator. And entity needs these tags to be 
	 *  processed by this Translator instance. 
	 * 
	 *  @todo might want this function on the MassProcessor level. TBD
	 * 
	 *  @param OutTagTypes tag types will be appended to this array. No uniqueness checks are performed. */
	void AppendRequiredTags(FMassTagBitSet& InOutTags) const { InOutTags += RequiredTags; }

protected:
	void AddRequiredTagsToQuery(FMassEntityQuery& EntityQuery);

protected:
	/** These are the tag fragments expected by this translator that let other code (like entity traits) hint to 
	 *  the system which translators they'd want their entity to be processed by. */
	FMassTagBitSet RequiredTags;
};
