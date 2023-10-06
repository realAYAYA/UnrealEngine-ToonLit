// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintViewModelContextCustomization.h"

#include "Bindings/MVVMBindingHelper.h"
#include "DetailWidgetRow.h"
#include "Features/IModularFeatures.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyAccessEditor.h"
#include "IPropertyTypeCustomization.h"
#include "MVVMBlueprintView.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMEditorSubsystem.h"
#include "PropertyHandle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "WidgetBlueprintEditor.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SMVVMViewModelPanel.h"

#define LOCTEXT_NAMESPACE "BlueprintViewModelContextDetailCustomization"

namespace UE::MVVM
{
namespace Private
{
	FText BindingWidgetForVM_GetName()
	{
		return FText::GetEmpty();
	}

	bool BindingWidgetForVM_CanBindProperty(const FProperty* Property, const UClass* ClassToLookFor)
	{
		const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property);
		return (ObjectProperty != nullptr && ObjectProperty->PropertyClass->IsChildOf(ClassToLookFor));
	}
}

/**
 *
 */
bool FViewModelPropertyAccessEditor::CanBindProperty(FProperty* Property) const
{
	// Property == GeneratePureBindingsProperty is only to start the algo
	return ViewModelProperty != Property && (Private::BindingWidgetForVM_CanBindProperty(Property, ClassToLookFor.Get()) || Property == GeneratePureBindingsProperty);
}

bool FViewModelPropertyAccessEditor::CanBindFunction(UFunction* Function) const
{
	return Private::BindingWidgetForVM_CanBindProperty(BindingHelper::GetReturnProperty(Function), ClassToLookFor.Get());
}

bool FViewModelPropertyAccessEditor::CanBindToClass(UClass* Class) const
{
	return true;
}

void FViewModelPropertyAccessEditor::AddBinding(FName, const TArray<FBindingChainElement>& BindingChain)
{
	TStringBuilder<256> Path;
	for (const FBindingChainElement& Binding : BindingChain)
	{
		if (Path.Len() != 0)
		{
			Path << TEXT('.');
		}
		Path << Binding.Field.GetFName();
	}

	AssignToProperty->SetValue(Path.ToString());
}

bool FViewModelPropertyAccessEditor::HasValidClassToLookFor() const
{
	return ClassToLookFor.Get() != nullptr;
}

TSharedRef<SWidget> FViewModelPropertyAccessEditor::MakePropertyBindingWidget(TSharedRef<FWidgetBlueprintEditor> WidgetBlueprintEditor, FProperty* PropertyToMatch, TSharedRef<IPropertyHandle> InAssignToProperty, FName ViewModelPropertyName)
{
	UClass* SkeletonClass = WidgetBlueprintEditor->GetBlueprintObj()->SkeletonGeneratedClass.Get();
	if (!SkeletonClass)
	{
		return SNullWidget::NullWidget;
	}
	ViewModelProperty = SkeletonClass->FindPropertyByName(ViewModelPropertyName);
	AssignToProperty = InAssignToProperty;

	if (!IModularFeatures::Get().IsModularFeatureAvailable("PropertyAccessEditor"))
	{
		return SNullWidget::NullWidget;
	}

	GeneratePureBindingsProperty = PropertyToMatch;
	FPropertyBindingWidgetArgs Args;
	Args.Property = GeneratePureBindingsProperty;
	Args.bAllowArrayElementBindings = false;
	Args.bAllowStructMemberBindings = true;
	Args.bAllowUObjectFunctions = true;
	Args.bAllowStructFunctions = true;
	Args.bAllowNewBindings = true;
	Args.bGeneratePureBindings = true;

	Args.CurrentBindingText.BindStatic(&Private::BindingWidgetForVM_GetName);
	Args.OnCanBindProperty.BindRaw(this, &FViewModelPropertyAccessEditor::CanBindProperty);
	Args.OnCanBindFunction.BindRaw(this, &FViewModelPropertyAccessEditor::CanBindFunction);
	Args.OnCanBindToClass.BindRaw(this, &FViewModelPropertyAccessEditor::CanBindToClass);
	Args.OnAddBinding.BindRaw(this, &FViewModelPropertyAccessEditor::AddBinding);

	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
	TSharedRef<SWidget> Result = PropertyAccessEditor.MakePropertyBindingWidget(WidgetBlueprintEditor->GetBlueprintObj(), Args);
	Result->SetEnabled(MakeAttributeRaw(this, &FViewModelPropertyAccessEditor::HasValidClassToLookFor));
	return Result;
}

