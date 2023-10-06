// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningAgentsDebug.h"

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "LearningAgentsObservations.generated.h"

namespace UE::Learning
{
	struct FFeatureObject;
	struct FFloatFeature;
	struct FTimeFeature;
	struct FAngleFeature;
	struct FRotationFeature;
	struct FDirectionFeature;
	struct FPlanarDirectionFeature;
	struct FPositionFeature;
	struct FScalarPositionFeature;
	struct FPlanarPositionFeature;
	struct FVelocityFeature;
	struct FScalarVelocityFeature;
	struct FPlanarVelocityFeature;
	struct FAngularVelocityFeature;
	struct FScalarAngularVelocityFeature;
}

class ULearningAgentsInteractor;

// For functions in this file, we are favoring having more verbose names such as "AddFloatObservation" vs simply "Add" in 
// order to keep it easy to find the correct function in blueprints.

//------------------------------------------------------------------

/** The base class for all observations. Observations define the inputs to your agents. */
UCLASS(Abstract, BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsObservation : public UObject
{
	GENERATED_BODY()

public:

	/** Reference to the Interactor this observation is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;

public:

	/** Initialize the internal state for a given maximum number of agents */
	void Init(const int32 MaxAgentNum);

	/**
	 * Called whenever agents are added to the associated ULearningAgentsInteractor object.
	 * @param AgentIds Array of agent ids which have been added
	 */
	virtual void OnAgentsAdded(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are removed from the associated ULearningAgentsInteractor object.
	 * @param AgentIds Array of agent ids which have been removed
	 */
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are reset on the associated ULearningAgentsInteractor object.
	 * @param AgentIds Array of agent ids which have been reset
	 */
	virtual void OnAgentsReset(const TArray<int32>& AgentIds);

	/** Get the number of times an observation has been set for the given agent id. */
	uint64 GetAgentIteration(const int32 AgentId) const;

public:
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG

	/** Color used to draw this observation in the visual log */
	FLinearColor VisualLogColor = FColor::Red;

	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const {}
#endif

protected:

	/** Number of times this observation has been set for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> AgentIteration;
};

//------------------------------------------------------------------

/** A simple float observation. Used as a catch-all for situations where a more type-specific observation does not exist yet. */
UCLASS()
class LEARNINGAGENTS_API UFloatObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new float observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta=(DefaultToSelf = "InInteractor"))
	static UFloatObservation* AddFloatObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Observation The value currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetFloatObservation(const int32 AgentId, const float Observation);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

/** A simple array of floats observation. Used as a catch-all for situations where a more type-specific observation does not exist yet. */
UCLASS()
class LEARNINGAGENTS_API UFloatArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new float array observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Num The number of floats in the array
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UFloatArrayObservation* AddFloatArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 Num = 1, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Observation The value currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetFloatArrayObservation(const int32 AgentId, const TArray<float>& Observation);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

//------------------------------------------------------------------

/** A simple observation for an FVector. */
UCLASS()
class LEARNINGAGENTS_API UVectorObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new vector observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UVectorObservation* AddVectorObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Observation The values currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetVectorObservation(const int32 AgentId, const FVector Observation);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

/** A simple observation for an array of FVector. */
UCLASS()
class LEARNINGAGENTS_API UVectorArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new vector array observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Num The number of vectors in the array
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UVectorArrayObservation* AddVectorArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 Num = 1, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Observation The values currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetVectorArrayObservation(const int32 AgentId, const TArray<FVector>& Observation);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

//------------------------------------------------------------------

/** An observation of an enumeration. */
UCLASS()
class LEARNINGAGENTS_API UEnumObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new enum observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param EnumType The type of enum to use
	 * @param Name The name of this new observation. Used for debugging.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UEnumObservation* AddEnumObservation(ULearningAgentsInteractor* InInteractor, const UEnum* EnumType, const FName Name = NAME_None);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Value The enum value currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetEnumObservation(const int32 AgentId, const uint8 Value);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	const UEnum* Enum = nullptr;
	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

/** An observation of an array of enumerations. */
UCLASS()
class LEARNINGAGENTS_API UEnumArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new enum array observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param EnumType The type of enum to use
	 * @param Name The name of this new observation. Used for debugging.
	 * @param EnumNum The number of enum observations in the array
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UEnumArrayObservation* AddEnumArrayObservation(ULearningAgentsInteractor* InInteractor, const UEnum* EnumType, const FName Name = NAME_None, const int32 EnumNum = 1);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Values The enum values currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetEnumArrayObservation(const int32 AgentId, const TArray<uint8>& Values);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	const UEnum* Enum = nullptr;
	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

//------------------------------------------------------------------

/** An observation of a time relative to another time. */
UCLASS()
class LEARNINGAGENTS_API UTimeObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new time observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UTimeObservation* AddTimeObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Time The time currently being observed.
	 * @param RelativeTime The time the provided time should be encoded relative to.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetTimeObservation(const int32 AgentId, const float Time, const float RelativeTime = 0.0f);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FTimeFeature> FeatureObject;
};

/** An observation of an array of times. */
UCLASS()
class LEARNINGAGENTS_API UTimeArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new angle observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param TimeNum The number of times in the array
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UTimeArrayObservation* AddTimeArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 TimeNum = 1, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Time The times currently being observed.
	 * @param RelativeTime The time the provided time should be encoded relative to.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetTimeArrayObservation(const int32 AgentId, const TArray<float>& Times, const float RelativeTime = 0.0f);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FTimeFeature> FeatureObject;
};

