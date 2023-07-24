// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rigs/RigHierarchyContainer.h"
#include "Rigs/RigCurveContainer.h"
#include "AnimationDataSource.h"
#include "Animation/AttributesRuntime.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigUnitContext.generated.h"

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
		, DataSourceRegistry(nullptr)
		, InteractionType((uint8)EControlRigInteractionType::None)
		, ElementsBeingInteracted()
	{
	}

	/** An external anim attribute container */
	UE::Anim::FStackAttributeContainer* AnimAttributeContainer;
	
	/** The registry to access data source */
	const UAnimationDataSourceRegistry* DataSourceRegistry;

	/** The current hierarchy settings */
	FRigHierarchySettings HierarchySettings;

	/** The type of interaction currently happening on the rig (0 == None) */
	uint8 InteractionType;

	/** The elements being interacted with. */
	TArray<FRigElementKey> ElementsBeingInteracted;

	/**
	 * Returns a given data source and cast it to the expected class.
	 *
	 * @param InName The name of the data source to look up.
	 * @return The requested data source
	 */
	template<class T>
	T* RequestDataSource(const FName& InName) const
	{
		if (DataSourceRegistry == nullptr)
		{
			return nullptr;
		}
		return DataSourceRegistry->RequestSource<T>(InName);
	}

	/**
	 * Returns true if this context is currently being interacted on
	 */
	bool IsInteracting() const
	{
		return InteractionType != (uint8)EControlRigInteractionType::None;
	}
};

USTRUCT(BlueprintType)
struct FControlRigExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()

	FControlRigExecuteContext()
		: FRigVMExecuteContext()
		, Hierarchy(nullptr)
	{
	}

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FControlRigExecuteContext* OtherContext = (const FControlRigExecuteContext*)InOtherContext; 
		Hierarchy = OtherContext->Hierarchy;
	}

	FRigUnitContext UnitContext;
	URigHierarchy* Hierarchy;
};

#if WITH_EDITOR
#define UE_CONTROLRIG_RIGUNIT_REPORT(Severity, Format, ...) \
if(ExecuteContext.GetLog() != nullptr) \
{ \
	ExecuteContext.GetLog()->Report(EMessageSeverity::Severity, ExecuteContext.GetFunctionName(), ExecuteContext.GetInstructionIndex(), FString::Printf((Format), ##__VA_ARGS__)); \
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
