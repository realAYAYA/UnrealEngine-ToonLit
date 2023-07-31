// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AssertionMacros.h"
#include "RigVMCore/RigVMTraits.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

#include "RigVMUserWorkflow.generated.h"

class FProperty;
class URigVMUserWorkflowOptions;
struct FFrame;

DECLARE_DELEGATE_ThreeParams(FRigVMReportDelegate, EMessageSeverity::Type, UObject*, const FString&);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRigVMPerformUserWorkflowDelegate, const URigVMUserWorkflowOptions*, UObject*);
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FRigVMPeformUserWorkflowDynamicDelegate, const URigVMUserWorkflowOptions*, InOptions, UObject*, InController);

// Types of workflows offered by a rigvm struct node
UENUM(BlueprintType)
enum class ERigVMUserWorkflowType : uint8
{
	Invalid = 0 UMETA(Hidden),
	NodeContext = 0x001,
	PinContext = 0x002,
	OnPinDefaultChanged  = 0x004,
	All = NodeContext | PinContext | OnPinDefaultChanged
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMUserWorkflow
{
	GENERATED_BODY()

public:

	FORCEINLINE FRigVMUserWorkflow()
	: Type(ERigVMUserWorkflowType::Invalid)
	, OptionsClass(nullptr)
	{}

	FORCEINLINE FRigVMUserWorkflow(
		const FString& InTitle,
		const FString& InTooltip,
		ERigVMUserWorkflowType InType,
		FRigVMPerformUserWorkflowDelegate InGetActionsDelegate,
		UClass* InOptionsClass)
	: Title(InTitle)
	, Tooltip(InTooltip)
	, Type(InType)
	, PerformDelegate(InGetActionsDelegate)
	, OptionsClass(InOptionsClass)
	{}
	
	FORCEINLINE virtual ~FRigVMUserWorkflow() {}

	FORCEINLINE bool IsValid() const
	{
		return Type != ERigVMUserWorkflowType::Invalid &&
			GetOptionsClass() != nullptr &&
			(PerformDelegate.IsBound() || PerformDynamicDelegate.IsBound());
	}

	FORCEINLINE const FString& GetTitle() const { return Title; }
	FORCEINLINE const FString& GetTooltip() const { return Tooltip; }
	FORCEINLINE ERigVMUserWorkflowType GetType() const { return Type; }
	FORCEINLINE UClass* GetOptionsClass() const { return OptionsClass; }

	bool Perform(const URigVMUserWorkflowOptions* InOptions, UObject* InController) const;

protected:

	bool ValidateOptions(const URigVMUserWorkflowOptions* InOptions) const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Workflow, meta = (AllowPrivateAccess = true))
	FString Title;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Workflow, meta = (AllowPrivateAccess = true))
	FString Tooltip;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Workflow, meta = (AllowPrivateAccess = true))
	ERigVMUserWorkflowType Type;

	FRigVMPerformUserWorkflowDelegate PerformDelegate;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Workflow, meta = (ScriptName = "OnPerformWorkflow", AllowPrivateAccess = true))
	FRigVMPeformUserWorkflowDynamicDelegate PerformDynamicDelegate;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Workflow, meta = (AllowPrivateAccess = true))
	TObjectPtr<UClass> OptionsClass;
};

UCLASS(BlueprintType)
class RIGVM_API URigVMUserWorkflowOptions : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category=Options)
	FORCEINLINE bool IsValid() const { return Subject != nullptr; }
	
	UFUNCTION(BlueprintCallable, Category=Options)
	bool RequiresDialog() const;

	template<typename T = UObject>
	FORCEINLINE T* GetSubject() const { return Cast<T>(Subject.Get()); }

	FORCEINLINE UObject* GetSubjectChecked() const
	{
		UObject* Object = Subject.Get();
		check(Object);
		return Object;
	}

	template<typename T>
	FORCEINLINE T* GetSubjectChecked() const { return CastChecked<T>(Subject.Get()); }

	const FRigVMUserWorkflow& GetWorkflow() const { return Workflow; }

	void Report(EMessageSeverity::Type InSeverity, const FString& InMessage) const;

	template <typename FmtType, typename... Types>
	void Reportf(EMessageSeverity::Type InSeverity, const FmtType& Fmt, Types... Args) const
	{
		Report(InSeverity, FString::Printf(Fmt, Args...));
	}

	UFUNCTION(BlueprintCallable, Category=Options)
	FORCEINLINE void ReportError(const FString& InMessage)
	{
		Report(EMessageSeverity::Error, InMessage);
	}

	UFUNCTION(BlueprintCallable, Category=Options)
	FORCEINLINE void ReportWarning(const FString& InMessage)
	{
		Report(EMessageSeverity::Warning, InMessage);
	}

	UFUNCTION(BlueprintCallable, Category=Options)
	FORCEINLINE void ReportInfo(const FString& InMessage)
	{
		Report(EMessageSeverity::Info, InMessage);
	}

protected:

	virtual bool RequiresDialog(const FProperty* InProperty) const;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Options, meta = (AllowPrivateAccess = true))
	TObjectPtr<UObject> Subject;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Options, meta = (AllowPrivateAccess = true))
	FRigVMUserWorkflow Workflow;

	FRigVMReportDelegate ReportDelegate;

	friend class URigVMController;
};
