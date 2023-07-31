// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMViewModelBindingListWidget.h"

#include "Algo/Transform.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "FieldNotification/IFieldValueChanged.h"
#include "Misc/MemStack.h"
#include "Misc/Optional.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "Types/MVVMAvailableBinding.h"
#include "Types/MVVMFieldVariant.h"
#include "WidgetBlueprint.h"
#include "Widgets/PropertyViewer/SPropertyViewer.h"

#define LOCTEXT_NAMESPACE "SSourceBindingList"

using UE::PropertyViewer::SPropertyViewer;

namespace UE::MVVM
{

namespace Private
{
	TOptional<FFieldVariant> PassFilter(const FMVVMAvailableBinding& Binding, const FMVVMFieldVariant& FieldVariant, EFieldVisibility FieldVisibilityFlags, const FProperty* AssignableTo, bool bDoObjectProperty);
	TOptional<FFieldVariant> PassFilter(const FMVVMAvailableBinding& Binding, const UStruct* Struct, EFieldVisibility FieldVisibilityFlags, const FProperty* AssignableTo, bool bDoObjectProperty);

	TOptional<FFieldVariant> PassFilter(const FMVVMAvailableBinding& Binding, const UStruct* Struct, EFieldVisibility FieldVisibilityFlags, const FProperty* AssignableTo, bool bDoObjectProperty)
	{
		if (EnumHasAllFlags(FieldVisibilityFlags, EFieldVisibility::Readable) && !Binding.IsReadable())
		{
			return TOptional<FFieldVariant>();
		}

		if (EnumHasAllFlags(FieldVisibilityFlags, EFieldVisibility::Notify) && !Binding.HasNotify())
		{
			return TOptional<FFieldVariant>();
		}

		FMVVMFieldVariant FieldVariant = BindingHelper::FindFieldByName(Struct, Binding.GetBindingName());
		return PassFilter(Binding, FieldVariant, FieldVisibilityFlags, AssignableTo, bDoObjectProperty);
	}

	TOptional<FFieldVariant> PassFilter(const FMVVMAvailableBinding& Binding, const FMVVMFieldVariant& FieldVariant, EFieldVisibility FieldVisibilityFlags, const FProperty* AssignableTo, bool bDoObjectProperty)
	{
		if (ensure(!FieldVariant.IsEmpty()))
		{
			if (FieldVariant.IsFunction())
			{
				if (EnumHasAllFlags(FieldVisibilityFlags, EFieldVisibility::Writable) && !Binding.IsWritable())
				{
					return TOptional<FFieldVariant>();
				}

				if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsFunctionAllowed(FieldVariant.GetFunction()))
				{
					return TOptional<FFieldVariant>();
				}

				const UFunction* Function = FieldVariant.GetFunction();
				if (Function != nullptr && AssignableTo != nullptr && 
					!BindingHelper::ArePropertiesCompatible(BindingHelper::GetReturnProperty(Function), AssignableTo))
				{
					return TOptional<FFieldVariant>();
				}

				return FFieldVariant(Function);
			}
			else if (FieldVariant.IsProperty())
			{
				if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsPropertyAllowed(FieldVariant.GetProperty()))
				{
					return TOptional<FFieldVariant>();
				}

				const FProperty* Property = FieldVariant.GetProperty();
				if (Property != nullptr && AssignableTo != nullptr && 
					!BindingHelper::ArePropertiesCompatible(Property, AssignableTo))
				{
					return TOptional<FFieldVariant>();
				}

				// If the path ends with the object property, then it needs to follow the writable rule
				if (bDoObjectProperty || !CastField<FObjectPropertyBase>(FieldVariant.GetProperty()))
				{
					if (EnumHasAllFlags(FieldVisibilityFlags, EFieldVisibility::Writable) && !Binding.IsWritable())
					{
						return TOptional<FFieldVariant>();
					}
				}

				return FFieldVariant(Property);
			}
		}

		return TOptional<FFieldVariant>();
	}
}