//------------------------------------------------------------------

/** An observation of an angle relative to another angle. */
UCLASS()
class LEARNINGAGENTS_API UAngleObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new angle observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation. Angle observations are encoded as directions. 
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UAngleObservation* AddAngleObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Angle The angle currently being observed.
	 * @param RelativeAngle The frame of reference angle.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetAngleObservation(const int32 AgentId, const float Angle, const float RelativeAngle = 0.0f);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FAngleFeature> FeatureObject;
};

/** An observation of an array of angles. */
UCLASS()
class LEARNINGAGENTS_API UAngleArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new angle observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param AngleNum The number of angles in the array
	 * @param Scale Used to normalize the data for the observation. Angle observations are encoded as directions. 
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UAngleArrayObservation* AddAngleArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 AngleNum = 1, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Angles The angles currently being observed.
	 * @param RelativeAngle The frame of reference angle.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetAngleArrayObservation(const int32 AgentId, const TArray<float>& Angles, const float RelativeAngle = 0.0f);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FAngleFeature> FeatureObject;
};

/** An observation of a rotation relative to another rotation. */
UCLASS()
class LEARNINGAGENTS_API URotationObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new rotation observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation. Rotation observations are encoded as directions. 
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static URotationObservation* AddRotationObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Rotation The rotation currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetRotationObservation(const int32 AgentId, const FRotator Rotation, const FRotator RelativeRotation = FRotator::ZeroRotator);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Rotation The rotation currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetRotationObservationFromQuat(const int32 AgentId, const FQuat Rotation, const FQuat RelativeRotation);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FRotationFeature> FeatureObject;
};

/** An observation of an array of rotations. */
UCLASS()
class LEARNINGAGENTS_API URotationArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new rotation observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param RotationNum The number of rotations in the array
	 * @param Scale Used to normalize the data for the observation. Rotation observations are encoded as directions. 
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static URotationArrayObservation* AddRotationArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 RotationNum = 1, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Rotations The rotations currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetRotationArrayObservation(const int32 AgentId, const TArray<FRotator>& Rotations, const FRotator RelativeRotation = FRotator::ZeroRotator);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Rotations The rotations currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetRotationArrayObservationFromQuats(const int32 AgentId, const TArray<FQuat>& Rotations, const FQuat RelativeRotation);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FRotationFeature> FeatureObject;
};

//------------------------------------------------------------------

/** An observation of a direction vector. */
UCLASS()
class LEARNINGAGENTS_API UDirectionObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new direction observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UDirectionObservation* AddDirectionObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Direction The direction currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetDirectionObservation(const int32 AgentId, const FVector Direction, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FDirectionFeature> FeatureObject;
};

