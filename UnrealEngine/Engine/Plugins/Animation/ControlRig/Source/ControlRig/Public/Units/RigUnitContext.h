// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rigs/RigHierarchyContainer.h"
#include "Rigs/RigCurveContainer.h"
#include "Rigs/RigNameCache.h"
#include "ControlRigLog.h"
#include "AnimationDataSource.h"
#include "Animation/AttributesRuntime.h"
#include "Drawing/ControlRigDrawInterface.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "RigUnitContext.generated.h"

/** Current state of rig
*	What  state Control Rig currently is
*/
UENUM()
enum class EControlRigState : uint8
{
	Init,
	Update,
	Invalid,
};

/**
 * The type of interaction happening on a rig
 */
UENUM()
enum class EControlRigInteractionType : uint8
{
	None = 0,
	Translate = (1 << 0),
	Rotate = (1 << 1),
	Scale = (1 << 2),
	All = Translate | Rotate | Scale
};

USTRUCT()
struct CONTROLRIG_API FRigHierarchySettings
{
	GENERATED_BODY();

	FRigHierarchySettings()
		: ProceduralElementLimit(128)
	{
	}

	// Sets the limit for the number of elements to create procedurally
	UPROPERTY(EditAnywhere, Category = "Hierarchy Settings")
	int32 ProceduralElementLimit;
};

/** Execution context that rig units use */
struct FRigUnitContext
{
	/** default constructor */
	FRigUnitContext()
		: AnimAttributeContainer(nullptr)
		, DrawInterface(nullptr)
		, DrawContainer(nullptr)
		, DataSourceRegistry(nullptr)
		, DeltaTime(0.f)
		, AbsoluteTime(0.f)
		, State(EControlRigState::Invalid)
		, Hierarchy(nullptr)
		, InteractionType((uint8)EControlRigInteractionType::None)
		, ElementsBeingInteracted()
		, ToWorldSpaceTransform(FTransform::Identity)
		, OwningComponent(nullptr)
		, OwningActor(nullptr)
		, World(nullptr)
#if WITH_EDITOR
		, Log(nullptr)
#endif
		, NameCache(nullptr)
	{
	}

	/** An external anim attribute container */
	UE::Anim::FStackAttributeContainer* AnimAttributeContainer;
	
	/** The draw interface for the units to use */
	FControlRigDrawInterface* DrawInterface;

	/** The draw container for the units to use */
	FControlRigDrawContainer* DrawContainer;

	/** The registry to access data source */
	const UAnimationDataSourceRegistry* DataSourceRegistry;

	/** The current delta time */
	float DeltaTime;

	/** The current delta time */
	float AbsoluteTime;

	/** The current frames per second */
	float FramesPerSecond;

	/** Current execution context */
	EControlRigState State;

	/** The current hierarchy being executed */
	URigHierarchy* Hierarchy;

	/** The current hierarchy settings */
	FRigHierarchySettings HierarchySettings;

	/** The type of interaction currently happening on the rig (0 == None) */
	uint8 InteractionType;

	/** The elements being interacted with. */
	TArray<FRigElementKey> ElementsBeingInteracted;

	/** The current transform going from rig (global) space to world space */
	FTransform ToWorldSpaceTransform;

	/** The current component this rig is owned by */
	USceneComponent* OwningComponent;

	/** The current actor this rig is owned by */
	const AActor* OwningActor;

	/** The world this rig is running in */
	const UWorld* World;

#if WITH_EDITOR
	/** A handle to the compiler log */
	FControlRigLog* Log;
#endif

	/** A container to store all names */
	FRigNameCache* NameCache;

	/**
	 * Returns a given data source and cast it to the expected class.
	 *
	 * @param InName The name of the data source to look up.
	 * @return The requested data source
	 */
	template<class T>
	FORCEINLINE T* RequestDataSource(const FName& InName) const
	{
		if (DataSourceRegistry == nullptr)
		{
			return nullptr;
		}
		return DataSourceRegistry->RequestSource<T>(InName);
	}

	/**
	 * Converts a transform from rig (global) space to world space
	 */
	FORCEINLINE FTransform ToWorldSpace(const FTransform& InTransform) const
	{
		return InTransform * ToWorldSpaceTransform;
	}

	/**
	 * Converts a transform from world space to rig (global) space
	 */
	FORCEINLINE FTransform ToRigSpace(const FTransform& InTransform) const
	{
		return InTransform.GetRelativeTransform(ToWorldSpaceTransform);
	}

	/**
	 * Converts a location from rig (global) space to world space
	 */
	FORCEINLINE FVector ToWorldSpace(const FVector& InLocation) const
	{
		return ToWorldSpaceTransform.TransformPosition(InLocation);
	}

	/**
	 * Converts a location from world space to rig (global) space
	 */
	FORCEINLINE FVector ToRigSpace(const FVector& InLocation) const
	{
		return ToWorldSpaceTransform.InverseTransformPosition(InLocation);
	}

	/**
	 * Converts a rotation from rig (global) space to world space
	 */
	FORCEINLINE FQuat ToWorldSpace(const FQuat& InRotation) const
	{
		return ToWorldSpaceTransform.TransformRotation(InRotation);
	}

	/**
	 * Converts a rotation from world space to rig (global) space
	 */
	FORCEINLINE FQuat ToRigSpace(const FQuat& InRotation) const
	{
		return ToWorldSpaceTransform.InverseTransformRotation(InRotation);
	}

	/**
	 * Returns true if this context is currently being interacted on
	 */
	FORCEINLINE bool IsInteracting() const
	{
		return InteractionType != (uint8)EControlRigInteractionType::None;
	}
};

#if WITH_EDITOR
#define UE_CONTROLRIG_RIGUNIT_REPORT(Severity, Format, ...) \
if(Context.Log != nullptr) \
{ \
	Context.Log->Report(EMessageSeverity::Severity, RigVMExecuteContext.GetFunctionName(), RigVMExecuteContext.GetInstructionIndex(), FString::Printf((Format), ##__VA_ARGS__)); \
}
#define UE_CONTROLRIG_RIGUNIT_LOG_MESSAGE(Format, ...) UE_CONTROLRIG_RIGUNIT_REPORT(Info, (Format), ##__VA_ARGS__)
#define UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(Format, ...) UE_CONTROLRIG_RIGUNIT_REPORT(Warning, (Format), ##__VA_ARGS__)
#define UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(Format, ...) UE_CONTROLRIG_RIGUNIT_REPORT(Error, (Format), ##__VA_ARGS__)
#else
#define UE_CONTROLRIG_RIGUNIT_REPORT(Severity, Format, ...)
#define UE_CONTROLRIG_RIGUNIT_LOG_MESSAGE(Format, ...)
#define UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(Format, ...)
#define UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(Format, ...)
#endif
