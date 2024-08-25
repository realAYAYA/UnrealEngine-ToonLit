// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningObservation.h"

#include "LearningAgentsNeuralNetwork.h" // Included for ELearningAgentsActivationFunction

#include "Engine/EngineTypes.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "LearningAgentsObservations.generated.h"

class ULearningAgentsManagerListener;
class ULearningAgentsObservationSchema;
class ULearningAgentsObservationObject;
struct FLearningAgentsObservationSchemaElement;
struct FLearningAgentsObservationObjectElement;

class USplineComponent;

/** An element of an Observation Schema */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsObservationSchemaElement
{
	GENERATED_BODY()

	UE::Learning::Observation::FSchemaElement SchemaElement;
};

/** An element of an Observation Object */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsObservationObjectElement
{
	GENERATED_BODY()

	UE::Learning::Observation::FObjectElement ObjectElement;
};

/** Comparison operator for Observation Object Elements */
bool operator==(const FLearningAgentsObservationObjectElement& Lhs, const FLearningAgentsObservationObjectElement& Rhs);

/** Hashing operator for Observation Object Elements */
uint32 GetTypeHash(const FLearningAgentsObservationObjectElement& Element);

template<>
struct TStructOpsTypeTraits<FLearningAgentsObservationObjectElement> : public TStructOpsTypeTraitsBase2<FLearningAgentsObservationObjectElement>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};

/**
 * Observation Schema
 *
 * This object is used to construct a schema describing some structure of observations.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsObservationSchema : public UObject
{
	GENERATED_BODY()

public:

	UE::Learning::Observation::FSchema ObservationSchema;
};

/**
 * Observation Object
 *
 * This object is used to construct or get the values of observations.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsObservationObject : public UObject
{
	GENERATED_BODY()

public:

	UE::Learning::Observation::FObject ObservationObject;
};

/** Enum Type representing either observation A or observation B */
UENUM(BlueprintType)
enum class ELearningAgentsEitherObservation : uint8
{
	A,
	B,
};

/** Enum Type representing either a Null observation or some Valid observation */
UENUM(BlueprintType)
enum class ELearningAgentsOptionalObservation : uint8
{
	Null,
	Valid,
};

