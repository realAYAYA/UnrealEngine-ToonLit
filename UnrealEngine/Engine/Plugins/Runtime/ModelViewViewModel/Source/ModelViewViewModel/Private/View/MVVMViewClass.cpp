// Copyright Epic Games, Inc. All Rights Reserved.

#include "View/MVVMViewClass.h"
#include "Types/MVVMFieldContext.h"
#include "View/MVVMView.h"
#include "View/MVVMViewModelContextResolver.h"

#include "Bindings/MVVMFieldPathHelper.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "MVVMGameSubsystem.h"
#include "MVVMMessageLog.h"
#include "MVVMSubsystem.h"

#include <limits>

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMViewClass)


#define LOCTEXT_NAMESPACE "MVVMViewClass"

///////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////
UObject* FMVVMViewClass_SourceCreator::CreateInstance(const UMVVMViewClass* InViewClass, UMVVMView* InView, UUserWidget* InUserWidget) const
{
	check(InViewClass);
	check(InView);
	check(InUserWidget);

	UObject* Result = nullptr;
	const bool bOptional = (Flags & (uint8)ESourceFlags::IsOptional) != 0;

	if ((Flags & (uint8)ESourceFlags::TypeCreateInstance) != 0)
	{
		if (ExpectedSourceType.Get() != nullptr)
		{
			Result = NewObject<UObject>(InUserWidget, ExpectedSourceType.Get(), NAME_None, RF_Transient);
		}
		else if (!bOptional)
		{
			UE::MVVM::FMessageLog Log(InUserWidget);
			Log.Error(FText::Format(LOCTEXT("CreateInstanceCreateInstance", "The source '{0}' could not be created. The class is not loaded."), FText::FromName(PropertyName)));
		}
	}
	else if (Resolver)
	{
		Result = Resolver->CreateInstance(ExpectedSourceType.Get(), InUserWidget, InView);
		if (!Result && !bOptional)
		{
			UE::MVVM::FMessageLog Log(InUserWidget);
			Log.Error(FText::Format(LOCTEXT("CreateInstanceFailResolver", "The source '{0}' could not be created. Resolver returned an invalid value."), FText::FromName(PropertyName)));
		}
		if (Result && !Result->IsA(ExpectedSourceType.Get()))
		{
			Result = nullptr;

			UE::MVVM::FMessageLog Log(InUserWidget);
			Log.Error(FText::Format(LOCTEXT("CreateInstanceFailResolverExpected", "The source '{0}' could not be created. Resolver returned viewodel of an unexpected type."), FText::FromName(PropertyName)));
		}
	}
	else if (GlobalViewModelInstance.IsValid())
	{
		UMVVMViewModelCollectionObject* Collection = nullptr;
		UMVVMViewModelBase* FoundViewModelInstance = nullptr;
		if (const UWorld* World = InUserWidget->GetWorld())
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				Collection = GameInstance->GetSubsystem<UMVVMGameSubsystem>()->GetViewModelCollection();
				if (Collection)
				{
					FoundViewModelInstance = Collection->FindViewModelInstance(GlobalViewModelInstance);
				}
			}
		}

		if (FoundViewModelInstance != nullptr)
		{
			ensureMsgf(FoundViewModelInstance->IsA(GlobalViewModelInstance.ContextClass), TEXT("The Global View Model Instance is not of the expected type."));
			Result = FoundViewModelInstance;
		}
		else if (!bOptional)
		{
			UE::MVVM::FMessageLog Log(InUserWidget);
			if (Collection)
			{
				Log.Error(FText::Format(LOCTEXT("CreateInstanceFailedGlobal", "The source '{0}' was not found in the global view model collection."), FText::FromName(GlobalViewModelInstance.ContextName)));
			}
			else
			{
				Log.Error(FText::Format(LOCTEXT("CreateInstanceFailedGlobalInstance", "The source '{0}' will be invalid because the global view model collection could not be found."), FText::FromName(GlobalViewModelInstance.ContextName)));
			}
		}
	}
	else if (FieldPath.IsValid())
	{
		TValueOrError<UE::MVVM::FFieldContext, void> FieldPathResult = InViewClass->GetBindingLibrary().EvaluateFieldPath(InUserWidget, FieldPath);
		if (FieldPathResult.HasValue())
		{
			TValueOrError<UObject*, void> ObjectResult = UE::MVVM::FieldPathHelper::EvaluateObjectProperty(FieldPathResult.GetValue());
			if (ObjectResult.HasValue() && ObjectResult.GetValue() != nullptr)
			{
				Result = ObjectResult.GetValue();
			}
		}

		if (Result == nullptr && !bOptional)
		{
			UE::MVVM::FMessageLog Log(InUserWidget);
			Log.Error(FText::Format(LOCTEXT("CreateInstanceInvalidBiding", "The source '{0}' was evaluated to be invalid at initialization."), FText::FromName(PropertyName)));
		}
	}

	return Result;
}