/**
 * 
 */
FBlueprintViewModelContextDetailCustomization::FBlueprintViewModelContextDetailCustomization(TWeakPtr<FWidgetBlueprintEditor> InEditor)
	: WidgetBlueprintEditor(InEditor)
{}


void FBlueprintViewModelContextDetailCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	FName Name_NotifyFieldValueClass = TEXT("NotifyFieldValueClass");
	FName Name_ViewModelContextId = TEXT("ViewModelContextId");
	FName Name_ViewModelName = GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewModelContext, ViewModelName);
	FName Name_ViewModelPropertyPath = GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewModelContext, ViewModelPropertyPath);
	FName Name_CreationType = GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewModelContext, CreationType);
	FName Name_CreateSetterFunction = GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewModelContext, bCreateSetterFunction);

	FGuid ViewModelContextId;
	FName ViewModelPropertyName;
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex);
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if (PropertyName == Name_NotifyFieldValueClass)
		{
			ensure(CastField<FClassProperty>(ChildHandle->GetProperty()));
			NotifyFieldValueClassHandle = ChildHandle;
			UObject* Object = nullptr;
			if (ChildHandle->GetValue(Object) == FPropertyAccess::Success)
			{
				UClass* ViewModelClass = Cast<UClass>(Object);
				if (ViewModelClass)
				{
					AllowedCreationTypes = GetAllowedContextCreationType(ViewModelClass);
				}
				PropertyAccessEditor.ClassToLookFor = ViewModelClass;
			}
			ChildHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FBlueprintViewModelContextDetailCustomization::HandleClassChanged));
		}

		else if (PropertyName == Name_ViewModelName)
		{
			ensure(CastField<FNameProperty>(ChildHandle->GetProperty()));
			ViewModelNameHandle = ChildHandle;
			ChildHandle->GetValue(ViewModelPropertyName);
		}
		else if (PropertyName == Name_ViewModelPropertyPath)
		{
			ensure(CastField<FStrProperty>(ChildHandle->GetProperty()));
			PropertyPathHandle = ChildHandle;
		}
		else if (PropertyName == Name_CreationType)
		{
			ensure(CastField<FEnumProperty>(ChildHandle->GetProperty()));
			CreationTypeHandle = ChildHandle;
		}
		else if (PropertyName == Name_ViewModelContextId)
		{
			ensure(CastField<FStructProperty>(ChildHandle->GetProperty()) && CastField<FStructProperty>(ChildHandle->GetProperty())->Struct->GetFName() == "Guid");
			void* Buffer = nullptr;
			if (ChildHandle->GetValueData(Buffer) == FPropertyAccess::Success)
			{
				ViewModelContextId = *reinterpret_cast<FGuid*>(Buffer);
			}
		}
	}

	bool bCanEdit = false;
	bool bCanRename = false;
	{

		TSharedPtr<FWidgetBlueprintEditor> Editor = WidgetBlueprintEditor.Pin();
		const UWidgetBlueprint* WidgetBP = Editor ? Editor->GetWidgetBlueprintObj() : nullptr;
		const UMVVMBlueprintView* BlueprintView = WidgetBP ? GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetView(WidgetBP) : nullptr;
		const FMVVMBlueprintViewModelContext* ViewModel = BlueprintView ? BlueprintView->FindViewModel(ViewModelContextId) : nullptr;
		if (ViewModel)
		{
			bCanEdit = ViewModel->bCanEdit;
			bCanRename = ViewModel->bCanRename;
		}
	}

	if (NotifyFieldValueClassHandle)
	{
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex);
			const FName PropertyName = ChildHandle->GetProperty()->GetFName();

			if (PropertyName == Name_ViewModelName)
			{
				ensure(CastField<FNameProperty>(ChildHandle->GetProperty()));
				if (TSharedPtr<FWidgetBlueprintEditor> SharedWidgetBlueprintEditor = WidgetBlueprintEditor.Pin())
				{
					IDetailPropertyRow& PropertyRow = ChildBuilder.AddProperty(ChildHandle.ToSharedRef());

					TSharedPtr<SWidget> NameWidget, ValueWidget;
					PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget);
					PropertyRow.CustomWidget()
					.NameContent()
					[
						NameWidget.ToSharedRef()
					]
					.ValueContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(SEditableTextBox)
							.Text(this, &FBlueprintViewModelContextDetailCustomization::GetViewModalNameValueAsText)
							.Font(CustomizationUtils.GetRegularFont())
							.SelectAllTextWhenFocused(true)
							.ClearKeyboardFocusOnCommit(false)
							.OnTextCommitted(this, &FBlueprintViewModelContextDetailCustomization::HandleNameTextCommitted)
							.OnVerifyTextChanged(this, &FBlueprintViewModelContextDetailCustomization::HandleNameVerifyTextChanged)
							.SelectAllTextOnCommit(true)
							.IsEnabled(bCanRename && ViewModelContextId.IsValid())
						]
					]
					.IsEnabled(bCanRename);
				}
			}
			else if (PropertyName == Name_ViewModelPropertyPath)
			{
				ensure(CastField<FStrProperty>(ChildHandle->GetProperty()));
				if (TSharedPtr<FWidgetBlueprintEditor> SharedWidgetBlueprintEditor = WidgetBlueprintEditor.Pin())
				{
					IDetailPropertyRow& PropertyRow = ChildBuilder.AddProperty(ChildHandle.ToSharedRef());

					TSharedPtr<SWidget> NameWidget, ValueWidget;
					PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget);
					PropertyRow.CustomWidget()
					.NameContent()
					[
						NameWidget.ToSharedRef()
					]
					.ValueContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							ValueWidget.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							PropertyAccessEditor.MakePropertyBindingWidget(SharedWidgetBlueprintEditor.ToSharedRef(), NotifyFieldValueClassHandle->GetProperty(), PropertyPathHandle.ToSharedRef(), ViewModelPropertyName)
						]
					];
				}
			}
			else if (PropertyName == Name_CreationType)
			{
				IDetailPropertyRow& PropertyRow = ChildBuilder.AddProperty(ChildHandle.ToSharedRef());

				TSharedPtr<SWidget> NameWidget, ValueWidget;
				PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget);
				PropertyRow.CustomWidget()
					.NameContent()
					[
						NameWidget.ToSharedRef()
					]
					.ValueContent()
					[
						SNew(SComboButton)
						.ContentPadding(FMargin(4.f, 0.f))
						.OnGetMenuContent(this, &FBlueprintViewModelContextDetailCustomization::CreateExecutionTypeMenuContent)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(this, &FBlueprintViewModelContextDetailCustomization::GetCreationTypeValue)
							.ToolTipText(this, &FBlueprintViewModelContextDetailCustomization::GetExecutionTypeValueToolTip)
						]
					]
					.IsEnabled(bCanEdit);
			}
			else if (PropertyName == Name_CreateSetterFunction)
			{
				if (GetDefault<UMVVMDeveloperProjectSettings>()->bAllowGeneratedViewModelSetter)
				{
					ChildBuilder.AddProperty(ChildHandle.ToSharedRef())
						.IsEnabled(bCanEdit);
				}
			}
			else
			{
				ChildBuilder.AddProperty(ChildHandle.ToSharedRef())
					.IsEnabled(bCanEdit);;
			}
		}
	}
}