FFieldIterator_Bindable::FFieldIterator_Bindable(const UWidgetBlueprint* InWidgetBlueprint, EFieldVisibility InVisibilityFlags, const FProperty* InAssignableTo) :
	WidgetBlueprint(InWidgetBlueprint),
	FieldVisibilityFlags(InVisibilityFlags),
	AssignableTo(InAssignableTo)
{
}

TArray<FFieldVariant> FFieldIterator_Bindable::GetFields(const UStruct* Struct) const
{
	TArray<FFieldVariant> Result;

	auto AddResult = [this, &Result, Struct](const TArray<FMVVMAvailableBinding>& AvailableBindingsList)
	{
		Result.Reserve(AvailableBindingsList.Num());
		for (const FMVVMAvailableBinding& Value : AvailableBindingsList)
		{
			TOptional<FFieldVariant> PassResult = Private::PassFilter(Value, Struct, FieldVisibilityFlags, AssignableTo, false);
			if (PassResult.IsSet())
			{
				Result.Add(MoveTemp(PassResult.GetValue()));
			}
		}
	};

	if (const UClass* Class = Cast<const UClass>(Struct))
	{
		const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get();
		TSubclassOf<UObject> AccessorClass = WidgetBlueprintPtr ? WidgetBlueprintPtr->GeneratedClass : nullptr;
		TArray<FMVVMAvailableBinding> Bindings = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetAvailableBindings(const_cast<UClass*>(Class), AccessorClass);
		AddResult(Bindings);
	}
	else if (const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(Struct))
	{
		TArray<FMVVMAvailableBinding> Bindings = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetAvailableBindingsForStruct(ScriptStruct);
		AddResult(Bindings);
	}

	Result.Sort([](const FFieldVariant& A, const FFieldVariant& B)
		{
			bool bIsAViewModel = A.Get<FObjectPropertyBase>() && A.Get<FObjectPropertyBase>()->PropertyClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass());
			bool bIsBViewModel = B.Get<FObjectPropertyBase>() && B.Get<FObjectPropertyBase>()->PropertyClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass());
			if (A.IsUObject() && B.IsUObject())
			{
				return A.GetFName().LexicalLess(B.GetFName());
			}
			else if (bIsAViewModel && bIsBViewModel)
			{
				return A.GetFName().LexicalLess(B.GetFName());
			}
			else if (bIsAViewModel)
			{
				return true;
			}
			else if (bIsBViewModel)
			{
				return false;
			}
			else if (A.IsUObject())
			{
				return true;
			}
			else if (B.IsUObject())
			{
				return false;
			}
			return A.GetFName().LexicalLess(B.GetFName());
		});

	return Result;
}

/** */
void SSourceBindingList::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint)
{
	WidgetBlueprint = InWidgetBlueprint;
	FieldIterator = MakeUnique<FFieldIterator_Bindable>(InWidgetBlueprint, InArgs._FieldVisibilityFlags, InArgs._AssignableTo);
	FieldExpander = MakeUnique<UE::PropertyViewer::FFieldExpander_Default>();
	FieldExpander->SetExpandObject(UE::PropertyViewer::FFieldExpander_Default::EObjectExpandFlag::UseInstanceClass);
	FieldExpander->SetExpandScriptStruct(true);
	FieldExpander->SetExpandFunction(false);

	OnDoubleClicked = InArgs._OnDoubleClicked;

	PropertyViewer = SNew(SPropertyViewer)
		.FieldIterator(FieldIterator.Get())
		.FieldExpander(FieldExpander.Get())
		.PropertyVisibility(SPropertyViewer::EPropertyVisibility::Hidden)
		.bShowFieldIcon(true)
		.bSanitizeName(true)
		.SelectionMode(InArgs._EnableSelection ? ESelectionMode::Single : ESelectionMode::None)
		.bShowSearchBox(InArgs._ShowSearchBox)
		.OnSelectionChanged(this, &SSourceBindingList::HandleSelectionChanged)
		.OnDoubleClicked(this, &SSourceBindingList::HandleDoubleClicked);

	ChildSlot
	[
		PropertyViewer.ToSharedRef()
	];
}

void SSourceBindingList::ClearSources()
{
	SelectedPath = FMVVMBlueprintPropertyPath();

	Sources.Reset();
	if (PropertyViewer)
	{
		PropertyViewer->RemoveAll();
	}
}

