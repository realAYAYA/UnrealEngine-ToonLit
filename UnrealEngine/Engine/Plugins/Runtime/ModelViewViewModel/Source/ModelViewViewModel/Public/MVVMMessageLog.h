// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/MessageLog.h"

#include "UObject/WeakObjectPtr.h"

struct FMVVMViewClass_Binding;
struct FMVVMViewClass_BindingKey;
class UMVVMViewClass;
class UUserWidget;

MODELVIEWVIEWMODEL_API DECLARE_LOG_CATEGORY_EXTERN(LogMVVM, Log, All);

namespace UE::MVVM
{
class MODELVIEWVIEWMODEL_API FMessageLog : private ::FMessageLog
{
public:
	static const FName LogName;

	FMessageLog(const UUserWidget* InUserWidget);

	using ::FMessageLog::AddMessage;
	using ::FMessageLog::AddMessages;
	TSharedRef<FTokenizedMessage> Message(EMessageSeverity::Type InSeverity, const FText& InMessage);
	using ::FMessageLog::CriticalError;
	TSharedRef<FTokenizedMessage> Error(const FText& InMessage);
	TSharedRef<FTokenizedMessage> PerformanceWarning(const FText& InMessage);
	TSharedRef<FTokenizedMessage> Warning(const FText& InMessage);
	TSharedRef<FTokenizedMessage> Info(const FText& InMessage);

	void AddBindingToken(TSharedRef<FTokenizedMessage> NewMessage, const UMVVMViewClass* Class, const FMVVMViewClass_Binding& ClassBinding, FMVVMViewClass_BindingKey Key);

private:
	TWeakObjectPtr<const UUserWidget> UserWidget;
};
}