void FBlueprintViewModelContextDetailCustomization::HandleClassChanged()
{
	UObject* Object = nullptr;
	AllowedCreationTypes.Reset();
	PropertyAccessEditor.ClassToLookFor.Reset();
	if (NotifyFieldValueClassHandle->GetValue(Object) == FPropertyAccess::Success)
	{
		if (UClass* ViewModelClass = Cast<UClass>(Object))
		{
			PropertyAccessEditor.ClassToLookFor = ViewModelClass;
			AllowedCreationTypes = GetAllowedContextCreationType(ViewModelClass);
		}
	}
}

TSharedRef<SWidget> FBlueprintViewModelContextDetailCustomization::CreateExecutionTypeMenuContent()
{
	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, nullptr, nullptr, true);

	const UEnum* EnumCreationType = StaticEnum<EMVVMBlueprintViewModelContextCreationType>();
	for (EMVVMBlueprintViewModelContextCreationType Type : AllowedCreationTypes)
	{
		int32 Index = EnumCreationType->GetIndexByValue((int64)Type);
		MenuBuilder.AddMenuEntry(
			EnumCreationType->GetDisplayNameTextByIndex(Index),
			EnumCreationType->GetToolTipTextByIndex(Index),
			FSlateIcon(),
			FUIAction
			(
				FExecuteAction::CreateLambda([this, Type]()
				{
					uint8 Value = static_cast<uint8>(Type);
					CreationTypeHandle->SetValue(Value);
				})
			)
		);
	}

	return MenuBuilder.MakeWidget();
}

