// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ControlRig.h"
#include "Logging/TokenizedMessage.h"

#include "ControlRigValidationPass.generated.h"

class UControlRigValidationPass;

DECLARE_DELEGATE(FControlRigValidationClearDelegate);
DECLARE_DELEGATE_FourParams(FControlRigValidationReportDelegate, EMessageSeverity::Type, const FRigElementKey&, float, const FString&);
// todo DECLARE_DELEGATE_TwoParams(FControlRigValidationControlRigChangedDelegate, UControlRigValidator*, UControlRig*);

USTRUCT()
struct FControlRigValidationContext
{
	GENERATED_BODY()

public:

	FControlRigValidationContext();

	void Clear();
	void Report(EMessageSeverity::Type InSeverity, const FString& InMessage);
	void Report(EMessageSeverity::Type InSeverity, const FRigElementKey& InKey, const FString& InMessage);
	void Report(EMessageSeverity::Type InSeverity, const FRigElementKey& InKey, float InQuality, const FString& InMessage);
	
	FControlRigValidationClearDelegate& OnClear() { return ClearDelegate;  }
	FControlRigValidationReportDelegate& OnReport() { return ReportDelegate; }

	FControlRigDrawInterface* GetDrawInterface() { return DrawInterface; }

	FString GetDisplayNameForEvent(const FName& InEventName) const;

private:

	FControlRigValidationClearDelegate ClearDelegate;;
	FControlRigValidationReportDelegate ReportDelegate;

	FControlRigDrawInterface* DrawInterface;

	friend class UControlRigValidator;
};

/** Used to perform validation on a debugged Control Rig */
UCLASS()
class CONTROLRIG_API UControlRigValidator : public UObject
{
	GENERATED_UCLASS_BODY()

	UControlRigValidationPass* FindPass(UClass* InClass) const;
	UControlRigValidationPass* AddPass(UClass* InClass);
	void RemovePass(UClass* InClass);

	UControlRig* GetControlRig() const { return WeakControlRig.Get(); }
	void SetControlRig(UControlRig* InControlRig);

	FControlRigValidationClearDelegate& OnClear() { return ValidationContext.OnClear(); }
	FControlRigValidationReportDelegate& OnReport() { return ValidationContext.OnReport(); }
	// todo FControlRigValidationControlRigChangedDelegate& OnControlRigChanged() { return ControlRigChangedDelegate;  }

private:

	UPROPERTY()
	TArray<TObjectPtr<UControlRigValidationPass>> Passes;

	void OnControlRigInitialized(UControlRig* Subject, EControlRigState State, const FName& EventName);
	void OnControlRigExecuted(UControlRig* Subject, EControlRigState State, const FName& EventName);

	FControlRigValidationContext ValidationContext;
	TWeakObjectPtr<UControlRig> WeakControlRig;
	// todo FControlRigValidationControlRigChangedDelegate ControlRigChangedDelegate;
};


/** Used to perform validation on a debugged Control Rig */
UCLASS(Abstract)
class CONTROLRIG_API UControlRigValidationPass : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// Called whenever the rig being validated question is changed
	virtual void OnSubjectChanged(UControlRig* InControlRig, FControlRigValidationContext* InContext) {}

	// Called whenever the rig in question is initialized
	virtual void OnInitialize(UControlRig* InControlRig, FControlRigValidationContext* InContext) {}

	// Called whenever the rig is running an event
	virtual void OnEvent(UControlRig* InControlRig, const FName& InEventName, FControlRigValidationContext* InContext) {}
};
