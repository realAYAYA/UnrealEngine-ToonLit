// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningAction.h"

#include "LearningAgentsNeuralNetwork.h" // Included for ELearningAgentsActivationFunction

#include "Engine/EngineTypes.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "LearningAgentsActions.generated.h"

class ULearningAgentsActionSchema;
class ULearningAgentsActionObject;
struct FLearningAgentsActionSchemaElement;
struct FLearningAgentsActionObjectElement;

/** An element of an Action Schema */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsActionSchemaElement
{
	GENERATED_BODY()

	UE::Learning::Action::FSchemaElement SchemaElement;
};

/** An element of an Action Object */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsActionObjectElement
{
	GENERATED_BODY()

	UE::Learning::Action::FObjectElement ObjectElement;
};

/** Comparison operator for Action Object Elements */
bool operator==(const FLearningAgentsActionObjectElement& Lhs, const FLearningAgentsActionObjectElement& Rhs);

/** Hashing operator for Action Object Elements */
uint32 GetTypeHash(const FLearningAgentsActionObjectElement& Element);

template<>
struct TStructOpsTypeTraits<FLearningAgentsActionObjectElement> : public TStructOpsTypeTraitsBase2<FLearningAgentsActionObjectElement>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};

/**
 * Action Schema
 *
 * This object is used to construct a schema describing some structure of actions.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsActionSchema : public UObject
{
	GENERATED_BODY()

public:

	UE::Learning::Action::FSchema ActionSchema;
};

/**
 * Action Object
 *
 * This object is used to construct or get the values of actions.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsActionObject : public UObject
{
	GENERATED_BODY()

public:

	UE::Learning::Action::FObject ActionObject;
};

/** Enum Type representing either action A or action B */
UENUM(BlueprintType)
enum class ELearningAgentsEitherAction : uint8
{
	A,
	B,
};

/** Enum Type representing either a Null action or some Valid action */
UENUM(BlueprintType)
enum class ELearningAgentsOptionalAction : uint8
{
	Null,
	Valid,
};