void FMVVMViewClass_SourceCreator::DestroyInstance(const UObject* ViewModel, const UMVVMView* View) const
{
	if (Resolver)
	{
		Resolver->DestroyInstance(ViewModel, View);
	}
}

#if UE_WITH_MVVM_DEBUGGING
FMVVMViewClass_SourceCreator::FToStringArgs FMVVMViewClass_SourceCreator::FToStringArgs::Short()
{
	FToStringArgs Result;
	Result.bUseDisplayName = false;
	Result.bAddCreationMode = false;
	Result.bAddFlags = false;
	return Result;
}

FMVVMViewClass_SourceCreator::FToStringArgs FMVVMViewClass_SourceCreator::FToStringArgs::All()
{
	return FToStringArgs();
}

FString FMVVMViewClass_SourceCreator::ToString(const FMVVMCompiledBindingLibrary& BindingLibrary, FToStringArgs Args) const
{
	TStringBuilder<512> StringBuilder;
	StringBuilder << TEXT("Type: ");
#if WITH_EDITOR
	if (Args.bUseDisplayName)
	{
		StringBuilder << ExpectedSourceType->GetDisplayNameText().ToString();
	}
	else
#endif
	{
		StringBuilder << ExpectedSourceType->GetPathName();
	}
	
	StringBuilder << TEXT(", SourceName: ");
	StringBuilder << GetSourceName();

	if (!ParentSourceName.IsNone())
	{
		StringBuilder << TEXT(", ParentSourceName: ");
		StringBuilder << GetParentSourceName();
	}

	if (Args.bAddFlags)
	{
		bool bHasFlag = false;
		auto AddPipe = [&StringBuilder, &bHasFlag]()
		{
			if (bHasFlag)
			{
				StringBuilder << TEXT('|');
			}
			bHasFlag = true;
		};

		StringBuilder << TEXT(", Flags: ");
		if ((Flags & (uint8)ESourceFlags::IsOptional) != 0)
		{
			AddPipe();
			StringBuilder << TEXT("Optional");
		}
		if (CanBeSet())
		{
			AddPipe();
			StringBuilder << TEXT("CanBeSet");
		}
		if (CanBeEvaluated())
		{
			AddPipe();
			StringBuilder << TEXT("CanBeEvaluated");
		}
		if (IsSourceAUserWidgetProperty())
		{
			AddPipe();
			StringBuilder << TEXT("IsUserWidgetProperty");
		}
	}

	if (Args.bAddCreationMode)
	{
		StringBuilder << TEXT("\n    CreationType: ");
		if ((Flags & (uint8)ESourceFlags::TypeCreateInstance) != 0)
		{
			StringBuilder << TEXT("CreateInsance");
		}
		else if (Resolver)
		{
			StringBuilder << Resolver->GetClass()->GetFullName();
		}
		else if (GlobalViewModelInstance.IsValid())
		{
			StringBuilder << TEXT("GlobalCollection={");
			if (GlobalViewModelInstance.ContextClass)
			{
				StringBuilder << GlobalViewModelInstance.ContextClass->GetPathName();
			}
			else
			{
				StringBuilder << TEXT("<Invalid>");
			}
			StringBuilder << TEXT(", ");
			StringBuilder << GlobalViewModelInstance.ContextName;
			StringBuilder << TEXT("}");
		}
		else if (FieldPath.IsValid())
		{
			StringBuilder << TEXT("Path=");
			TValueOrError<FString, FString> FieldPathString = BindingLibrary.FieldPathToString(FieldPath, Args.bUseDisplayName);
			if (FieldPathString.HasValue())
			{
				StringBuilder << FieldPathString.GetValue();
			}
			else
			{
				StringBuilder << TEXT("<Error>");
				StringBuilder << FieldPathString.GetError();
			}
		}
		else
		{
			StringBuilder << TEXT("None"); // The setting is Manual. At runtime, do nothing.
		}
	}

	return StringBuilder.ToString();
}
#endif