/** An observation of an array of direction vectors. */
UCLASS()
class LEARNINGAGENTS_API UDirectionArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new direction array observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param DirectionNum The number of directions in the array
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UDirectionArrayObservation* AddDirectionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 DirectionNum = 1, const float Scale = 1.0f);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Directions The directions currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetDirectionArrayObservation(const int32 AgentId, const TArray<FVector>& Directions, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FDirectionFeature> FeatureObject;
};

/** An observation of a direction vector projected onto a plane. */
UCLASS()
class LEARNINGAGENTS_API UPlanarDirectionObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar direction observation to the given agent interactor. The axis parameters define the plane. Call
	 * during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPlanarDirectionObservation* AddPlanarDirectionObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f, const FVector Axis0 = FVector::ForwardVector, const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Direction The direction currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarDirectionObservation(const int32 AgentId, const FVector Direction, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarDirectionFeature> FeatureObject;
};

/** An observation of an array of direction vectors projected onto a plane. */
UCLASS()
class LEARNINGAGENTS_API UPlanarDirectionArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar direction observation to the given agent interactor. The axis parameters define the plane. Call
	 * during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param DirectionNum The number of directions in the array
	 * @param Scale Used to normalize the data for the observation.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPlanarDirectionArrayObservation* AddPlanarDirectionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 DirectionNum = 1, const float Scale = 1.0f, const FVector Axis0 = FVector::ForwardVector, const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Directions The directions currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarDirectionArrayObservation(const int32 AgentId, const TArray<FVector>& Directions, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarDirectionFeature> FeatureObject;
};

//------------------------------------------------------------------

/** An observation of a position vector. */
UCLASS()
class LEARNINGAGENTS_API UPositionObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new position observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPositionObservation* AddPositionObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 100.0f);

	/**
	 * Sets the data for this observation. The relative position & rotation can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position & forward rotation. Call during
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Position The position currently being observed.
	 * @param RelativePosition The vector Position will be offset from.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPositionObservation(const int32 AgentId, const FVector Position, const FVector RelativePosition = FVector::ZeroVector, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPositionFeature> FeatureObject;
};

/** An observation of an array of positions. */
UCLASS()
class LEARNINGAGENTS_API UPositionArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new position array observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param PositionNum The number of positions in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPositionArrayObservation* AddPositionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 PositionNum = 1, const float Scale = 100.0f);

	/**
	 * Sets the data for this observation. The relative position & rotation can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position & forward rotation. Call during
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Positions The positions currently being observed.
	 * @param RelativePosition The vector Positions will be offset from.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPositionArrayObservation(int32 AgentId, const TArray<FVector>& Positions, const FVector RelativePosition = FVector::ZeroVector, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPositionFeature> FeatureObject;
};

/** An observation of a position along a single axis. Can be useful for providing information like object heights. */
UCLASS()
class LEARNINGAGENTS_API UScalarPositionObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new scalar position observation to the given agent interactor. Call during 
	 * ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UScalarPositionObservation* AddScalarPositionObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 100.0f);

	/**
	 * Sets the data for this observation. The relative position can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position. Call during 
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Position The position currently being observed.
	 * @param RelativePosition The vector Position will be offset from.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarPositionObservation(const int32 AgentId, const float Position, const float RelativePosition = 0.0f);

	/**
	 * Sets the data for this observation. The relative position can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position. Call during
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Position The position currently being observed.
	 * @param RelativePosition The vector Position will be offset from.
	 * @param Axis The axis along which to encode the positions
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarPositionObservationWithAxis(const int32 AgentId, const FVector Position, const FVector RelativePosition = FVector::ZeroVector, const FVector Axis = FVector::UpVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FScalarPositionFeature> FeatureObject;
};

/** An observation of an array of positions along a single axis. */
UCLASS()
class LEARNINGAGENTS_API UScalarPositionArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new scalar position array observation to the given agent interactor. Call during 
	 * ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param PositionNum The number of positions in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UScalarPositionArrayObservation* AddScalarPositionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 PositionNum = 1, const float Scale = 100.0f);

	/**
	 * Sets the data for this observation. The relative position can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position. Call during
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Positions The positions currently being observed.
	 * @param RelativePosition The vector Positions will be offset from.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarPositionArrayObservation(const int32 AgentId, const TArray<float>& Positions, const float RelativePosition = 0.0f);

	/**
	 * Sets the data for this observation. The relative position can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position. Call during
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Positions The positions currently being observed.
	 * @param RelativePosition The vector Positions will be offset from.
	 * @param Axis The axis along which to encode the positions
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarPositionArrayObservationWithAxis(const int32 AgentId, const TArray<FVector>& Positions, const FVector RelativePosition = FVector::ZeroVector, const FVector Axis = FVector::UpVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FScalarPositionFeature> FeatureObject;
};

/** An observation of a position projected onto a plane. */
UCLASS()
class LEARNINGAGENTS_API UPlanarPositionObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar position observation to the given agent interactor. The axis parameters define the plane. Call
	 * during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPlanarPositionObservation* AddPlanarPositionObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 100.0f, const FVector Axis0 = FVector::ForwardVector, const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this observation. The relative position & rotation can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position & forward rotation. Call during
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Position The position currently being observed.
	 * @param RelativePosition The vector Position will be offset from.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarPositionObservation(const int32 AgentId, const FVector Position, const FVector RelativePosition = FVector::ZeroVector, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarPositionFeature> FeatureObject;
};