UCLASS()
class LEARNINGAGENTS_API ULearningAgentsObservations : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Project a transform onto the ground plane, leaving just rotation around the vertical axis */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static FTransform ProjectTransformOntoGroundPlane(const FTransform Transform, const FVector LocalForwardVector = FVector::ForwardVector, const float GroundPlaneHeight = 0.0f);


	/** Find an Enum type by Name. This can be used to find Enum types defined in C++. This call can be expensive so the result should be cached. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	static UEnum* FindEnumByName(const FString& Name);

	/**
	 * Validates that the given observation object matches the schema. Will log errors on objects that don't match.
	 *
	 * @param Schema				Observation Schema
	 * @param SchemaElement			Observation Schema Element
	 * @param Object				Observation Object
	 * @param ObjectElement			Observation Object Element
	 * @returns						true if the object matches the schema
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	static bool ValidateObservationObjectMatchesSchema(
		const ULearningAgentsObservationSchema* Schema,
		const FLearningAgentsObservationSchemaElement SchemaElement,
		const ULearningAgentsObservationObject* Object,
		const FLearningAgentsObservationObjectElement ObjectElement);

	/**
	 * Logs an Observation Object Element. Useful for debugging.
	 *
	 * @param Object				Observation Object
	 * @param ObjectElement			Observation Object Element
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	static void LogObservation(const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element);

public:

	/**
	 * Specifies a new null observation. This represents an empty observation and can be useful when an observation is needed which has no value.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyNullObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("NullObservation"));

	/**
	 * Specifies a new continuous observation. This represents an observation made up of several float values.
	 *
	 * @param Schema The Observation Schema
	 * @param Size The number of float values in the observation.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyContinuousObservation(ULearningAgentsObservationSchema* Schema, const int32 Size, const FName Tag = TEXT("ContinuousObservation"));

	/**
	 * Specifies a new exclusive discrete observation. This represents a discrete observation which is an exclusive selection from multiple choices.
	 *
	 * @param Schema The Observation Schema
	 * @param Size The number of discrete options in the observation.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyExclusiveDiscreteObservation(ULearningAgentsObservationSchema* Schema, const int32 Size, const FName Tag = TEXT("ExclusiveDiscreteObservation"));

	/**
	 * Specifies a new inclusive discrete observation. This represents a discrete observation which is an inclusive selection from multiple choices.
	 *
	 * @param Schema The Observation Schema
	 * @param Size The number of discrete options in the observation.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyInclusiveDiscreteObservation(ULearningAgentsObservationSchema* Schema, const int32 Size, const FName Tag = TEXT("InclusiveDiscreteObservation"));

	/**
	 * Specifies a new count observation. This represents a count of something such as the size of, or index into, an array.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyCountObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("CountObservation"));

	/**
	 * Specifies a new struct observation. This represents a group of named sub-observations.
	 *
	 * @param Schema The Observation Schema
	 * @param Elements The sub-observations that make up this struct.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyStructObservation(ULearningAgentsObservationSchema* Schema, const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const FName Tag = TEXT("StructObservation"));

	/**
	 * Specifies a new struct observation. This represents a group of named sub-observations.
	 *
	 * @param Schema The Observation Schema
	 * @param ElementNames The names of the sub-observations that make up this struct.
	 * @param Elements The corresponding sub-observations that make up this struct. Must be the same size as ElementNames.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifyStructObservationFromArrays(ULearningAgentsObservationSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const FName Tag = TEXT("StructObservation"));
	
	/**
	 * Specifies a new struct observation. This represents a group of named sub-observations.
	 *
	 * @param Schema The Observation Schema
	 * @param ElementNames The names of the sub-observations that make up this struct.
	 * @param Elements The corresponding sub-observations that make up this struct. Must be the same size as ElementNames.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	static FLearningAgentsObservationSchemaElement SpecifyStructObservationFromArrayViews(ULearningAgentsObservationSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const FName Tag = TEXT("StructObservation"));

	/**
	 * Specifies a new exclusive union observation. This represents an observation which is exclusively chosen from a set of named sub-observations.
	 * In other words, when this observation is created, you only need to provide one observation from the given sub-observations.
	 *
	 * @param Schema The Observation Schema
	 * @param Elements The sub-observations that make up this union.
	 * @param EncodingSize The encoding size used to encode each sub-observation.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyExclusiveUnionObservation(ULearningAgentsObservationSchema* Schema, const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize = 128, const FName Tag = TEXT("ExclusiveUnionObservation"));

	/**
	 * Specifies a new exclusive union observation. This represents an observation which is exclusively chosen from a set of named sub-observations.
	 * In other words, when this observation is created, you only need to provide one observation from the given sub-observations.
	 *
	 * @param Schema The Observation Schema
	 * @param ElementNames The names of the sub-observations that make up this union.
	 * @param Elements The corresponding sub-observations that make up this union. Must be the same size as ElementNames.
	 * @param EncodingSize The encoding size used to encode each sub-observation.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifyExclusiveUnionObservationFromArrays(ULearningAgentsObservationSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize = 128, const FName Tag = TEXT("ExclusiveUnionObservation"));
	
	/**
	 * Specifies a new exclusive union observation. This represents an observation which is exclusively chosen from a set of named sub-observations.
	 * In other words, when this observation is created, you only need to provide one observation from the given sub-observations.
	 *
	 * @param Schema The Observation Schema
	 * @param ElementNames The names of the sub-observations that make up this union.
	 * @param Elements The corresponding sub-observations that make up this union. Must be the same size as ElementNames.
	 * @param EncodingSize The encoding size used to encode each sub-observation.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	static FLearningAgentsObservationSchemaElement SpecifyExclusiveUnionObservationFromArrayViews(ULearningAgentsObservationSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 EncodingSize = 128, const FName Tag = TEXT("ExclusiveUnionObservation"));

	/**
	 * Specifies a new inclusive union observation. This represents an observation which is inclusively chosen from a set of named sub-observations.
	 * In other words, when this observation is created, you can provide any combination of observations from the given sub-observations. Internally
	 * this observation uses Attention so can be slower to evaluate and more difficult to train than other observation types. For this reason it 
	 * should be used sparingly.
	 *
	 * @param Schema The Observation Schema
	 * @param Elements The sub-observations that make up this union.
	 * @param AttentionEncodingSize The encoding size used by the attention mechanism.
	 * @param AttentionHeadNum The number of heads used by the attention mechanism.
	 * @param ValueEncodingSize The output encoding size used by the attention mechanism.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyInclusiveUnionObservation(ULearningAgentsObservationSchema* Schema, const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Tag = TEXT("InclusiveUnionObservation"));

	/**
	 * Specifies a new inclusive union observation. This represents an observation which is inclusively chosen from a set of named sub-observations.
	 * In other words, when this observation is created, you can provide any combination of observations from the given sub-observations. Internally
	 * this observation uses Attention so can be slower to evaluate and more difficult to train than other observation types. For this reason it
	 * should be used sparingly.
	 *
	 * @param Schema The Observation Schema
	 * @param ElementNames The names of the sub-observations that make up this union.
	 * @param Elements The corresponding sub-observations that make up this union. Must be the same size as ElementNames.
	 * @param AttentionEncodingSize The encoding size used by the attention mechanism.
	 * @param AttentionHeadNum The number of heads used by the attention mechanism.
	 * @param ValueEncodingSize The output encoding size used by the attention mechanism.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifyInclusiveUnionObservationFromArrays(ULearningAgentsObservationSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Tag = TEXT("InclusiveUnionObservation"));
	
	/**
	 * Specifies a new inclusive union observation. This represents an observation which is inclusively chosen from a set of named sub-observations.
	 * In other words, when this observation is created, you can provide any combination of observations from the given sub-observations. Internally
	 * this observation uses Attention so can be slower to evaluate and more difficult to train than other observation types. For this reason it
	 * should be used sparingly.
	 *
	 * @param Schema The Observation Schema
	 * @param ElementNames The names of the sub-observations that make up this union.
	 * @param Elements The corresponding sub-observations that make up this union. Must be the same size as ElementNames.
	 * @param AttentionEncodingSize The encoding size used by the attention mechanism.
	 * @param AttentionHeadNum The number of heads used by the attention mechanism.
	 * @param ValueEncodingSize The output encoding size used by the attention mechanism.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	static FLearningAgentsObservationSchemaElement SpecifyInclusiveUnionObservationFromArrayViews(ULearningAgentsObservationSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Tag = TEXT("InclusiveUnionObservation"));

	/**
	 * Specifies a new static array observation. This represents an observation made up of a fixed-size array of some other observation.
	 *
	 * @param Schema The Observation Schema
	 * @param Element The sub-observation that represents elements of this array.
	 * @param Num The number of elements in the fixed size array.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifyStaticArrayObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 Num, const FName Tag = TEXT("StaticArrayObservation"));

	/**
	 * Specifies a new set observation. This represents an observation made up of a Set of some other observation. This Set can be variable in size 
	 * (up to some fixed maximum size) and elements are considered unordered. Internally this observation uses Attention so can be slower to evaluate 
	 * and more difficult to train than other observation types. For this reason it should be used sparingly.
	 *
	 * @param Schema The Observation Schema
	 * @param Element The sub-observation that represents elements of this array.
	 * @param MaxNum The maximum number of elements that can be included in the set.
	 * @param AttentionEncodingSize The encoding size used by the attention mechanism.
	 * @param AttentionHeadNum The number of heads used by the attention mechanism.
	 * @param ValueEncodingSize The output encoding size used by the attention mechanism.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifySetObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Tag = TEXT("SetObservation"));

	/**
	 * Specifies a new pair observation. This represents an observation made up of two sub-observations.
	 *
	 * @param Schema The Observation Schema
	 * @param Key The first sub-observation.
	 * @param Value The second sub-observation.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifyPairObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Key, const FLearningAgentsObservationSchemaElement Value, const FName Tag = TEXT("PairObservation"));

	/**
	 * Specifies a new array observation. This represents an observation made up of an Array of some other observation. This Array can be variable in 
	 * size (up to some fixed maximum size) and the order of elements is taken into consideration. Internally this observation uses Attention so can 
	 * be slower to evaluate and more difficult to train than other observation types. For this reason it should be used sparingly.
	 *
	 * @param Schema The Observation Schema
	 * @param Element The sub-observation that represents elements of this array.
	 * @param MaxNum The maximum number of elements that can be included in the array.
	 * @param AttentionEncodingSize The encoding size used by the attention mechanism.
	 * @param AttentionHeadNum The number of heads used by the attention mechanism.
	 * @param ValueEncodingSize The output encoding size used by the attention mechanism.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifyArrayObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Tag = TEXT("ArrayObservation"));

	/**
	 * Specifies a new map observation. This represents an observation made up of a Map of some other key and pair observations. This Map can be 
	 * variable in size (up to some fixed maximum size) and elements are considered unordered. Internally this observation uses Attention so can
	 * be slower to evaluate and more difficult to train than other observation types. For this reason it should be used sparingly.
	 *
	 * @param Schema The Observation Schema
	 * @param KeyElement The sub-observation that represents keys in this map.
	 * @param ValueElement The sub-observation that represents values in this map.
	 * @param MaxNum The maximum number of elements that can be included in the map.
	 * @param AttentionEncodingSize The encoding size used by the attention mechanism.
	 * @param AttentionHeadNum The number of heads used by the attention mechanism.
	 * @param ValueEncodingSize The output encoding size used by the attention mechanism.
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsObservationSchemaElement SpecifyMapObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement KeyElement, const FLearningAgentsObservationSchemaElement ValueElement, const int32 MaxNum, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Tag = TEXT("MapObservation"));

	/**
	 * Specifies a new enum observation. This represents an exclusive choice from elements of the given Enum. To use this with an Enum defined in C++ 
	 * use the FindEnumByName convenience function.
	 *
	 * @param Schema The Observation Schema
	 * @param Enum The enum type to use.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyEnumObservation(ULearningAgentsObservationSchema* Schema, const UEnum* Enum, const FName Tag = TEXT("EnumObservation"));

	/**
	 * Specifies a new bitmask observation. This represents an inclusive choice from elements of the given Enum. To use this with an Enum defined in 
	 * C++ use the FindEnumByName convenience function.
	 *
	 * @param Schema The Observation Schema
	 * @param Enum The enum type to use.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyBitmaskObservation(ULearningAgentsObservationSchema* Schema, const UEnum* Enum, const FName Tag = TEXT("BitmaskObservation"));

	/**
	 * Specifies a new optional observation. This represents an observation which may or may not be provided.
	 *
	 * @param Schema The Observation Schema
	 * @param Elements The sub-observation that may or may not be provided.
	 * @param EncodingSize The encoding size used to encode this sub-observation.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyOptionalObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize = 128, const FName Tag = TEXT("OptionalObservation"));

	/**
	 * Specifies a new either observation. This represents an observation which will be either sub-observation A or sub-observation B.
	 *
	 * @param Schema The Observation Schema
	 * @param A The first sub-observation.
	 * @param A The second sub-observation.
	 * @param EncodingSize The encoding size used to encode each sub-observation.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationSchemaElement SpecifyEitherObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement A, const FLearningAgentsObservationSchemaElement B, const int32 EncodingSize = 128, const FName Tag = TEXT("EitherObservation"));

	/**
	 * Specifies a new encoding observation. This represents an observation which will be an encoding of another sub-observation using a small neural
	 * network.
	 *
	 * @param Schema The Observation Schema
	 * @param Element The sub-observation to be encoded.
	 * @param EncodingSize The encoding size used to encode this sub-observation.
	 * @param HiddenLayerNum The number of hidden layers used to encode this sub-observation.
	 * @param ActivationFunction The activation function used to encode this sub-observation.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationSchemaElement SpecifyEncodingObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize = 128, const int32 HiddenLayerNum = 1, const ELearningAgentsActivationFunction ActivationFunction = ELearningAgentsActivationFunction::ELU, const FName Tag = TEXT("EncodingObservation"));

	/**
	 * Specifies a new bool observation. A true or false observation.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyBoolObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("BoolObservation"));

	/**
	 * Specifies a new float observation. A simple observation which can be used as a catch-all for situations where a
	 * type-specific observation does not exist.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyFloatObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("FloatObservation"));

	/**
	 * Specifies a new location observation. Allows an agent to observe the location of some entity.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyLocationObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("LocationObservation"));

	/**
	 * Specifies a new rotation observation. Allows an agent to observe the rotation of some entity. Rotations are encoded as two columns of the 
	 * rotation matrix to ensure there is no discontinuity in the encoding.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyRotationObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("RotationObservation"));

	/**
	 * Specifies a new scale observation. Allows an agent to observe the scale of some entity. Negative scales are not supported by this observation 
	 * type.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyScaleObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("ScaleObservation"));

	/**
	 * Specifies a new transform observation. Allows an agent to observe the transform of some entity.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyTransformObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("TransformObservation"));

	/**
	 * Specifies a new angle observation. This will be encoded as a 2-dimension Cartesian vector so that 0 and 350 are close to each other in the 
	 * encoded space.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyAngleObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("AngleObservation"));

	/**
	 * Specifies a new velocity observation. Allows an agent to observe the velocity of some entity.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyVelocityObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("VelocityObservation"));

	/**
	 * Specifies a new direction observation. Allows an agent to observe the direction of some entity.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyDirectionObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("DirectionObservation"));

	/**
	 * Specifies a new location along spline observation. This observes the location of the spline at the given distance along that spline.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyLocationAlongSplineObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("LocationAlongSplineObservation"));

	/**
	 * Specifies a new proportion along spline observation. This observes the proportion along a spline at the given distance. For looped splines 
	 * this will be treated effectively like an angle between 0 and 360 degrees and encoded appropriately so that 0 and 350 are close to each other in the 
	 * encoded space, while for non-looped splines this will be treated as a value between 0 and 1.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyProportionAlongSplineObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("ProportionAlongSplineObservation"));

	/**
	 * Specifies a new direction along spline observation. This observes the direction of the spline at the given distance along that spline.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyDirectionAlongSplineObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("DirectionAlongSplineObservation"));

	/**
	 * Specifies a new proportion along ray observation. This observes how far a you can travel along a ray before collision. Rays that can travel 
	 * the full distance are encoded as zero, while rays that collide instantly are encoded as one.
	 *
	 * @param Schema The Observation Schema
	 * @param Tag The tag of this new observation. Used during observation object validation and debugging.
	 * @return The newly created observation schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationSchemaElement SpecifyProportionAlongRayObservation(ULearningAgentsObservationSchema* Schema, const FName Tag = TEXT("ProportionAlongRayObservation"));

public:

	/**
	 * Make a new null observation.
	 *
	 * @param Object The Observation Object
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationObjectElement MakeNullObservation(ULearningAgentsObservationObject* Object, const FName Tag = TEXT("NullObservation"));

	/**
	 * Make a new continuous observation. The size of Values must match the Size given during Specify.
	 *
	 * @param Object The Observation Object
	 * @param Values The observation values.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeContinuousObservation(
		ULearningAgentsObservationObject* Object, 
		const TArray<float>& Values, 
		const FName Tag = TEXT("ContinuousObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new continuous observation. The size of Values must match the Size given during Specify.
	 *
	 * @param Object The Observation Object
	 * @param Values The observation values.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeContinuousObservationFromArrayView(
		ULearningAgentsObservationObject* Object, 
		const TArrayView<const float> Values, 
		const FName Tag = TEXT("ContinuousObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new exclusive discrete observation.
	 *
	 * @param Object The Observation Object
	 * @param DiscreteIndex The index of the discrete observation. Values must be smaller than the given Size.
	 * @param Size The size of the discrete observation. Must be equal to the size given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeExclusiveDiscreteObservation(
		ULearningAgentsObservationObject* Object, 
		const int32 DiscreteIndex, 
		const int32 Size,
		const FName Tag = TEXT("ExclusiveDiscreteObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new inclusive discrete observation.
	 *
	 * @param Object The Observation Object
	 * @param DiscreteIndices The indices of the discrete observations. All values must be smaller than the given Size.
	 * @param Size The size of the discrete observation. Must be equal to the size given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeInclusiveDiscreteObservation(
		ULearningAgentsObservationObject* Object, 
		const TArray<int32>& DiscreteIndices, 
		const int32 Size, 
		const FName Tag = TEXT("InclusiveDiscreteObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);
	
	/**
	 * Make a new inclusive discrete observation.
	 *
	 * @param Object The Observation Object
	 * @param DiscreteIndices The indices of the discrete observations. All values must be smaller than the given Size.
	 * @param Size The size of the discrete observation. Must be equal to the size given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeInclusiveDiscreteObservationFromArrayView(
		ULearningAgentsObservationObject* Object, 
		const TArrayView<const int32> DiscreteIndices, 
		const int32 Size, 
		const FName Tag = TEXT("InclusiveDiscreteObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new count observation.
	 *
	 * @param Object The Observation Object
	 * @param Num The number of items. Must be less than or equal to MaxNum.
	 * @param MaxNum The maximum number of items possible.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeCountObservation(
		ULearningAgentsObservationObject* Object, 
		const int32 Num, 
		const int32 MaxNum, 
		const FName Tag = TEXT("CountObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new struct observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The named sub-observations. Must match what was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationObjectElement MakeStructObservation(ULearningAgentsObservationObject* Object, const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Tag = TEXT("StructObservation"));

	/**
	 * Make a new struct observation.
	 *
	 * @param Object The Observation Object
	 * @param ElementNames The names of the sub-observations. Must match what was given during Specify.
	 * @param Elements The corresponding sub-observations. Must match what was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationObjectElement MakeStructObservationFromArrays(ULearningAgentsObservationObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag = TEXT("StructObservation"));
	
	/**
	 * Make a new struct observation.
	 *
	 * @param Object The Observation Object
	 * @param ElementNames The names of the sub-observations. Must match what was given during Specify.
	 * @param Elements The corresponding sub-observations. Must match what was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeStructObservationFromArrayViews(ULearningAgentsObservationObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag = TEXT("StructObservation"));

	/**
	 * Make a new exclusive union observation.
	 *
	 * @param Object The Observation Object
	 * @param ElementName The name of the chosen sub-observation.
	 * @param Element The corresponding chosen sub-observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationObjectElement MakeExclusiveUnionObservation(ULearningAgentsObservationObject* Object, const FName ElementName, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ExclusiveUnionObservation"));

	/**
	 * Make a new inclusive union observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The chosen sub-observations.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "Elements"))
	static FLearningAgentsObservationObjectElement MakeInclusiveUnionObservation(ULearningAgentsObservationObject* Object, const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Tag = TEXT("InclusiveUnionObservation"));

	/**
	 * Make a new inclusive union observation.
	 *
	 * @param Object The Observation Object
	 * @param ElementNames The names of the chosen sub-observations.
	 * @param Elements The corresponding chosen sub-observations.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, AutoCreateRefTerm = "ElementNames,Elements"))
	static FLearningAgentsObservationObjectElement MakeInclusiveUnionObservationFromArrays(ULearningAgentsObservationObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag = TEXT("InclusiveUnionObservation"));
	
	/**
	 * Make a new inclusive union observation.
	 *
	 * @param Object The Observation Object
	 * @param ElementNames The names of the chosen sub-observations.
	 * @param Elements The corresponding chosen sub-observations.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeInclusiveUnionObservationFromArrayViews(ULearningAgentsObservationObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag = TEXT("InclusiveUnionObservation"));

	/**
	 * Make a new static array observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must match what was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationObjectElement MakeStaticArrayObservation(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag = TEXT("StaticArrayObservation"));

	/**
	 * Make a new static array observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must match what was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeStaticArrayObservationFromArrayView(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag = TEXT("StaticArrayObservation"));

	/**
	 * Make a new set observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "Elements"))
	static FLearningAgentsObservationObjectElement MakeSetObservation(ULearningAgentsObservationObject* Object, const TSet<FLearningAgentsObservationObjectElement>& Elements, const FName Tag = TEXT("SetObservation"));

	/**
	 * Make a new set observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "Elements"))
	static FLearningAgentsObservationObjectElement MakeSetObservationFromArray(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag = TEXT("SetObservation"));

	/**
	 * Make a new set observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeSetObservationFromArrayView(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag = TEXT("SetObservation"));

	/**
	 * Make a new pair observation.
	 *
	 * @param Object The Observation Object
	 * @param Key The key sub-observation.
	 * @param Key The value sub-observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationObjectElement MakePairObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Key, const FLearningAgentsObservationObjectElement Value, const FName Tag = TEXT("PairObservation"));

	/**
	 * Make a new array observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param MaxNum The maximum number of elements possible for this observation. Must match what was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, AutoCreateRefTerm = "Elements"))
	static FLearningAgentsObservationObjectElement MakeArrayObservation(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Elements, const int32 MaxNum, const FName Tag = TEXT("ArrayObservation"));

	/**
	 * Make a new array observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param MaxNum The maximum number of elements possible for this observation. Must match what was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeArrayObservationFromArrayView(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const int32 MaxNum, const FName Tag = TEXT("ArrayObservation"));

	/**
	 * Make a new map observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "Map"))
	static FLearningAgentsObservationObjectElement MakeMapObservation(ULearningAgentsObservationObject* Object, const TMap<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement>& Map, const FName Tag = TEXT("MapObservation"));

	/**
	 * Make a new map observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, AutoCreateRefTerm = "Keys,Values"))
	static FLearningAgentsObservationObjectElement MakeMapObservationFromArrays(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Keys, const TArray<FLearningAgentsObservationObjectElement>& Values, const FName Tag = TEXT("MapObservation"));
	
	/**
	 * Make a new map observation.
	 *
	 * @param Object The Observation Object
	 * @param Elements The sub-observations. The number of elements here must be less than or equal to the maximum that was given during Specify.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	static FLearningAgentsObservationObjectElement MakeMapObservationFromArrayViews(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Keys, const TArrayView<const FLearningAgentsObservationObjectElement> Values, const FName Tag = TEXT("MapObservation"));

	/**
	 * Make a new enum observation.
	 *
	 * @param Object The Observation Object
	 * @param Enum The enum type for this observation. Must match what was given during Specify.
	 * @param EnumValue The enum value.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeEnumObservation(
		ULearningAgentsObservationObject* Object, 
		const UEnum* Enum, 
		const uint8 EnumValue, 
		const FName Tag = TEXT("EnumObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new bitmask observation.
	 *
	 * @param Object The Observation Object
	 * @param Enum The enum type for this observation. Must match what was given during Specify.
	 * @param BitmaskValue The bitmask value.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeBitmaskObservation(
		ULearningAgentsObservationObject* Object, 
		const UEnum* Enum, 
		const int32 BitmaskValue, 
		const FName Tag = TEXT("BitmaskObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new optional observation.
	 *
	 * @param Object The Observation Object
	 * @param Element The sub-observation given.
	 * @param Option The indicator as to if this is observation should be used.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationObjectElement MakeOptionalObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const ELearningAgentsOptionalObservation Option, const FName Tag = TEXT("OptionalObservation"));

	/**
	 * Make a new null optional observation. Use this to provide a null optional observation.
	 *
	 * @param Object The Observation Object
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsObservationObjectElement MakeOptionalNullObservation(ULearningAgentsObservationObject* Object, const FName Tag = TEXT("OptionalObservation"));

	/**
	 * Make a new valid optional observation. Use this to provide a valid optional observation.
	 *
	 * @param Object The Observation Object
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationObjectElement MakeOptionalValidObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("OptionalObservation"));

	/**
	 * Make a new either observation.
	 *
	 * @param Object The Observation Object
	 * @param Element The sub-observation given.
	 * @param Option The indicator as to if this is observation A or B.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsObservationObjectElement MakeEitherObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const ELearningAgentsEitherObservation Either, const FName Tag = TEXT("EitherObservation"));

	/**
	 * Make a new either A observation. Use this to provide option A.
	 *
	 * @param Object The Observation Object
	 * @param Element The sub-observation given.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, DisplayName = "Make Either A Observation"))
	static FLearningAgentsObservationObjectElement MakeEitherAObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement A, const FName Tag = TEXT("EitherObservation"));

	/**
	 * Make a new either B observation. Use this to provide option B.
	 *
	 * @param Object The Observation Object
	 * @param Element The sub-observation given.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, DisplayName = "Make Either B Observation"))
	static FLearningAgentsObservationObjectElement MakeEitherBObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement B, const FName Tag = TEXT("EitherObservation"));

	/**
	 * Make a new encoding observation. This must be used in conjunction with SpecifyEncodingObservation.
	 * 
	 * @param Object The Observation Object
	 * @param Element The Observation Element to be encoded.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsObservationObjectElement MakeEncodingObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("EncodingObservation"));

	/**
	 * Make a new bool observation.
	 *
	 * @param Object The Observation Object
	 * @param Value The new value of this observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeBoolObservation(
		ULearningAgentsObservationObject* Object,
		const bool bValue,
		const FName Tag = TEXT("BoolObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new float observation.
	 *
	 * @param Object The Observation Object
	 * @param Value The new value of this observation.
	 * @param FloatScale Used to normalize the data for this observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeFloatObservation(
		ULearningAgentsObservationObject* Object,
		const float Value,
		const float FloatScale = 1.0f,
		const FName Tag = TEXT("FloatObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new location observation.
	 *
	 * @param Object The Observation Object
	 * @param Location The location of interest to the agent.
	 * @param RelativeTransform The transform the provided location should be encoded relative to.
	 * @param LocationScale Used to normalize the data for this observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeLocationObservation(
		ULearningAgentsObservationObject* Object,
		const FVector Location,
		const FTransform RelativeTransform = FTransform(),
		const float LocationScale = 100.0f,
		const FName Tag = TEXT("LocationObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new rotation observation.
	 *
	 * @param Object The Observation Object
	 * @param Rotation The rotation of interest to the agent.
	 * @param RelativeRotation The rotation the provided rotation should be encoded relative to.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerRotationLocation A location for the visual logger to display the rotation in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeRotationObservation(
		ULearningAgentsObservationObject* Object,
		const FRotator Rotation,
		const FRotator RelativeRotation = FRotator::ZeroRotator,
		const FName Tag = TEXT("RotationObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerRotationLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new rotation observation from a quaternion.
	 *
	 * @param Object The Observation Object
	 * @param Rotation The rotation of interest to the agent.
	 * @param RelativeRotation The rotation the provided rotation should be encoded relative to.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerRotationLocation A location for the visual logger to display the rotation in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeRotationObservationFromQuat(
		ULearningAgentsObservationObject* Object,
		const FQuat Rotation,
		const FQuat RelativeRotation,
		const FName Tag = TEXT("RotationObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerRotationLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new scale observation.
	 *
	 * @param Object The Observation Object
	 * @param Scale The scale of interest to the agent.
	 * @param RelativeScale The scale the provided scale should be encoded relative to.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerScaleLocation A location for the visual logger to display the scale in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeScaleObservation(
		ULearningAgentsObservationObject* Object,
		const FVector Scale,
		const FVector RelativeScale = FVector(1, 1, 1),
		const FName Tag = TEXT("ScaleObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerScaleLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new transform observation.
	 *
	 * @param Object The Observation Object
	 * @param Transform The transform of interest to the agent.
	 * @param RelativeTransform The transform the provided transform should be encoded relative to.
	 * @param LocationScale Used to normalize the transform's location for this observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeTransformObservation(
		ULearningAgentsObservationObject* Object,
		const FTransform Transform,
		const FTransform RelativeTransform = FTransform(),
		const float LocationScale = 100.0f,
		const FName Tag = TEXT("TransformObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new angle observation. Angles should be given in degrees.
	 *
	 * @param Object The Observation Object
	 * @param Angle The angle of interest to the agent.
	 * @param RelativeAngle The angle the provided angle should be encoded relative to.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeAngleObservation(
		ULearningAgentsObservationObject* Object, 
		const float Angle, 
		const float RelativeAngle = 0.0f, 
		const FName Tag = TEXT("AngleObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new angle observation. Angles should be given in radians.
	 * 
	 * @param Object The Observation Object
	 * @param Angle The angle of interest to the agent.
	 * @param RelativeAngle The angle the provided angle should be encoded relative to.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeAngleObservationRadians(
		ULearningAgentsObservationObject* Object, 
		const float Angle, 
		const float RelativeAngle = 0.0f, 
		const FName Tag = TEXT("AngleObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new velocity observation.
	 *
	 * @param Object The Observation Object
	 * @param Velocity The velocity of interest to the agent.
	 * @param RelativeTransform The transform the provided velocity should be encoded relative to.
	 * @param VelocityScale Used to normalize the data for this observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerVelocityLocation A location for the visual logger to display the velocity in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeVelocityObservation(
		ULearningAgentsObservationObject* Object, 
		const FVector Velocity, 
		const FTransform RelativeTransform = FTransform(), 
		const float VelocityScale = 200.0f, 
		const FName Tag = TEXT("VelocityObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerVelocityLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new direction observation.
	 *
	 * @param Object The Observation Object
	 * @param Direction The direction of interest to the agent.
	 * @param RelativeTransform The transform the provided direction should be encoded relative to.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerDirectionLocation A location for the visual logger to display the direction in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerArrowLength The length of the arrow to display to represent the direction.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeDirectionObservation(
		ULearningAgentsObservationObject* Object,
		const FVector Direction,
		const FTransform RelativeTransform = FTransform(),
		const FName Tag = TEXT("DirectionObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerDirectionLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const float VisualLoggerArrowLength = 100.0f,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new location along spline observation.
	 *
	 * @param Object The Observation Object
	 * @param SplineComponent The spline to observe.
	 * @param DistanceAlongSpline The distance along that spline.
	 * @param RelativeTransform The transform the provided location should be encoded relative to.
	 * @param LocationScale Used to normalize the transform's location for this observation.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeLocationAlongSplineObservation(
		ULearningAgentsObservationObject* Object,
		const USplineComponent* SplineComponent,
		const float DistanceAlongSpline,
		const FTransform RelativeTransform = FTransform(),
		const float LocationScale = 100.0f,
		const FName Tag = TEXT("LocationAlongSplineObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new proportion along spline observation.
	 *
	 * @param Object The Observation Object
	 * @param SplineComponent The spline to observe.
	 * @param DistanceAlongSpline The distance along that spline.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeProportionAlongSplineObservation(
		ULearningAgentsObservationObject* Object, 
		const USplineComponent* SplineComponent, 
		const float DistanceAlongSpline, 
		const FName Tag = TEXT("ProportionAlongSplineObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new direction along spline observation.
	 *
	 * @param Object The Observation Object
	 * @param SplineComponent The spline to observe.
	 * @param DistanceAlongSpline The distance along that spline.
	 * @param RelativeTransform The transform the provided direction should be encoded relative to.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerArrowLength The length of the arrow to display to represent the direction.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeDirectionAlongSplineObservation(
		ULearningAgentsObservationObject* Object,
		const USplineComponent* SplineComponent,
		const float DistanceAlongSpline,
		const FTransform RelativeTransform = FTransform(),
		const FName Tag = TEXT("DirectionAlongSplineObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const float VisualLoggerArrowLength = 100.0f,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	/**
	 * Make a new proportion along ray observation. 
	 *
	 * @param Object The Observation Object
	 * @param RayStart The local ray start location.
	 * @param RayEnd The local ray end location.
	 * @param RayTransform The transform to use to transform the local ray starts and ends into the world space.
	 * @param CollisionChannel The collision channel to collide against.
	 * @param Tag The tag of the corresponding observation. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this observation. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this observation.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The newly created observation object element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static FLearningAgentsObservationObjectElement MakeProportionAlongRayObservation(
		ULearningAgentsObservationObject* Object, 
		const FVector RayStart, 
		const FVector RayEnd, 
		const FTransform RayTransform = FTransform(), 
		const ECollisionChannel CollisionChannel = ECollisionChannel::ECC_WorldStatic, 
		const FName Tag = TEXT("ProportionAlongRayObservation"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

public:

	// Get Basic Observations

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 2, ReturnDisplayName = "Success"))
	static bool GetNullObservation(const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("NullObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetContinuousObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ContinuousObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetContinuousObservation(TArray<float>& OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ContinuousObservation"));
	static bool GetContinuousObservationToArrayView(TArrayView<float> OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ContinuousObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetExclusiveDiscreteObservation(int32& OutIndex, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ExclusiveDiscreteObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetInclusiveDiscreteObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("InclusiveDiscreteObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetInclusiveDiscreteObservation(TArray<int32>& OutIndices, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("InclusiveDiscreteObservation"));
	static bool GetInclusiveDiscreteObservationToArrayView(TArrayView<int32> OutIndices, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("InclusiveDiscreteObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetCountObservation(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const int32 MaxNum, const FName Tag = TEXT("CountObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStructObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("StructObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStructObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("StructObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetStructObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("StructObservation"));
	static bool GetStructObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("StructObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetExclusiveUnionObservation(FName& OutElementName, FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ExclusiveUnionObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetInclusiveUnionObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("InclusiveUnionObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetInclusiveUnionObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("InclusiveUnionObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetInclusiveUnionObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("InclusiveUnionObservation"));
	static bool GetInclusiveUnionObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("InclusiveUnionObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStaticArrayObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("StaticArrayObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStaticArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("StaticArrayObservation"));
	static bool GetStaticArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("StaticArrayObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetSetObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("SetObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetSetObservation(TSet<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("SetObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetSetObservationToArray(TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("SetObservation"));
	static bool GetSetObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("SetObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetPairObservation(FLearningAgentsObservationObjectElement& OutKey, FLearningAgentsObservationObjectElement& OutValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("PairObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetArrayObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ArrayObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const int32 MaxNum, const FName Tag = TEXT("ArrayObservation"));
	static bool GetArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const int32 MaxNum, const FName Tag = TEXT("ArrayObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetMapObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("MapObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetMapObservation(TMap<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("MapObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetMapObservationToArrays(TArray<FLearningAgentsObservationObjectElement>& OutKeys, TArray<FLearningAgentsObservationObjectElement>& OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("MapObservation"));
	static bool GetMapObservationToArrayViews(TArrayView<FLearningAgentsObservationObjectElement> OutKeys, TArrayView<FLearningAgentsObservationObjectElement> OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("MapObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetEnumObservation(uint8& OutEnumValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const UEnum* Enum, const FName Tag = TEXT("EnumObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetBitmaskObservation(int32& OutBitmaskValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const UEnum* Enum, const FName Tag = TEXT("BitmaskObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ExpandEnumAsExecs = "OutOption", ReturnDisplayName = "Success"))
	static bool GetOptionalObservation(ELearningAgentsOptionalObservation& OutOption, FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("OptionalObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ExpandEnumAsExecs = "OutEither", ReturnDisplayName = "Success"))
	static bool GetEitherObservation(ELearningAgentsEitherObservation& OutEither, FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("EitherObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetEncodingObservation(FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("EncodingObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetBoolObservation(bool& bOutValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("BoolObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetFloatObservation(float& OutValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const float FloatScale = 1.0f, const FName Tag = TEXT("FloatObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success"))
	static bool GetLocationObservation(FVector& OutLocation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Tag = TEXT("LocationObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetRotationObservation(FRotator& OutRotation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FRotator RelativeRotation = FRotator::ZeroRotator, const FName Tag = TEXT("RotationObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetRotationObservationAsQuat(FQuat& OutRotation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FQuat RelativeRotation, const FName Tag = TEXT("RotationObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetScaleObservation(FVector& OutScale, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FVector RelativeScale = FVector(1, 1, 1), const FName Tag = TEXT("ScaleObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success"))
	static bool GetTransformObservation(FTransform& OutTransform, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Tag = TEXT("TransformObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetAngleObservation(float& OutAngle, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle = 0.0f, const FName Tag = TEXT("AngleObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetAngleObservationRadians(float& OutAngle, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle = 0.0f, const FName Tag = TEXT("AngleObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success"))
	static bool GetVelocityObservation(FVector& OutVelocity, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float VelocityScale = 200.0f, const FName Tag = TEXT("VelocityObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetDirectionObservation(FVector& OutDirection, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const FName Tag = TEXT("DirectionObservation"));

	// Get Spline Observations

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success"))
	static bool GetLocationAlongSplineObservation(FVector& OutLocation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Tag = TEXT("LocationAlongSplineObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success"))
	static bool GetProportionAlongSplineObservation(bool& bOutIsClosedLoop, float& OutAngle, float& OutPropotion, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ProportionAlongSplineObservation"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetDirectionAlongSplineObservation(FVector& OutDirection, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const FName Tag = TEXT("DirectionAlongSplineObservation"));

	// Get Ray Cast Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetProportionAlongRayObservation(float& OutProportion, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag = TEXT("ProportionAlongRayObservation"));

};


