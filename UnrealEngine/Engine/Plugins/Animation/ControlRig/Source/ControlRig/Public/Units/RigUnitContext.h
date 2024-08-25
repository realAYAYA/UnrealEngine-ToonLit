// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rigs/RigHierarchyContainer.h"
#include "Rigs/RigCurveContainer.h"
#include "AnimationDataSource.h"
#include "Animation/AttributesRuntime.h"
#include "ControlRigAssetUserData.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigUnitContext.generated.h"

class UControlRig;
class UControlRigShapeLibrary;
struct FRigModuleInstance;

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

UENUM(BlueprintType, meta = (RigVMTypeAllowed))
enum class ERigMetaDataNameSpace : uint8
{
	// Use no namespace - store the metadata directly on the item
	None,
	// Store the metadata for item relative to its module
	Self,
	// Store the metadata relative to its parent model
	Parent,
	// Store the metadata under the root module
	Root,
	Last UMETA(Hidden)
};

USTRUCT()
struct CONTROLRIG_API FRigHierarchySettings
{
	GENERATED_BODY();

	FRigHierarchySettings()
		: ProceduralElementLimit(2000)
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

	/** Acceptable subset of connection matches */
	FModularRigResolveResult ConnectionResolve;

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

public:
	
	FControlRigExecuteContext()
		: FRigVMExecuteContext()
		, Hierarchy(nullptr)
		, RigModuleNameSpace()
		, RigModuleNameSpaceHash(0)
	{
	}

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FControlRigExecuteContext* OtherContext = (const FControlRigExecuteContext*)InOtherContext; 
		Hierarchy = OtherContext->Hierarchy;
	}

	/**
     * Finds a name spaced user data object
     */
	const UNameSpacedUserData* FindUserData(const FString& InNameSpace) const
	{
		// walk in reverse since asset user data at the end with the same
		// namespace overrides previously listed user data
		for(int32 Index = AssetUserData.Num() - 1; AssetUserData.IsValidIndex(Index); Index--)
		{
			if(!IsValid(AssetUserData[Index]))
			{
				continue;
			}
			if(const UNameSpacedUserData* NameSpacedUserData = Cast<UNameSpacedUserData>(AssetUserData[Index]))
			{
				if(NameSpacedUserData->NameSpace.Equals(InNameSpace, ESearchCase::CaseSensitive))
				{
					return NameSpacedUserData;
				}
			}
		}
		return nullptr;
	}

	/**
	 * Add the namespace from a given name
	 */
	FName AddRigModuleNameSpace(const FName& InName) const;
	FString AddRigModuleNameSpace(const FString& InName) const;

	/**
	 * Remove the namespace from a given name
	 */
	FName RemoveRigModuleNameSpace(const FName& InName) const;
	FString RemoveRigModuleNameSpace(const FString& InName) const;

	/**
	 * Returns true if this context is used on a module currently
	 */
	bool IsRigModule() const
	{
		return !GetRigModuleNameSpace().IsEmpty();
	}

	/**
	 * Returns the namespace of the currently running rig module
	 */
	FString GetRigModuleNameSpace() const
	{
		return RigModuleNameSpace;
	}

	/**
	 * Returns the namespace given a namespace type
	 */
	FString GetElementNameSpace(ERigMetaDataNameSpace InNameSpaceType) const;
	
	/**
	 * Returns the module this unit is running inside of (or nullptr)
	 */
	const FRigModuleInstance* GetRigModuleInstance() const
	{
		return RigModuleInstance;
	}
	
	/**
	 * Returns the module this unit is running inside of (or nullptr)
	 */
	const FRigModuleInstance* GetRigModuleInstance(ERigMetaDataNameSpace InNameSpaceType) const;

	/**
	 * Adapts a metadata name according to rig module namespace.
	 */
	FName AdaptMetadataName(ERigMetaDataNameSpace InNameSpaceType, const FName& InMetadataName) const;

	/** The list of available asset user data object */
	TArray<const UAssetUserData*> AssetUserData;

	DECLARE_DELEGATE_FourParams(FOnAddShapeLibrary, const FControlRigExecuteContext* InContext, const FString&, UControlRigShapeLibrary*, bool /* log results */);
	FOnAddShapeLibrary OnAddShapeLibraryDelegate;

	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnShapeExists, const FName&);
	FOnShapeExists OnShapeExistsDelegate;
	
	FRigUnitContext UnitContext;
	URigHierarchy* Hierarchy;

#if WITH_EDITOR
	virtual void Report(const FRigVMLogSettings& InLogSettings, const FName& InFunctionName, int32 InInstructionIndex, const FString& InMessage) const override
	{
		FString Prefix = GetRigModuleNameSpace();
		if (!Prefix.IsEmpty())
		{
			const FString Name = FString::Printf(TEXT("%s %s"), *Prefix, *InFunctionName.ToString());
			FRigVMExecuteContext::Report(InLogSettings, *Name, InInstructionIndex, InMessage);
		}
		else
		{
			FRigVMExecuteContext::Report(InLogSettings, InFunctionName, InInstructionIndex, InMessage);
		}
	}
#endif

private:
	FString RigModuleNameSpace;
	uint32 RigModuleNameSpaceHash;
	const FRigModuleInstance* RigModuleInstance;

	friend class FControlRigExecuteContextRigModuleGuard;
	friend class UModularRig;
};

class CONTROLRIG_API FControlRigExecuteContextRigModuleGuard
{
public:
	FControlRigExecuteContextRigModuleGuard(FControlRigExecuteContext& InContext, const UControlRig* InControlRig);
	FControlRigExecuteContextRigModuleGuard(FControlRigExecuteContext& InContext, const FString& InNewModuleNameSpace);
	~FControlRigExecuteContextRigModuleGuard();

private:

	FControlRigExecuteContext& Context;
	FString PreviousRigModuleNameSpace;
	uint32 PreviousRigModuleNameSpaceHash;
};

#if WITH_EDITOR
#define UE_CONTROLRIG_RIGUNIT_REPORT(Severity, Format, ...) \
ExecuteContext.Report(EMessageSeverity::Severity, ExecuteContext.GetFunctionName(), ExecuteContext.GetInstructionIndex(), FString::Printf((Format), ##__VA_ARGS__)); 

#define UE_CONTROLRIG_RIGUNIT_LOG_MESSAGE(Format, ...) UE_CONTROLRIG_RIGUNIT_REPORT(Info, (Format), ##__VA_ARGS__)
#define UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(Format, ...) UE_CONTROLRIG_RIGUNIT_REPORT(Warning, (Format), ##__VA_ARGS__)
#define UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(Format, ...) UE_CONTROLRIG_RIGUNIT_REPORT(Error, (Format), ##__VA_ARGS__)
#else
#define UE_CONTROLRIG_RIGUNIT_REPORT(Severity, Format, ...)
#define UE_CONTROLRIG_RIGUNIT_LOG_MESSAGE(Format, ...)
#define UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(Format, ...)
#define UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(Format, ...)
#endif