/** An observation of an array of positions projected onto a plane. */
UCLASS()
class LEARNINGAGENTS_API UPlanarPositionArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar position array observation to the given agent interactor. The axis parameters define the plane.
	 * Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param PositionNum The number of positions in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPlanarPositionArrayObservation* AddPlanarPositionArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 PositionNum = 1, const float Scale = 100.0f, const FVector Axis0 = FVector::ForwardVector, const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this observation. The relative position & rotation can be used to make this observation
	 * relative to the agent's perspective, e.g. by passing the agent's position & forward rotation. Call during
	 * ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Positions The positions currently being observed.
	 * @param RelativePosition The vector Positions will be offset from.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarPositionArrayObservation(const int32 AgentId, const TArray<FVector>& Positions, const FVector RelativePosition = FVector::ZeroVector, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarPositionFeature> FeatureObject;
};

//------------------------------------------------------------------

/** An observation of a velocity. */
UCLASS()
class LEARNINGAGENTS_API UVelocityObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new velocity observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UVelocityObservation* AddVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 200.0f);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocity The velocity currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetVelocityObservation(const int32 AgentId, const FVector Velocity, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FVelocityFeature> FeatureObject;
};

/** An observation of an array of velocities. */
UCLASS()
class LEARNINGAGENTS_API UVelocityArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new velocity observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param VelocityNum The number of velocities in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UVelocityArrayObservation* AddVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 VelocityNum = 1, const float Scale = 200.0f);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocities The velocities currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetVelocityArrayObservation(const int32 AgentId, const TArray<FVector>& Velocities, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FVelocityFeature> FeatureObject;
};

/** An observation of a velocity along a single axis. */
UCLASS()
class LEARNINGAGENTS_API UScalarVelocityObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new scalar velocity observation to the given agent interactor. 
	 * Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UScalarVelocityObservation* AddScalarVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 200.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocity The velocity currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarVelocityObservation(const int32 AgentId, const float Velocity);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocity The velocity currently being observed.
	 * @param Axis The axis to encode the velocity along
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarVelocityObservationWithAxis(const int32 AgentId, const FVector Velocity, const FVector Axis = FVector::UpVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FScalarVelocityFeature> FeatureObject;
};

/** An observation of an array of velocities along a single axis. */
UCLASS()
class LEARNINGAGENTS_API UScalarVelocityArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new scalar velocity observation to the given agent interactor.
	 * Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param VelocityNum The number of velocities in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UScalarVelocityArrayObservation* AddScalarVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 VelocityNum = 1, const float Scale = 200.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocities The velocities currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarVelocityArrayObservation(const int32 AgentId, const TArray<float>& Velocities);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocities The velocities currently being observed.
	 * @param Axis The axis to encode the velocity along
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarVelocityArrayObservationWithAxis(const int32 AgentId, const TArray<FVector>& Velocities, const FVector Axis = FVector::UpVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FScalarVelocityFeature> FeatureObject;
};