void SSourceBindingList::AddSource(UClass* Class, FName Name, FGuid Guid)
{
	FBindingSource Source;
	Source.Class = Class;
	Source.Name = Name;
	Source.ViewModelId = Guid;

	AddSources(MakeArrayView(&Source, 1));
} 

void SSourceBindingList::AddWidgetBlueprint(const UWidgetBlueprint* InWidgetBlueprint)
{
	FBindingSource Source;
	Source.Class = InWidgetBlueprint->GeneratedClass;
	Source.Name = InWidgetBlueprint->GetFName();
	Source.DisplayName = FText::FromString(InWidgetBlueprint->GetName());

	AddSources(MakeArrayView(&Source, 1 ));
}

void SSourceBindingList::AddWidgets(TArrayView<const UWidget*> InWidgets)
{
	TArray<FBindingSource, TInlineAllocator<16>> NewSources;
	NewSources.Reserve(InWidgets.Num());

	for (const UWidget* Widget : InWidgets)
	{
		FBindingSource& Source = NewSources.AddDefaulted_GetRef();
		Source.Class = Widget->GetClass();
		Source.Name = Widget->GetFName();
		Source.DisplayName = Widget->GetLabelText();
	}

	AddSources(NewSources);
}

void SSourceBindingList::AddViewModels(TArrayView<const FMVVMBlueprintViewModelContext> InViewModels)
{
	TArray<FBindingSource, TInlineAllocator<16>> NewSources;
	NewSources.Reserve(InViewModels.Num());

	for (const FMVVMBlueprintViewModelContext& ViewModelContext : InViewModels)
	{
		FBindingSource& Source = NewSources.AddDefaulted_GetRef();
		Source.Class = ViewModelContext.GetViewModelClass();
		Source.ViewModelId = ViewModelContext.GetViewModelId();
	}

	AddSources(NewSources);
}

void SSourceBindingList::AddSource(const FBindingSource& InSource)
{
	AddSources(MakeArrayView(&InSource, 1));
}