UCLASS()
class LEARNINGAGENTS_API ULearningAgentsActions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Validates that the given action object matches the schema. Will log errors on objects that don't match.
	 *
	 * @param Schema				Action Schema
	 * @param SchemaElement			Action Schema Element
	 * @param Object				Action Object
	 * @param ObjectElement			Action Object Element
	 * @returns						true if the object matches the schema
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	static bool ValidateActionObjectMatchesSchema(
		const ULearningAgentsActionSchema* Schema,
		const FLearningAgentsActionSchemaElement SchemaElement,
		const ULearningAgentsActionObject* Object,
		const FLearningAgentsActionObjectElement ObjectElement);

	/**
	 * Logs an Action Object Element. Useful for debugging.
	 *
	 * @param Object				Action Object
	 * @param ObjectElement			Action Object Element
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	static void LogAction(const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element);

public:

	/**
	 * Specifies a new null action. This represents an empty action and can be useful when an action is needed which does nothing.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyNullAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("NullAction"));

	/**
	 * Specifies a new continuous action. This represents an action made up of several float values sampled from a Gaussian distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Size The number of float values in the action.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionSchemaElement SpecifyContinuousAction(ULearningAgentsActionSchema* Schema, const int32 Size, const FName Tag = TEXT("ContinuousAction"));

	/**
	 * Specifies a new exclusive discrete action. This represents an action which is an exclusive choice from a number of discrete options, sampled 
	 * from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Size The number of discrete options in the action.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a uniform distribution over options. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyExclusiveDiscreteAction(ULearningAgentsActionSchema* Schema, const int32 Size, const TArray<float>& PriorProbabilities, const FName Tag = TEXT("DiscreteExclusiveAction"));
	
	/**
	 * Specifies a new exclusive discrete action. This represents an action which is an exclusive choice from a number of discrete options, sampled
	 * from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Size The number of discrete options in the action.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a uniform distribution over options. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	static FLearningAgentsActionSchemaElement SpecifyExclusiveDiscreteActionFromArrayView(ULearningAgentsActionSchema* Schema, const int32 Size, const TArrayView<const float> PriorProbabilities = {}, const FName Tag = TEXT("DiscreteExclusiveAction"));

	/**
	 * Specifies a new inclusive discrete action. This represents an action which is an inclusive choice from a number of discrete options, sampled
	 * from a Bernoulli distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Size The number of discrete options in the action.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a probability of 0.5 for each option.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyInclusiveDiscreteAction(ULearningAgentsActionSchema* Schema, const int32 Size, const TArray<float>& PriorProbabilities, const FName Tag = TEXT("DiscreteInclusiveAction"));
	
	/**
	 * Specifies a new inclusive discrete action. This represents an action which is an inclusive choice from a number of discrete options, sampled
	 * from a Bernoulli distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Size The number of discrete options in the action.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a probability of 0.5 for each option.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	static FLearningAgentsActionSchemaElement SpecifyInclusiveDiscreteActionFromArrayView(ULearningAgentsActionSchema* Schema, const int32 Size, const TArrayView<const float> PriorProbabilities = {}, const FName Tag = TEXT("DiscreteInclusiveAction"));

	/**
	 * Specifies a new struct action. This represents an action which is made up of a number of named sub-actions.
	 *
	 * @param Schema The Action Schema
	 * @param Elements The sub-actions.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionSchemaElement SpecifyStructAction(ULearningAgentsActionSchema* Schema, const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const FName Tag = TEXT("StructAction"));

	/**
	 * Specifies a new struct action. This represents an action which is made up of a number of named sub-actions.
	 *
	 * @param Schema The Action Schema
	 * @param ElementNames The names of the sub-actions.
	 * @param Elements The corresponding sub-actions. Must be the same size as ElementNames.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionSchemaElement SpecifyStructActionFromArrays(ULearningAgentsActionSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const FName Tag = TEXT("StructAction"));
	
	/**
	 * Specifies a new struct action. This represents an action which is made up of a number of named sub-actions.
	 *
	 * @param Schema The Action Schema
	 * @param ElementNames The names of the sub-actions.
	 * @param Elements The corresponding sub-actions. Must be the same size as ElementNames.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	static FLearningAgentsActionSchemaElement SpecifyStructActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const FName Tag = TEXT("StructAction"));

	/**
	 * Specifies a new exclusive union action. This represents an action which is an exclusive choice from a number of named sub-actions, sampled
	 * from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Elements The sub-actions.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a uniform distribution over options. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyExclusiveUnionAction(ULearningAgentsActionSchema* Schema, const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Tag = TEXT("ExclusiveUnionAction"));

	/**
	 * Specifies a new exclusive union action. This represents an action which is an exclusive choice from a number of named sub-actions, sampled
	 * from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param ElementNames The names of the sub-actions.
	 * @param Elements The corresponding sub-actions. Must be the same size as ElementNames.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a uniform distribution over options. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyExclusiveUnionActionFromArrays(ULearningAgentsActionSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Tag = TEXT("ExclusiveUnionAction"));
	
	/**
	 * Specifies a new exclusive union action. This represents an action which is an exclusive choice from a number of named sub-actions, sampled
	 * from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param ElementNames The names of the sub-actions.
	 * @param Elements The corresponding sub-actions. Must be the same size as ElementNames.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a uniform distribution over options. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	static FLearningAgentsActionSchemaElement SpecifyExclusiveUnionActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities = {}, const FName Tag = TEXT("ExclusiveUnionAction"));

	/**
	 * Specifies a new inclusive union action. This represents an action which is an inclusive choice from a number of named sub-actions, sampled
	 * from a Bernoulli distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Elements The sub-actions.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a probability of 0.5 for each option.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyInclusiveUnionAction(ULearningAgentsActionSchema* Schema, const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Tag = TEXT("InclusiveUnionAction"));

	/**
	 * Specifies a new inclusive union action. This represents an action which is an inclusive choice from a number of named sub-actions, sampled
	 * from a Bernoulli distribution.
	 *
	 * @param Schema The Action Schema
	 * @param ElementNames The names of the sub-actions.
	 * @param Elements The corresponding sub-actions. Must be the same size as ElementNames.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a probability of 0.5 for each option.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyInclusiveUnionActionFromArrays(ULearningAgentsActionSchema* Schema, const TArray<FName> ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Tag = TEXT("InclusiveUnionAction"));
	
	/**
	 * Specifies a new inclusive union action. This represents an action which is an inclusive choice from a number of named sub-actions, sampled
	 * from a Bernoulli distribution.
	 *
	 * @param Schema The Action Schema
	 * @param ElementNames The names of the sub-actions.
	 * @param Elements The corresponding sub-actions. Must be the same size as ElementNames.
	 * @param PriorProbabilities The prior probabilities of each option. Can be left empty to use a probability of 0.5 for each option.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	static FLearningAgentsActionSchemaElement SpecifyInclusiveUnionActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities = {}, const FName Tag = TEXT("InclusiveUnionAction"));

	/**
	 * Specifies a new static array action. This represents an action which is a fixed sized array of some sub-action.
	 *
	 * @param Schema The Action Schema
	 * @param Element The sub-action.
	 * @param Num The number of elements in the array.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionSchemaElement SpecifyStaticArrayAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Element, const int32 Num, const FName Tag = TEXT("StaticArrayAction"));

	/**
	 * Specifies a new pair action. This represents an action which is made up of a key and value sub-actions.
	 *
	 * @param Schema The Action Schema
	 * @param Key The key sub-action.
	 * @param Value The value sub-action.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionSchemaElement SpecifyPairAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Key, const FLearningAgentsActionSchemaElement Value, const FName Tag = TEXT("PairAction"));

	/**
	 * Specifies a new enum action. This represents an action which is an exclusive choice from entries of an Enum, sampled from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Enum The Enum type.
	 * @param PriorProbabilities The prior probabilities of each enum element. Can be left empty to use a uniform distribution over elements. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyEnumAction(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Tag = TEXT("EnumAction"));

	/**
	 * Specifies a new enum action. This represents an action which is an exclusive choice from entries of an Enum, sampled from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Enum The Enum type.
	 * @param PriorProbabilities The prior probabilities of each enum element. Can be left empty to use a uniform distribution over elements. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyEnumActionFromArray(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Tag = TEXT("EnumAction"));
	
	/**
	 * Specifies a new enum action. This represents an action which is an exclusive choice from entries of an Enum, sampled from a Categorical distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Enum The Enum type.
	 * @param PriorProbabilities The prior probabilities of each enum element. Can be left empty to use a uniform distribution over elements. Should sum to one.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	static FLearningAgentsActionSchemaElement SpecifyEnumActionFromArrayView(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArrayView<const float> PriorProbabilities = {}, const FName Tag = TEXT("EnumAction"));

	/**
	 * Specifies a new bitmask action. This represents an action which is an inclusive choice from entries of an Enum, sampled from a Bernoulli 
	 * distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Enum The Enum type.
	 * @param PriorProbabilities The prior probabilities of each enum element. Can be left empty to use a probability of 0.5 for each element.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyBitmaskAction(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Tag = TEXT("BitmaskAction"));

	/**
	 * Specifies a new bitmask action. This represents an action which is an inclusive choice from entries of an Enum, sampled from a Bernoulli
	 * distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Enum The Enum type.
	 * @param PriorProbabilities The prior probabilities of each enum element. Can be left empty to use a probability of 0.5 for each element.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, AutoCreateRefTerm = "PriorProbabilities"))
	static FLearningAgentsActionSchemaElement SpecifyBitmaskActionFromArray(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Tag = TEXT("BitmaskAction"));
	
	/**
	 * Specifies a new bitmask action. This represents an action which is an inclusive choice from entries of an Enum, sampled from a Bernoulli
	 * distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Enum The Enum type.
	 * @param PriorProbabilities The prior probabilities of each enum element. Can be left empty to use a probability of 0.5 for each element.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	static FLearningAgentsActionSchemaElement SpecifyBitmaskActionFromArrayView(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArrayView<const float> PriorProbabilities = {}, const FName Tag = TEXT("BitmaskAction"));

	/**
	 * Specifies a new optional action. This represents an action which may or may not be generated.
	 *
	 * @param Schema The Action Schema
	 * @param Element The sub-action.
	 * @param PriorProbabilities The prior probability of sampling this action.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionSchemaElement SpecifyOptionalAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Element, const float PriorProbability = 0.5f, const FName Tag = TEXT("OptionalAction"));

	/**
	 * Specifies a new either action. This represents an action which is either action A or action B.
	 *
	 * @param Schema The Action Schema
	 * @param A The sub-action A.
	 * @param B The sub-action B.
	 * @param PriorProbabilityOfA The prior probability of sampling action A over action B.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionSchemaElement SpecifyEitherAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement A, const FLearningAgentsActionSchemaElement B, const float PriorProbabilityOfA = 0.5f, const FName Tag = TEXT("EitherAction"));

	/**
	 * Specifies a new encoding action. This represents an action which will be a decoding of another sub-action using a small neural network.
	 *
	 * @param Schema The Action Schema
	 * @param Element The sub-action.
	 * @param EncodingSize The encoding size used to decode this sub-action.
	 * @param HiddenLayerNum The number of hidden layers used to decode this sub-action.
	 * @param ActivationFunction The activation function used to decode this sub-action.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionSchemaElement SpecifyEncodingAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Element, const int32 EncodingSize = 128, const int32 HiddenLayerNum = 1, const ELearningAgentsActivationFunction ActivationFunction = ELearningAgentsActivationFunction::ELU, const FName Tag = TEXT("EncodingAction"));

	/**
	 * Specifies a new bool action. This represents an action which is either true or false.
	 *
	 * @param Schema The Action Schema
	 * @param PriorProbability The prior probability of this action being true.
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyBoolAction(ULearningAgentsActionSchema* Schema, const float PriorProbability = 0.5f, const FName Tag = TEXT("BoolAction"));

	/**
	 * Specifies a new float action. This represents an action which is a single float sampled from a Gaussian distribution. It can be used as a 
	 * catch-all for situations where a type-specific action does not exist.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyFloatAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("FloatAction"));

	/**
	 * Specifies a new location action. This represents an action which is a location sampled from a Gaussian distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyLocationAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("LocationAction"));

	/**
	 * Specifies a new rotation action. This represents an action which is a rotation sampled from a Gaussian distribution in the angle-axis space.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyRotationAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("RotationAction"));

	/**
	 * Specifies a new scale action. This represents an action which is a scale sampled from a Gaussian distribution in the log space.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyScaleAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("ScaleAction"));

	/**
	 * Specifies a new transform action.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyTransformAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("TransformAction"));

	/**
	 * Specifies a new angle action. This represents an action which is an angle sampled from a Gaussian distribution centered around zero.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyAngleAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("AngleAction"));

	/**
	 * Specifies a new velocity action. This represents an action which is a velocity sampled from a Gaussian distribution.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyVelocityAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("VelocityAction"));

	/**
	 * Specifies a new direction action. This represents an action which is a direction sampled from a Gaussian distribution and normalized.
	 *
	 * @param Schema The Action Schema
	 * @param Tag The tag of this new action. Used during action object validation and debugging.
	 * @return The newly created action schema element.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionSchemaElement SpecifyDirectionAction(ULearningAgentsActionSchema* Schema, const FName Tag = TEXT("DirectionAction"));

public:

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionObjectElement MakeNullAction(ULearningAgentsActionObject* Object, const FName Tag = TEXT("NullAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeContinuousAction(ULearningAgentsActionObject* Object, const TArray<float>& Values, const FName Tag = TEXT("ContinuousAction"));
	static FLearningAgentsActionObjectElement MakeContinuousActionFromArrayView(ULearningAgentsActionObject* Object, const TArrayView<const float> Values, const FName Tag = TEXT("ContinuousAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeExclusiveDiscreteAction(ULearningAgentsActionObject* Object, const int32 Index, const FName Tag = TEXT("DiscreteExclusiveAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeInclusiveDiscreteAction(ULearningAgentsActionObject* Object, const TArray<int32>& Indices, const FName Tag = TEXT("DiscreteInclusiveAction"));
	static FLearningAgentsActionObjectElement MakeInclusiveDiscreteActionFromArrayView(ULearningAgentsActionObject* Object, const TArrayView<const int32> Indices, const FName Tag = TEXT("DiscreteInclusiveAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeStructAction(ULearningAgentsActionObject* Object, const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Tag = TEXT("StructAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeStructActionFromArrays(ULearningAgentsActionObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Tag = TEXT("StructAction"));
	static FLearningAgentsActionObjectElement MakeStructActionFromArrayViews(ULearningAgentsActionObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Tag = TEXT("StructAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeExclusiveUnionAction(ULearningAgentsActionObject* Object, const FName ElementName, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("ExclusiveUnionAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeInclusiveUnionAction(ULearningAgentsActionObject* Object, const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Tag = TEXT("InclusiveUnionAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeInclusiveUnionActionFromArrays(ULearningAgentsActionObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Tag = TEXT("InclusiveUnionAction"));
	static FLearningAgentsActionObjectElement MakeInclusiveUnionActionFromArrayViews(ULearningAgentsActionObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Tag = TEXT("InclusiveUnionAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeStaticArrayAction(ULearningAgentsActionObject* Object, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Tag = TEXT("StaticArrayAction"));
	static FLearningAgentsActionObjectElement MakeStaticArrayActionFromArrayView(ULearningAgentsActionObject* Object, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Tag = TEXT("StaticArrayAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakePairAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Key, const FLearningAgentsActionObjectElement Value, const FName Tag = TEXT("PairAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeEnumAction(ULearningAgentsActionObject* Object, const UEnum* Enum, const uint8 EnumValue, const FName Tag = TEXT("EnumAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeBitmaskAction(ULearningAgentsActionObject* Object, const UEnum* Enum, const int32 BitmaskValue, const FName Tag = TEXT("BitmaskAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeOptionalAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const ELearningAgentsOptionalAction Option, const FName Tag = TEXT("OptionalAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 1))
	static FLearningAgentsActionObjectElement MakeOptionalNullAction(ULearningAgentsActionObject* Object, const FName Tag = TEXT("OptionalAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeOptionalValidAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("OptionalAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeEitherAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const ELearningAgentsEitherAction Either, const FName Tag = TEXT("EitherAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, DisplayName = "Make Either A Action"))
	static FLearningAgentsActionObjectElement MakeEitherAAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement A, const FName Tag = TEXT("EitherAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2, DisplayName = "Make Either B Action"))
	static FLearningAgentsActionObjectElement MakeEitherBAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement B, const FName Tag = TEXT("EitherAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeEncodingAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("EncodingAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 2))
	static FLearningAgentsActionObjectElement MakeBoolAction(ULearningAgentsActionObject* Object, const bool bValue, const FName Tag = TEXT("BoolAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeFloatAction(ULearningAgentsActionObject* Object, const float Value, const float FloatScale = 1.0f, const FName Tag = TEXT("FloatAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsActionObjectElement MakeLocationAction(ULearningAgentsActionObject* Object, const FVector Location, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Tag = TEXT("LocationAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsActionObjectElement MakeRotationAction(ULearningAgentsActionObject* Object, const FRotator Rotation, const FRotator RelativeRotation = FRotator::ZeroRotator, const float RotationScale = 90.0f, const FName Tag = TEXT("RotationAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsActionObjectElement MakeRotationActionFromQuat(ULearningAgentsActionObject* Object, const FQuat Rotation, const FQuat RelativeRotation, const float RotationScale = 90.0f, const FName Tag = TEXT("RotationAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeScaleAction(ULearningAgentsActionObject* Object, const FVector Scale, const FVector RelativeScale = FVector(1, 1, 1), const FName Tag = TEXT("ScaleAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsActionObjectElement MakeTransformAction(ULearningAgentsActionObject* Object, const FTransform Transform, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Tag = TEXT("TransformAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsActionObjectElement MakeAngleAction(ULearningAgentsActionObject* Object, const float Angle, const float RelativeAngle = 0.0f, const float AngleScale = 90.0f, const FName Tag = TEXT("AngleAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsActionObjectElement MakeAngleActionRadians(ULearningAgentsActionObject* Object, const float Angle, const float RelativeAngle = 0.0f, const float AngleScale = 1.57079632679f, const FName Tag = TEXT("AngleAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	static FLearningAgentsActionObjectElement MakeVelocityAction(ULearningAgentsActionObject* Object, const FVector Velocity, const FTransform RelativeTransform = FTransform(), const float VelocityScale = 200.0f, const FName Tag = TEXT("VelocityAction"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	static FLearningAgentsActionObjectElement MakeDirectionAction(ULearningAgentsActionObject* Object, const FVector Direction, const FTransform RelativeTransform = FTransform(), const FName Tag = TEXT("DirectionAction"));

public:

	/**
	 * Get a null action.
	 *
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 2, ReturnDisplayName = "Success"))
	static bool GetNullAction(const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("NullAction"));

	/**
	 * Get the number of values in a continuous action.
	 *
	 * @param OutNum The output number of values.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetContinuousActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("ContinuousAction"));

	/**
	 * Get the values of a continuous action.
	 *
	 * @param OutValues The output values.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetContinuousAction(
		TArray<float>& OutValues, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FName Tag = TEXT("ContinuousAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);
	
	/**
	 * Get the values of a continuous action. The OutValues ArrayView must be the correct size.
	 *
	 * @param OutValues The output values.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	static bool GetContinuousActionToArrayView(
		TArrayView<float> OutValues, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FName Tag = TEXT("ContinuousAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the index for an exclusive discrete action.
	 *
	 * @param OutIndex The output index.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetExclusiveDiscreteAction(
		int32& OutIndex, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FName Tag = TEXT("DiscreteExclusiveAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the number of indices for an inclusive discrete action.
	 *
	 * @param OutNum The output number of indices.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetInclusiveDiscreteActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("DiscreteInclusiveAction"));

	/**
	 * Get the indices for an inclusive discrete action.
	 *
	 * @param OutIndices The output indices.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetInclusiveDiscreteAction(
		TArray<int32>& OutIndices, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FName Tag = TEXT("DiscreteInclusiveAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);
	
	/**
	 * Get the indices for an inclusive discrete action. The OutIndices ArrayView must be the correct size.
	 *
	 * @param OutIndices The output indices.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	static bool GetInclusiveDiscreteActionToArrayView(
		TArrayView<int32> OutIndices, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FName Tag = TEXT("DiscreteInclusiveAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the number of sub-actions for a struct action.
	 *
	 * @param OutNum The output number of sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStructActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("StructAction"));

	/**
	 * Get the sub-actions for a struct action.
	 *
	 * @param OutElements The output sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStructAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("StructAction"));

	/**
	 * Get the sub-actions for a struct action.
	 *
	 * @param OutElementNames The output sub-action names.
	 * @param OutElements The output sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetStructActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("StructAction"));
	
	/**
	 * Get the sub-actions for a struct action. The OutElementNames and OutElements ArrayViews must be the correct size.
	 *
	 * @param OutElementNames The output sub-action names.
	 * @param OutElements The output sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	static bool GetStructActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("StructAction"));

	/**
	 * Get the chosen sub-action for an exclusive union action.
	 *
	 * @param OutElement The output sub-action.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetExclusiveUnionAction(FName& OutElementName, FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("ExclusiveUnionAction"));

	/**
	 * Get the number of sub-actions for an inclusive union action.
	 *
	 * @param OutNum The output number of sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetInclusiveUnionActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("InclusiveUnionAction"));

	/**
	 * Get the chosen sub-actions for an inclusive union action.
	 *
	 * @param OutElements The output sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetInclusiveUnionAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("InclusiveUnionAction"));

	/**
	 * Get the chosen sub-actions for an inclusive union action.
	 *
	 * @param OutElementNames The output sub-action names.
	 * @param OutElements The output sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetInclusiveUnionActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("InclusiveUnionAction"));
	
	/**
	 * Get the chosen sub-actions for an inclusive union action. The OutElementNames and OutElements ArrayViews must be the correct size.
	 *
	 * @param OutElementNames The output sub-action names.
	 * @param OutElements The output sub-actions.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	static bool GetInclusiveUnionActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("InclusiveUnionAction"));

	/**
	 * Get the number of entries in a static array action.
	 *
	 * @param OutNum The output number of entries.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStaticArrayActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("StaticArrayAction"));

	/**
	 * Get the entries of a static array action.
	 *
	 * @param OutElements The output sub-elements.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetStaticArrayAction(TArray<FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("StaticArrayAction"));
	
	/**
	 * Get the entries of a static array action. The OutElements ArrayView must be the correct size.
	 *
	 * @param OutElements The output sub-elements.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	static bool GetStaticArrayActionToArrayView(TArrayView<FLearningAgentsActionObjectElement> OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("StaticArrayAction"));

	/**
	 * Get the sub-actions of a pair action.
	 *
	 * @param OutKey The output key sub-element.
	 * @param OutValue The output value sub-element.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success"))
	static bool GetPairAction(FLearningAgentsActionObjectElement& OutKey, FLearningAgentsActionObjectElement& OutValue, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("PairAction"));

	/**
	 * Get the enum value of an enum action.
	 *
	 * @param OutEnumValue The output enum value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Enum The Enum
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetEnumAction(
		uint8& OutEnumValue, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const UEnum* Enum, 
		const FName Tag = TEXT("EnumAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the bitmask value of a bitmask action.
	 *
	 * @param OutBitmaskValue The output bitmask value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Enum The Enum
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetBitmaskAction(
		int32& OutBitmaskValue, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const UEnum* Enum, 
		const FName Tag = TEXT("BitmaskAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the sub-action of an option action.
	 *
	 * @param OutOption The output optional specifier.
	 * @param OutElement The output sub-action.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ExpandEnumAsExecs = "OutOption", ReturnDisplayName = "Success"))
	static bool GetOptionalAction(ELearningAgentsOptionalAction& OutOption, FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("OptionalAction"));

	/**
	 * Get the sub-action of an either action.
	 *
	 * @param OutEither The output either specifier.
	 * @param OutElement The output sub-action.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ExpandEnumAsExecs = "OutEither", ReturnDisplayName = "Success"))
	static bool GetEitherAction(ELearningAgentsEitherAction& OutEither, FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("EitherAction"));

	/**
	 * Get the sub-action of an encoding action.
	 *
	 * @param OutElement The output sub-action.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success"))
	static bool GetEncodingAction(FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag = TEXT("EncodingAction"));

	/**
	 * Get the value for a bool action.
	 *
	 * @param bOutValue The output bool value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 3, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetBoolAction(
		bool& bOutValue, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FName Tag = TEXT("BoolAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a float action.
	 *
	 * @param OutValue The output float value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param FloatScale The scale used to control the overall magnitude of the outputted float action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetFloatAction(
		float& OutValue, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const float FloatScale = 1.0f, 
		const FName Tag = TEXT("FloatAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a location action.
	 *
	 * @param OutLocation The output location value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeTransform The relative transform to transform the location by.
	 * @param LocationScale The scale used to control the overall magnitude of the outputted location action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetLocationAction(
		FVector& OutLocation, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FTransform RelativeTransform = FTransform(), 
		const float LocationScale = 100.0f, 
		const FName Tag = TEXT("LocationAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a rotation action.
	 *
	 * @param OutRotation The output rotation value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeRotation The relative rotation to transform the rotation by.
	 * @param RotationScale The scale used to control the overall magnitude of the outputted rotation action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerRotationLocation A location for the visual logger to display the rotation in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetRotationAction(
		FRotator& OutRotation, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FRotator RelativeRotation = FRotator::ZeroRotator, 
		const float RotationScale = 90.0f, 
		const FName Tag = TEXT("RotationAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerRotationLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a rotation action as a quaternion.
	 *
	 * @param OutRotation The output rotation value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeRotation The relative rotation to transform the rotation by.
	 * @param RotationScale The scale used to control the overall magnitude of the outputted rotation action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerRotationLocation A location for the visual logger to display the rotation in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetRotationActionAsQuat(
		FQuat& OutRotation, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FQuat RelativeRotation, 
		const float RotationScale = 90.0f, 
		const FName Tag = TEXT("RotationAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerRotationLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a scale action.
	 *
	 * @param OutScale The output scale value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeScale The relative scale to transform the scale by.
	 * @param Scale The scale used to control the overall magnitude of the outputted scale action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetScaleAction(
		FVector& OutScale, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FVector RelativeScale = FVector(1, 1, 1), 
		const float Scale = 1.0f, 
		const FName Tag = TEXT("ScaleAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a transform action.
	 *
	 * @param OutTransform The output transform value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeTransform The relative transform.
	 * @param LocationScale The scale used to control the overall magnitude of the outputted location action.
	 * @param RotationScale The scale used to control the overall magnitude of the outputted rotation action.
	 * @param ScaleScale The scale used to control the overall magnitude of the outputted scale action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 7, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetTransformAction(
		FTransform& OutTransform, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FTransform RelativeTransform = FTransform(), 
		const float LocationScale = 100.0f, 
		const float RotationScale = 1.0f, 
		const float ScaleScale = 1.0f, 
		const FName Tag = TEXT("TransformAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for an angle action. Returned angle is in degrees.
	 *
	 * @param OutAngle The output angle value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeAngle The relative angle to transform the angle by.
	 * @param AngleScale The scale used to control the overall magnitude of the outputted scale action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerAngleLocation A location for the visual logger to display the angle in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetAngleAction(
		float& OutAngle, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const float RelativeAngle = 0.0f, 
		const float AngleScale = 90.0f, 
		const FName Tag = TEXT("AngleAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerAngleLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for an angle action. Returned angle is in radians.
	 *
	 * @param OutAngle The output angle value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeAngle The relative angle to transform the angle by.
	 * @param AngleScale The scale used to control the overall magnitude of the outputted scale action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerAngleLocation A location for the visual logger to display the angle in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetAngleActionRadians(
		float& OutAngle, 
		const ULearningAgentsActionObject* Object,
		const FLearningAgentsActionObjectElement Element, 
		const float RelativeAngle = 0.0f,
		const float AngleScale = 1.57079632679f, 
		const FName Tag = TEXT("AngleAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerAngleLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a velocity action.
	 *
	 * @param OutVelocity The output velocity value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeTransform The relative transform to transform the velocity by.
	 * @param VelocityScale The scale used to control the overall magnitude of the outputted velocity action.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerVelocityLocation A location for the visual logger to display the velocity in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 5, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetVelocityAction(
		FVector& OutVelocity, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FTransform RelativeTransform = FTransform(), 
		const float VelocityScale = 200.0f, 
		const FName Tag = TEXT("VelocityAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerVelocityLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);

	/**
	 * Get the value for a direction action.
	 *
	 * @param OutDirection The output direction value.
	 * @param Object The Action Object
	 * @param Element The Action Object Element
	 * @param RelativeTransform The relative transform to transform the direction by.
	 * @param Tag The tag of the corresponding action. Must match the tag given during Specify.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this action. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this action.
	 * @param VisualLoggerDirectionLocation A location for the visual logger to display the direction in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerArrowLength The length of the arrow to display to represent the direction.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return true if the provided Element is the correct type, otherwise false.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AdvancedDisplay = 4, ReturnDisplayName = "Success", DefaultToSelf = "VisualLoggerListener"))
	static bool GetDirectionAction(
		FVector& OutDirection, 
		const ULearningAgentsActionObject* Object, 
		const FLearningAgentsActionObjectElement Element, 
		const FTransform RelativeTransform = FTransform(),
		const FName Tag = TEXT("DirectionAction"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerDirectionLocation = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const float VisualLoggerArrowLength = 100.0f,
		const FLinearColor VisualLoggerColor = FLinearColor::Blue);
};




