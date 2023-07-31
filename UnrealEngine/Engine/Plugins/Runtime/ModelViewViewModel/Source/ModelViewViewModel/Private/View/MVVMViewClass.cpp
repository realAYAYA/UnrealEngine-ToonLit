// Copyright Epic Games, Inc. All Rights Reserved.

#include "View/MVVMViewClass.h"
#include "View/MVVMView.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Bindings/MVVMFieldPathHelper.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "FieldNotification/IFieldValueChanged.h"
#include "MVVMMessageLog.h"
#include "MVVMViewModelBase.h"
#include "MVVMSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMViewClass)


#define LOCTEXT_NAMESPACE "MVVMViewClass"

///////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////

FMVVMViewClass_SourceCreator FMVVMViewClass_SourceCreator::MakeManual(FName InName, UClass* InNotifyFieldValueChangedClass)
{
	FMVVMViewClass_SourceCreator Result;
	if (ensure(InNotifyFieldValueChangedClass && InNotifyFieldValueChangedClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass())))
	{
		Result.PropertyName = InName;
		Result.ExpectedSourceType = InNotifyFieldValueChangedClass;
		Result.bCreateInstance = false;
		Result.bOptional = true;
	}
	return Result;
}

FMVVMViewClass_SourceCreator FMVVMViewClass_SourceCreator::MakeInstance(FName InName, UClass* InNotifyFieldValueChangedClass)
{
	FMVVMViewClass_SourceCreator Result;
	if (ensure(InNotifyFieldValueChangedClass && InNotifyFieldValueChangedClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass())))
	{
		Result.PropertyName = InName;
		Result.ExpectedSourceType = InNotifyFieldValueChangedClass;
		Result.bCreateInstance = true;
		Result.bOptional = false;
	}
	return Result;
}


FMVVMViewClass_SourceCreator FMVVMViewClass_SourceCreator::MakeFieldPath(FName InName, UClass* InNotifyFieldValueChangedClass, FMVVMVCompiledFieldPath InFieldPath, bool bOptional)
{
	FMVVMViewClass_SourceCreator Result;
	if (ensure(InNotifyFieldValueChangedClass&& InNotifyFieldValueChangedClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass())))
	{
		if (ensure(InFieldPath.IsValid()))
		{
			Result.PropertyName = InName;
			Result.ExpectedSourceType = InNotifyFieldValueChangedClass;
			Result.FieldPath = InFieldPath;
			Result.bOptional = bOptional;
		}
	}
	return Result;
}


FMVVMViewClass_SourceCreator FMVVMViewClass_SourceCreator::MakeGlobalContext(FName InName, FMVVMViewModelContext InContext, bool bOptional)
{
	FMVVMViewClass_SourceCreator Result;
	if (ensure(InContext.ContextClass && InContext.ContextClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass())))
	{
		Result.PropertyName = InName;
		Result.ExpectedSourceType = InContext.ContextClass;
		Result.GlobalViewModelInstance = MoveTemp(InContext);
		Result.bOptional = bOptional;
	}
	return Result;
}


UObject* FMVVMViewClass_SourceCreator::CreateInstance(const UMVVMViewClass* InViewClass, UMVVMView* InView, UUserWidget* InUserWidget) const
{
	check(InViewClass);
	check(InView);
	check(InUserWidget);

	UObject* Result = nullptr;

	FObjectPropertyBase* FoundObjectProperty = FindFProperty<FObjectPropertyBase>(InUserWidget->GetClass(), PropertyName);
	if (ensureAlwaysMsgf(FoundObjectProperty, TEXT("The compiler should have added the property")))
	{
		auto AssignProperty = [FoundObjectProperty, InUserWidget](UObject* NewObject)
		{
			check(NewObject);
			if (ensure(NewObject->GetClass()->IsChildOf(FoundObjectProperty->PropertyClass)))
			{
				FoundObjectProperty->SetObjectPropertyValue_InContainer(InUserWidget, NewObject);
			}
		};

		if (bCreateInstance)
		{
			if (ExpectedSourceType.Get() != nullptr)
			{
				Result = NewObject<UObject>(InUserWidget, ExpectedSourceType.Get(), NAME_None, RF_Transient);
				AssignProperty(Result);
			}
			else if (!bOptional)
			{
				UE::MVVM::FMessageLog Log(InUserWidget);
				Log.Error(FText::Format(LOCTEXT("CreateInstanceCreateInstance", "The source '{0}' could not be created. The class is not loaded."), FText::FromName(PropertyName)));
			}
		}
		else if (GlobalViewModelInstance.IsValid())
		{
			UMVVMViewModelBase* FoundViewModelInstance = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetGlobalViewModelCollection()->FindViewModelInstance(GlobalViewModelInstance);
			if (FoundViewModelInstance != nullptr)
			{
				ensureMsgf(FoundViewModelInstance->IsA(GlobalViewModelInstance.ContextClass), TEXT("The Global View Model Instance is not of the expected type."));
				Result = FoundViewModelInstance;
				AssignProperty(Result);
			}
			else if (!bOptional)
			{
				UE::MVVM::FMessageLog Log(InUserWidget);
				Log.Error(FText::Format(LOCTEXT("CreateInstanceFailedGlobal", "The viewmodel '{0}' was not found in the global view model collection."), FText::FromName(GlobalViewModelInstance.ContextName)));
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
					AssignProperty(ObjectResult.GetValue());
				}
			}

			if (Result == nullptr && !bOptional)
			{
				UE::MVVM::FMessageLog Log(InUserWidget);
				Log.Error(FText::Format(LOCTEXT("CreateInstanceInvalidBiding", "The source '{0}' was evaluated to be invalid at initialization."), FText::FromName(PropertyName)));
			}
		}
	}

	return Result;
}


///////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////

FString FMVVMViewClass_CompiledBinding::ToString() const
{
	return TEXT("[PH]");
}


///////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////

void UMVVMViewClass::Initialize(UUserWidget* UserWidget)
{
	ensure(UserWidget->GetExtension<UMVVMView>() == nullptr);
	UMVVMView* View = UserWidget->AddExtension<UMVVMView>();
	if (ensure(View))
	{
		if (!bLoaded)
		{
			BindingLibrary.Load();
			bLoaded = true;
		}

		View->ConstructView(this);
	}
}

#undef LOCTEXT_NAMESPACE

