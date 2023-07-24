// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModel/MVVMViewModelBlueprintFactory.h"
#include "MVVMViewModelBase.h"
#include "ViewModel/MVVMViewModelBlueprint.h"
#include "ViewModel/MVVMViewModelBlueprintGeneratedClass.h"

#include "ClassViewerFilter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMViewModelBlueprintFactory)

#define LOCTEXT_NAMESPACE "ViewModelBlueprintFactory"

#if UE_MVVM_WITH_VIEWMODEL_EDITOR

/*------------------------------------------------------------------------------
	UMVVMViewModelBlueprintFactory implementation.
------------------------------------------------------------------------------*/

namespace UE::MVVM::Private
{

class FViewModelClassFilter : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet <const UClass*> AllowedChildrenOfClasses;

	/** Disallowed class flags. */
	EClassFlags DisallowedClassFlags;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return !InClass->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
	}
};

}//namespace

#endif

UMVVMViewModelBlueprintFactory::UMVVMViewModelBlueprintFactory()
{
#if UE_MVVM_WITH_VIEWMODEL_EDITOR
	bCreateNew = true;
#else
	bCreateNew = false;
#endif
	bEditAfterNew = true;
	SupportedClass = UMVVMViewModelBlueprint::StaticClass();
	ParentClass = UMVVMViewModelBase::StaticClass();
}

#if UE_MVVM_WITH_VIEWMODEL_EDITOR
bool UMVVMViewModelBlueprintFactory::ConfigureProperties()
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.DisplayMode = EClassViewerDisplayMode::Type::TreeView;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.bShowNoneOption = false;
	Options.bExpandAllNodes = true;

	TSharedRef<UE::MVVM::Private::FViewModelClassFilter> Filter = MakeShared<UE::MVVM::Private::FViewModelClassFilter>();
	Filter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add(UMVVMViewModelBase::StaticClass());

	Options.ClassFilters.Add(Filter);
	Options.ExtraPickerCommonClasses.Add(UMVVMViewModelBase::StaticClass());

	const FText TitleText = LOCTEXT("CreateViewmodelBlueprint", "Pick Root Viewmodel for New ViewModel Blueprint");
	UClass* ChosenParentClass = nullptr;
	if (SClassPickerDialog::PickClass(TitleText, Options, ChosenParentClass, UMVVMViewModelBase::StaticClass()))
	{
		ParentClass = ChosenParentClass ? ChosenParentClass : UMVVMViewModelBase::StaticClass();
	}

	return true;
}

bool UMVVMViewModelBlueprintFactory::ShouldShowInNewMenu() const
{
	return true;
}

UObject* UMVVMViewModelBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Anim Blueprint, then create and init one
	check(Class->IsChildOf(UMVVMViewModelBlueprint::StaticClass()));

	UClass* CurrentParentClass = ParentClass;
	if (CurrentParentClass == nullptr)
	{
		CurrentParentClass = UMVVMViewModelBase::StaticClass();
	}

	if ((CurrentParentClass == nullptr) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(CurrentParentClass) || !CurrentParentClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), CurrentParentClass ? FText::FromString(CurrentParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateViewmodelBlueprint", "Cannot create a Viewmodel Blueprint based on the class '{ClassName}'."), Args));
		return nullptr;
	}
	else
	{

		return CastChecked<UMVVMViewModelBlueprint>(FKismetEditorUtilities::CreateBlueprint(CurrentParentClass, InParent, Name, EBlueprintType::BPTYPE_Normal, UMVVMViewModelBlueprint::StaticClass(), UMVVMViewModelBlueprintGeneratedClass::StaticClass(), CallingContext));
	}
}

UObject* UMVVMViewModelBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#endif

#undef LOCTEXT_NAMESPACE