/** An observation of a velocity projected onto a plane. */
UCLASS()
class LEARNINGAGENTS_API UPlanarVelocityObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar velocity observation to the given agent interactor. The axis parameters define the plane.
	 * Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPlanarVelocityObservation* AddPlanarVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 200.0f, const FVector Axis0 = FVector::ForwardVector, const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocity The velocity currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarVelocityObservation(const int32 AgentId, const FVector Velocity, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarVelocityFeature> FeatureObject;
};

/** An observation of an array of velocities projected onto a plane. */
UCLASS()
class LEARNINGAGENTS_API UPlanarVelocityArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar velocity observation to the given agent interactor. The axis parameters define the plane.
	 * Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param VelocityNum The number of velocities in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPlanarVelocityArrayObservation* AddPlanarVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 VelocityNum = 1, const float Scale = 200.0f, const FVector Axis0 = FVector::ForwardVector, const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocities The velocities currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarVelocityArrayObservation(const int32 AgentId, const TArray<FVector>& Velocities, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarVelocityFeature> FeatureObject;
};

//------------------------------------------------------------------

/** An observation of an angular velocity. */
UCLASS()
class LEARNINGAGENTS_API UAngularVelocityObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new angular velocity observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UAngularVelocityObservation* AddAngularVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 180.0f);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param AngularVelocity The angular velocity currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetAngularVelocityObservation(const int32 AgentId, const FVector AngularVelocity, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FAngularVelocityFeature> FeatureObject;
};

/** An observation of an array of angular velocities. */
UCLASS()
class LEARNINGAGENTS_API UAngularVelocityArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new angular velocity array observation to the given agent interactor. Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param AngularVelocityNum The number of angular velocities in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UAngularVelocityArrayObservation* AddAngularVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 AngularVelocityNum = 1, const float Scale = 180.0f);

	/**
	 * Sets the data for this observation. The relative rotation can be used to make this observation relative to the
	 * agent's perspective, e.g. by passing the agent's forward rotation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocities The angular velocities currently being observed.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetAngularVelocityArrayObservation(const int32 AgentId, const TArray<FVector>& AngularVelocities, const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FAngularVelocityFeature> FeatureObject;
};

/** An observation of a scalar angular velocity. */
UCLASS()
class LEARNINGAGENTS_API UScalarAngularVelocityObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new scalar angular velocity observation to the given agent interactor.
	 * Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UScalarAngularVelocityObservation* AddScalarAngularVelocityObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 180.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param AngularVelocity The angular velocity currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarAngularVelocityObservation(const int32 AgentId, const float AngularVelocity);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param AngularVelocity The angular velocity currently being observed.
	 * @param Axis The axis to encode the angular velocity around
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarAngularVelocityObservationWithAxis(const int32 AgentId, const FVector AngularVelocity, const FVector Axis = FVector::UpVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FScalarAngularVelocityFeature> FeatureObject;
};

/** An observation of an array of scalar angular velocities. */
UCLASS()
class LEARNINGAGENTS_API UScalarAngularVelocityArrayObservation : public ULearningAgentsObservation
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new scalar angular velocity array observation to the given agent interactor.
	 * Call during ULearningAgentsInteractor::SetupObservations event.
	 * @param InInteractor The agent interactor to add this observation to.
	 * @param Name The name of this new observation. Used for debugging.
	 * @param AngularVelocityNum The number of angular velocities in the array.
	 * @param Scale Used to normalize the data for the observation.
	 * @return The newly created observation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UScalarAngularVelocityArrayObservation* AddScalarAngularVelocityArrayObservation(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 AngularVelocityNum = 1, const float Scale = 180.0f);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param AngularVelocities The angular velocities currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarAngularVelocityArrayObservation(const int32 AgentId, const TArray<float>& AngularVelocities);

	/**
	 * Sets the data for this observation. Call during ULearningAgentsInteractor::SetObservations event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param AngularVelocities The angular velocities currently being observed.
	 * @param Axis The axis to encode the angular velocity around
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarAngularVelocityArrayObservationWithAxis(const int32 AgentId, const TArray<FVector>& AngularVelocities, const FVector Axis = FVector::UpVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this observation to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FScalarAngularVelocityFeature> FeatureObject;
};
