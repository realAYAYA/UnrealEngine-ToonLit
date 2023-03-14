// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigLocalVariableDetails.h"

#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "SPinTypeSelector.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "ControlRigBlueprint.h"
#include "RigVMCore/RigVM.h"

#define LOCTEXT_NAMESPACE "LocalVariableDetails"

void FRigVMLocalVariableDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	ObjectsBeingCustomized.Reset();

	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailBuilder.GetObjectsBeingCustomized(DetailObjects);
	for(TWeakObjectPtr<UObject> DetailObject : DetailObjects)
	{
		UDetailsViewWrapperObject* WrapperObject = CastChecked<UDetailsViewWrapperObject>(DetailObject.Get());
		ObjectsBeingCustomized.Add(WrapperObject);
	}

	if(ObjectsBeingCustomized[0].IsValid())
	{
		VariableDescription = ObjectsBeingCustomized[0]->GetContent<FRigVMGraphVariableDescription>();
		GraphBeingCustomized = ObjectsBeingCustomized[0]->GetTypedOuter<URigVMGraph>();
		BlueprintBeingCustomized = GraphBeingCustomized->GetTypedOuter<UControlRigBlueprint>();

		NameValidator = FControlRigLocalVariableNameValidator(BlueprintBeingCustomized, GraphBeingCustomized, VariableDescription.Name);
	}

	DetailBuilder.HideCategory(TEXT("RigVMGraphVariableDescription"));
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Local Variable"));
		
	NameHandle = DetailBuilder.GetProperty(TEXT("Name"));
	TypeHandle = DetailBuilder.GetProperty(TEXT("CPPType"));
	TypeObjectHandle = DetailBuilder.GetProperty(TEXT("CPPTypeObject"));
	DefaultValueHandle = DetailBuilder.GetProperty(TEXT("DefaultValue"));
	
	const UEdGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();

	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();
	Category.AddCustomRow( LOCTEXT("LocalVariableName", "Variable Name") )
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("LocalVariableName", "Variable Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(250.0f)
	[
		SNew(SEditableTextBox)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(this, &FRigVMLocalVariableDetails::GetName)
		.OnTextCommitted(this, &FRigVMLocalVariableDetails::SetName)
		.OnVerifyTextChanged(this, &FRigVMLocalVariableDetails::OnVerifyNameChanged)
	];

	Category.AddCustomRow(LOCTEXT("VariableTypeLabel", "Variable Type"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("VariableTypeLabel", "Variable Type"))
			.Font(DetailFontInfo)
		]
		.ValueContent()
		.MaxDesiredWidth(980.f)
		[
			SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(GetDefault<UEdGraphSchema_K2>(), &UEdGraphSchema_K2::GetVariableTypeTree))
			.TargetPinType(this, &FRigVMLocalVariableDetails::OnGetPinInfo)
			.OnPinTypeChanged(this, &FRigVMLocalVariableDetails::HandlePinInfoChanged)
			.Schema(Schema)
			.TypeTreeFilter(ETypeTreeFilter::None)
			.Font(DetailFontInfo)
		];


	if (BlueprintBeingCustomized)
	{
		UControlRigBlueprintGeneratedClass* RigClass = BlueprintBeingCustomized->GetControlRigBlueprintGeneratedClass();
		UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));
		if (CDO->GetVM() != nullptr)
		{
			FString SourcePath = FString::Printf(TEXT("LocalVariableDefault::%s|%s::Const"), *GraphBeingCustomized->GetGraphName(), *VariableDescription.Name.ToString());
			URigVMMemoryStorage* LiteralMemory = CDO->GetVM()->GetLiteralMemory();
			FProperty* Property = LiteralMemory->FindPropertyByName(*SourcePath);
			if (Property)
			{
				IDetailCategoryBuilder& DefaultValueCategory = DetailBuilder.EditCategory(TEXT("DefaultValueCategory"), LOCTEXT("DefaultValueCategoryHeading", "Default Value"));
				Property->ClearPropertyFlags(CPF_EditConst);
			
				const FName SanitizedName = FRigVMPropertyDescription::SanitizeName(*SourcePath);
				TArray<UObject*> Objects = {LiteralMemory};
				IDetailPropertyRow* Row = DefaultValueCategory.AddExternalObjectProperty(Objects, SanitizedName);
				Row->DisplayName(FText::FromName(VariableDescription.Name));

				const FSimpleDelegate OnDefaultValueChanged = FSimpleDelegate::CreateLambda([this, Property, LiteralMemory]()
				{
					VariableDescription.DefaultValue = LiteralMemory->GetDataAsString(LiteralMemory->GetPropertyIndex(Property));
					DefaultValueHandle->SetValue(VariableDescription.DefaultValue);
				});

				TSharedPtr<IPropertyHandle> Handle = Row->GetPropertyHandle();
				Handle->SetOnPropertyValueChanged(OnDefaultValueChanged);
				Handle->SetOnChildPropertyValueChanged(OnDefaultValueChanged);
			}
		}
	}
}

FText FRigVMLocalVariableDetails::GetName() const
{
	return FText::FromName(VariableDescription.Name);
}

void FRigVMLocalVariableDetails::SetName(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (InNewText.ToString() == VariableDescription.Name.ToString())
	{
		return;
	}

	VariableDescription.Name =  *InNewText.ToString();
	NameHandle->SetValue(VariableDescription.Name);
}

bool FRigVMLocalVariableDetails::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	EValidatorResult Result = NameValidator.IsValid(InText.ToString(), false);
	OutErrorMessage = INameValidatorInterface::GetErrorText(InText.ToString(), Result);	

	return Result == EValidatorResult::Ok || Result == EValidatorResult::ExistingName;
}

FEdGraphPinType FRigVMLocalVariableDetails::OnGetPinInfo() const
{
	if (!VariableDescription.Name.IsNone())
	{
		return VariableDescription.ToPinType();
	}
	return FEdGraphPinType();
}

void FRigVMLocalVariableDetails::HandlePinInfoChanged(const FEdGraphPinType& PinType)
{
	VariableDescription.ChangeType(PinType);
	FControlRigBlueprintVMCompileScope CompileScope(BlueprintBeingCustomized);
	TypeHandle->SetValue(VariableDescription.CPPType);
	TypeObjectHandle->SetValue(VariableDescription.CPPTypeObject);	
}

ECheckBoxState FRigVMLocalVariableDetails::HandleBoolDefaultValueIsChecked() const
{
	return VariableDescription.DefaultValue == "1" ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FRigVMLocalVariableDetails::OnBoolDefaultValueChanged(ECheckBoxState InCheckBoxState)
{
	VariableDescription.DefaultValue = InCheckBoxState == ECheckBoxState::Checked ? "1" : "0";
	DefaultValueHandle->SetValue(VariableDescription.DefaultValue);
}

#undef LOCTEXT_NAMESPACE