void SSourceBindingList::AddSources(TArrayView<const FBindingSource> InSources)
{
	if (ensure(PropertyViewer))
	{
		const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get();
		for (const FBindingSource& Source : InSources)
		{
			if (const UClass* SourceClass = Source.Class.Get())
			{
				SPropertyViewer::FHandle Handle;
				if (SourceClass && Source.Class->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
				{
					UWidget* Widget = nullptr;
					if (WidgetBlueprintPtr && SourceClass->IsChildOf(UWidget::StaticClass()))
					{
						Widget = WidgetBlueprintPtr->WidgetTree->FindWidget(Source.Name);
					}

					if (Widget)
					{
						Handle = PropertyViewer->AddInstance(Widget);
					}
					else
					{
						Handle = PropertyViewer->AddContainer(SourceClass);
					}
				}
				Sources.Emplace(Source, Handle);
			}
		}
	}
}

FMVVMBlueprintPropertyPath SSourceBindingList::CreateBlueprintPropertyPath(SPropertyViewer::FHandle Handle, TArrayView<const FFieldVariant> FieldPath) const
{
	if (FieldPath.Num() < 0)
	{
		return FMVVMBlueprintPropertyPath();
	}

	const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get();
	ensure(WidgetBlueprintPtr);
	if (!WidgetBlueprintPtr)
	{
		return FMVVMBlueprintPropertyPath();
	}

	const TPair<FBindingSource, SPropertyViewer::FHandle>* Source = Sources.FindByPredicate(
		[Handle](const TPair<FBindingSource, SPropertyViewer::FHandle>& Source)
		{
			return Source.Value == Handle;
		});

	if (!Source)
	{
		return FMVVMBlueprintPropertyPath();
	}

	// Backward, test if the object can be access.
	//The last property can be a struct variable, inside a struct, inside..., inside an object. 
	bool bPassFilter = false;
	TSubclassOf<UObject> AccessorClass = WidgetBlueprintPtr ? WidgetBlueprintPtr->GeneratedClass : nullptr;
	for (int32 Index = FieldPath.Num() - 1; Index >= 0; --Index)
	{
		const FFieldVariant& FieldVariant = FieldPath[Index];
		const UStruct* OwnerStruct = nullptr;
		FName FieldName;
		if (const FProperty* Property = FieldVariant.Get<FProperty>())
		{
			OwnerStruct = Property->GetOwnerStruct();
			FieldName = Property->GetFName();
		}
		else if (const UFunction* Function = FieldVariant.Get<UFunction>())
		{
			OwnerStruct = Function->GetOwnerClass();
			FieldName = Function->GetFName();
		}

		if (const UClass* OwnerClass = Cast<const UClass>(OwnerStruct))
		{
			FMVVMAvailableBinding Binding = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetAvailableBinding(OwnerClass, FMVVMBindingName(FieldName), AccessorClass);
			if (Binding.IsValid())
			{
				bPassFilter = Private::PassFilter(Binding, OwnerClass, FieldIterator->GetFieldVisibilityFlags(), FieldIterator->GetAssignableTo(), true).IsSet();
			}
			break;
		}
	}

	FMVVMBlueprintPropertyPath PropertyPath;
	if (bPassFilter)
	{
		if (Source->Key.ViewModelId.IsValid())
		{
			PropertyPath.SetViewModelId(Source->Key.ViewModelId);
		}
		else
		{
			PropertyPath.SetWidgetName(Source->Key.Name);
		}

		PropertyPath.ResetBasePropertyPath();

		for (const FFieldVariant& Field : FieldPath)
		{
			PropertyPath.AppendBasePropertyPath(FMVVMConstFieldVariant(Field));
		}
	}

	return PropertyPath;
}

void SSourceBindingList::HandleSelectionChanged(SPropertyViewer::FHandle Handle, TArrayView<const FFieldVariant> FieldPath, ESelectInfo::Type SelectionType)
{
	SelectedPath = CreateBlueprintPropertyPath(Handle, FieldPath);
}

void SSourceBindingList::HandleDoubleClicked(SPropertyViewer::FHandle Handle, TArrayView<const FFieldVariant> FieldPath)
{
	if (OnDoubleClicked.IsBound())
	{
		FMVVMBlueprintPropertyPath ClickedPath = CreateBlueprintPropertyPath(Handle, FieldPath);
		OnDoubleClicked.Execute(ClickedPath);
	}
}

void SSourceBindingList::SetRawFilterText(const FText& InFilterText)
{
	if (PropertyViewer)
	{
		PropertyViewer->SetRawFilterText(InFilterText);
	}
}

FMVVMBlueprintPropertyPath SSourceBindingList::GetSelectedProperty() const
{
	return SelectedPath;
}

void SSourceBindingList::SetSelectedProperty(const FMVVMBlueprintPropertyPath& Property)
{
	if (!PropertyViewer.IsValid())
	{
		return;
	}

	SPropertyViewer::FHandle SelectedHandle;
	for (TPair<FBindingSource, SPropertyViewer::FHandle>& Source : Sources)
	{
		if ((Property.IsFromViewModel() && Source.Key.ViewModelId == Property.GetViewModelId()) ||
			(Property.IsFromWidget() && Source.Key.Name == Property.GetWidgetName()))
		{
			SelectedHandle = Source.Value;
			break;
		}
	}

	FMemMark Mark(FMemStack::Get());
	TArray<FFieldVariant, TMemStackAllocator<>> FieldPath;
	if (SelectedHandle.IsValid())
	{
		TArray<FMVVMConstFieldVariant> FieldVariants = Property.GetFields();
		FieldPath.Reserve(FieldVariants.Num());

		for (const FMVVMConstFieldVariant& Variant : FieldVariants)
		{
			FFieldVariant& Field = FieldPath.AddDefaulted_GetRef();
			if (Variant.IsFunction())
			{
				Field = FFieldVariant(Variant.GetFunction());
			}
			else if (Variant.IsProperty())
			{
				Field = FFieldVariant(Variant.GetProperty());
			}
		}
	}
	
	PropertyViewer->SetSelection(SelectedHandle, FieldPath);
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