#if WITH_EDITOR
void FMVVMViewClass_SourceCreator::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		if (Flags == 0)
		{
			Flags |= bCreateInstance_DEPRECATED ? (uint8)ESourceFlags::TypeCreateInstance : 0;
			Flags |= bIsUserWidgetProperty_DEPRECATED ? (uint8)ESourceFlags::IsUserWidgetProperty : 0;
			Flags |= bOptional_DEPRECATED ? (uint8)ESourceFlags::IsOptional : 0;
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////
namespace UE::MVVM::Private
{
	int32 GDefaultEvaluationMode = (int32)EMVVMExecutionMode::Immediate;
	static FAutoConsoleVariableRef CVarDefaultEvaluationMode(
		TEXT("MVVM.DefaultExecutionMode"),
		GDefaultEvaluationMode,
		TEXT("The default evaluation mode of a MVVM binding.")
	);
}

EMVVMExecutionMode FMVVMViewClass_CompiledBinding::GetExecuteMode() const
{
	EMVVMExecutionMode DefaultMode = (EMVVMExecutionMode)UE::MVVM::Private::GDefaultEvaluationMode;
	EMVVMExecutionMode Result = (Flags & EBindingFlags::OverrideExecuteMode) == 0 ? DefaultMode : ExecutionMode;
	return Result == EMVVMExecutionMode::DelayedWhenSharedElseImmediate ? (Binding.IsShared() ? EMVVMExecutionMode::Delayed : EMVVMExecutionMode::Immediate) : Result;
}

#if UE_WITH_MVVM_DEBUGGING
FMVVMViewClass_CompiledBinding::FToStringArgs FMVVMViewClass_CompiledBinding::FToStringArgs::Short()
{
	FToStringArgs Result;
	Result.bUseDisplayName = false;
	Result.bAddFieldPath = false;
	Result.bAddFlags = false;
	return Result;
}

FMVVMViewClass_CompiledBinding::FToStringArgs FMVVMViewClass_CompiledBinding::FToStringArgs::All()
{
	return FToStringArgs();
}

FString FMVVMViewClass_CompiledBinding::ToString() const
{
#if WITH_EDITOR
	return EditorId.ToString();
#else
	return FString();
#endif
}

FString FMVVMViewClass_CompiledBinding::ToString(const FMVVMCompiledBindingLibrary& BindingLibrary, FToStringArgs Args) const
{
	TStringBuilder<1024> StringBuilder;

#if WITH_EDITOR
	if (Args.bAddBindingId)
	{
		StringBuilder << TEXT("BindingId: ");
		EditorId.AppendString(StringBuilder);
		StringBuilder << TEXT("\n    ");
	}
#endif

	if (Args.bAddFieldPath)
	{
		if (GetBinding().IsValid())
		{
			StringBuilder << TEXT("Binding: ");

			FString SourceString;
			FString DestinationString;
			FString ConversionString;
			bool bErrorStrings = false;
			if (!IsConversionFunctionComplex())
			{
				TValueOrError<FString, FString> SourceFieldPathString = BindingLibrary.FieldPathToString(GetBinding().GetSourceFieldPath(), Args.bUseDisplayName);
				SourceString = SourceFieldPathString.HasValue() ? SourceFieldPathString.StealValue() : SourceFieldPathString.StealError();
				bErrorStrings = bErrorStrings || SourceFieldPathString.HasError();
			}
			{
				TValueOrError<FString, FString> DestinationFieldPathString = BindingLibrary.FieldPathToString(GetBinding().GetDestinationFieldPath(), Args.bUseDisplayName);
				DestinationString = DestinationFieldPathString.HasValue() ? DestinationFieldPathString.StealValue() : DestinationFieldPathString.StealError();
				bErrorStrings = bErrorStrings || DestinationFieldPathString.HasError();
			}
			if (GetBinding().GetConversionFunctionFieldPath().IsValid())
			{
				TValueOrError<FString, FString> ConversionFieldPathString = BindingLibrary.FieldPathToString(GetBinding().GetConversionFunctionFieldPath(), Args.bUseDisplayName);
				ConversionString = ConversionFieldPathString.HasValue() ? ConversionFieldPathString.StealValue() : ConversionFieldPathString.StealError();
				bErrorStrings = bErrorStrings || ConversionFieldPathString.HasError();
			}

			if (bErrorStrings)
			{
				StringBuilder << TEXT("Error: ");
			}

			if (Args.bUseDisplayName)
			{
				StringBuilder << TEXT('"');
			}
			StringBuilder << DestinationString;
			if (Args.bUseDisplayName)
			{
				StringBuilder << TEXT('"');
			}
			StringBuilder << TEXT(" = ");
			if (GetBinding().GetConversionFunctionFieldPath().IsValid())
			{
				if (Args.bUseDisplayName)
				{
					StringBuilder << TEXT('"');
				}
				StringBuilder << ConversionString;
				if (Args.bUseDisplayName)
				{
					StringBuilder << TEXT('"');
				}
				StringBuilder << TEXT(" ( ");
			}

			if (!IsConversionFunctionComplex())
			{
				if (Args.bUseDisplayName)
				{
					StringBuilder << TEXT('"');
				}
				StringBuilder << SourceString;
				if (Args.bUseDisplayName)
				{
					StringBuilder << TEXT('"');
				}
			}

			if (GetBinding().GetConversionFunctionFieldPath().IsValid())
			{
				StringBuilder << TEXT(" )");
			}

			StringBuilder << TEXT("\n    ");
		}
		else if (Args.bAddFieldPath && IsEvaluateSourceCreatorBinding())
		{
			StringBuilder << TEXT("Evaluate Source Creator: [");
			StringBuilder << EvaluateSourceCreatorIndex;
			StringBuilder << TEXT("]\n    ");
		}
		else
		{
			StringBuilder << TEXT("!!Invalid Binding Error!!");
			StringBuilder << TEXT("\n    ");
		}
	}

	StringBuilder << TEXT("SourceName: ");
	StringBuilder << GetSourceName();

	if (Args.bAddFieldId)
	{
		if (GetSourceFieldId().IsValid())
		{
			TValueOrError<UE::FieldNotification::FFieldId, void> SourceFieldId = BindingLibrary.GetFieldId(GetSourceFieldId());
			StringBuilder << TEXT(", FieldId: ");
			StringBuilder << (SourceFieldId.HasValue() ? SourceFieldId.GetValue().GetName() : FName());
		}
	}

	if (Args.bAddFlags)
	{
		if ((Flags & EBindingFlags::OverrideExecuteMode) != 0)
		{
			StringBuilder << TEXT(", Mode: ");
			StringBuilder << StaticEnum<EMVVMExecutionMode>()->GetNameByValue((int64)ExecutionMode);
		}

		StringBuilder << TEXT(", Flags: ");

		bool bAddPipe = false;
		auto AddPipe = [&StringBuilder, &bAddPipe]()
		{
			if (bAddPipe)
			{
				StringBuilder << TEXT('|');
			}
			bAddPipe = true;
		};

		if ((Flags & EBindingFlags::ExecuteAtInitialization) != 0)
		{
			AddPipe();
			StringBuilder << TEXT("ExecAtInit");
		}
		if ((Flags & EBindingFlags::OneTime) != 0)
		{
			AddPipe();
			StringBuilder << TEXT("OneTime");
		}
		if ((Flags & EBindingFlags::EnabledByDefault) != 0)
		{
			AddPipe();
			StringBuilder << TEXT("EnabledByDefault");
		}
		if ((Flags & EBindingFlags::ViewModelOptional) != 0)
		{
			AddPipe();
			StringBuilder << TEXT("Optional");
		}
		if (IsConversionFunctionComplex())
		{
			AddPipe();
			StringBuilder << TEXT("Complex");
		}
		if ((Flags & EBindingFlags::OverrideExecuteMode) != 0)
		{
			AddPipe();
			StringBuilder << TEXT("OverrideExecutionMode");
		}
		if ((Flags & EBindingFlags::SourceObjectIsSelf) != 0)
		{
			AddPipe();
			StringBuilder << TEXT("Self");
		}
	}

	return StringBuilder.ToString();
}
#endif


///////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////

void UMVVMViewClass::Initialize(UUserWidget* UserWidget)
{
	ensure(UserWidget->GetExtension<UMVVMView>() == nullptr);
	UMVVMView* View = UserWidget->AddExtension<UMVVMView>();
	if (ensure(View))
	{
		View->SetFlags(RF_Transient);
		View->ConstructView(this);
	}
}

void UMVVMViewClass::Construct(UUserWidget* UserWidget)
{
	++ViewCounter;
	ensure(ViewCounter <= std::numeric_limits<int32>::max());

	if (ViewCounter == 1)
	{
		BindingLibrary.Load();
	}

#if WITH_EDITOR
	if (GEditor)
	{
		BluerpintCompiledHandle = GEditor->OnBlueprintCompiled().AddUObject(this, &UMVVMViewClass::HandleBlueprintCompiled);
	}
#endif
}


void UMVVMViewClass::Destruct(UUserWidget* UserWidget)
{
	--ViewCounter;
	ensure(ViewCounter >= 0);
	if (ViewCounter == 0)
	{
		BindingLibrary.Unload();
	}

#if WITH_EDITOR
	if (GEditor && BluerpintCompiledHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(BluerpintCompiledHandle);
		BluerpintCompiledHandle.Reset();
	}
#endif
}

#if WITH_EDITOR
void UMVVMViewClass::BeginDestroy()
{
	if (GEditor && BluerpintCompiledHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(BluerpintCompiledHandle);
		BluerpintCompiledHandle.Reset();
	}
	Super::BeginDestroy();
}

void UMVVMViewClass::HandleBlueprintCompiled()
{
	// do not keep property alive of potential dead BP class.
	if (ViewCounter > 0)
	{
		BindingLibrary.Unload();
		BindingLibrary.Load();
	}
}
#endif

#if UE_WITH_MVVM_DEBUGGING
void UMVVMViewClass::Log(FMVVMViewClass_SourceCreator::FToStringArgs SourceArgs, FMVVMViewClass_CompiledBinding::FToStringArgs BindingArgs) const
{
	bool bPreviousLoad = ViewCounter > 0;
	if (!bPreviousLoad)
	{
		const_cast<UMVVMViewClass*>(this)->BindingLibrary.Load();
	}

	TStringBuilder<2048> Builder;
	Builder << TEXT("Compiled Sources for: ");
	Builder << GetOutermost()->GetFName();
	for (const FMVVMViewClass_SourceCreator& Sources : GetViewModelCreators())
	{
		Builder << TEXT("\n");
		Builder << Sources.ToString(GetBindingLibrary(), SourceArgs);
	}
	UE_LOG(LogMVVM, Log, TEXT("%s"), Builder.ToString());

	Builder.Reset();

	Builder << TEXT("Compiled Bindings for: ");
	Builder << GetOutermost()->GetFName();
	for (const FMVVMViewClass_CompiledBinding& Binding : GetCompiledBindings())
	{
		Builder << TEXT("\n");
		Builder << Binding.ToString(GetBindingLibrary(), BindingArgs);
	}
	UE_LOG(LogMVVM, Log, TEXT("%s"), Builder.ToString());

	if (!bPreviousLoad)
	{
#if WITH_EDITOR
		const_cast<UMVVMViewClass*>(this)->BindingLibrary.Unload();
#endif
	}
}
#endif

#undef LOCTEXT_NAMESPACE