FText FBlueprintViewModelContextDetailCustomization::GetCreationTypeValue() const
{
	uint8 Value = 0;
	if (CreationTypeHandle->GetValue(Value) == FPropertyAccess::Result::Success)
	{
		return StaticEnum<EMVVMBlueprintViewModelContextCreationType>()->GetDisplayNameTextByValue((int64)Value);
	}
	return FText::GetEmpty();
}

FText FBlueprintViewModelContextDetailCustomization::GetExecutionTypeValueToolTip() const
{
	uint8 Value = 0;
	if (CreationTypeHandle->GetValue(Value) == FPropertyAccess::Result::Success)
	{
		UEnum* EnumCreationType = StaticEnum<EMVVMBlueprintViewModelContextCreationType>();
		return EnumCreationType->GetToolTipTextByIndex(EnumCreationType->GetIndexByValue((int64)Value));
	}
	return FText::GetEmpty();
}

FText FBlueprintViewModelContextDetailCustomization::GetViewModalNameValueAsText() const
{
	check(ViewModelNameHandle.IsValid());
	FText Result;
	ViewModelNameHandle->GetValueAsFormattedText(Result);
	return Result;
}

namespace Private
{
bool VerifyViewModelName(TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor, TSharedPtr<IPropertyHandle> ViewModelNameHandle, const FText& RenameTo, bool bCommit, FText& OutErrorMessage)
{
	if (!WidgetBlueprintEditor || !ViewModelNameHandle)
	{
		return false;
	}

	if (RenameTo.IsEmptyOrWhitespace())
	{
		OutErrorMessage = LOCTEXT("EmptyViewModelName", "Empty viewmodel name.");
		return false;
	}

	const FString& NewNameString = RenameTo.ToString();
	if (NewNameString.Len() >= NAME_SIZE)
	{
		OutErrorMessage = LOCTEXT("ViewModelNameTooLong", "Viewmodel name is too long.");
		return false;
	}

	FString GeneratedName = SlugStringForValidName(NewNameString);
	if (NewNameString != GeneratedName)
	{
		OutErrorMessage = LOCTEXT("ViewModelHasInvalidChar", "ViewModel name has an invalid character.");
		return false;
	}

	FName CurrentViewModelName;
	if (ViewModelNameHandle->GetValue(CurrentViewModelName) != FPropertyAccess::Success)
	{
		OutErrorMessage = LOCTEXT("MultipleViewModel", "Can't edit multiple viewmodel name.");
		return false;
	}

	const FName GeneratedFName(*GeneratedName);
	check(GeneratedFName.IsValidXName(INVALID_OBJECTNAME_CHARACTERS));

	if (UWidgetBlueprint* WidgetBP = WidgetBlueprintEditor->GetWidgetBlueprintObj())
	{
		if (bCommit)
		{
			return GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->RenameViewModel(WidgetBP, CurrentViewModelName, *NewNameString, OutErrorMessage);
		}
		else
		{
			return GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->VerifyViewModelRename(WidgetBP, CurrentViewModelName, *NewNameString, OutErrorMessage);
		}
	}
	return false;
}
}

void FBlueprintViewModelContextDetailCustomization::HandleNameTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		FText OutErrorMessage;
		Private::VerifyViewModelName(WidgetBlueprintEditor.Pin(), ViewModelNameHandle, NewText, true, OutErrorMessage);
	}
}

bool FBlueprintViewModelContextDetailCustomization::HandleNameVerifyTextChanged(const FText& NewText, FText& OutError) const
{
	return Private::VerifyViewModelName(WidgetBlueprintEditor.Pin(), ViewModelNameHandle, NewText, false, OutError);
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
