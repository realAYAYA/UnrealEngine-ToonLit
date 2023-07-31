// Copyright Epic Games, Inc. All Rights Reserved.


#include "Graph/SControlRigGraphPinVariableBinding.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraphSchema_K2.h"
#include "DetailLayoutBuilder.h"
#include "RigVMModel/RigVMController.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "SControlRigGraphPinVariableBinding"

static const FText ControlRigVariableBindingMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

void SControlRigVariableBinding::Construct(const FArguments& InArgs)
{
	this->ModelPins = InArgs._ModelPins;
	this->FunctionReferenceNode = InArgs._FunctionReferenceNode;
	this->InnerVariableName = InArgs._InnerVariableName;
	this->Blueprint = InArgs._Blueprint;
	this->bCanRemoveBinding = InArgs._CanRemoveBinding;

	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

	BindingArgs.CurrentBindingText.BindRaw(this, &SControlRigVariableBinding::GetBindingText);
	BindingArgs.CurrentBindingImage.BindRaw(this, &SControlRigVariableBinding::GetBindingImage);
	BindingArgs.CurrentBindingColor.BindRaw(this, &SControlRigVariableBinding::GetBindingColor);

	BindingArgs.OnCanBindProperty.BindSP(this, &SControlRigVariableBinding::OnCanBindProperty);
	BindingArgs.OnCanBindToClass.BindSP(this, &SControlRigVariableBinding::OnCanBindToClass);

	BindingArgs.OnAddBinding.BindSP(this, &SControlRigVariableBinding::OnAddBinding);
	BindingArgs.OnCanRemoveBinding.BindSP(this, &SControlRigVariableBinding::OnCanRemoveBinding);
	BindingArgs.OnRemoveBinding.BindSP(this, &SControlRigVariableBinding::OnRemoveBinding);

	BindingArgs.bGeneratePureBindings = true;
	BindingArgs.bAllowNewBindings = true;
	BindingArgs.bAllowArrayElementBindings = false;
	BindingArgs.bAllowStructMemberBindings = false;
	BindingArgs.bAllowUObjectFunctions = false;

	BindingArgs.MenuExtender = MakeShareable(new FExtender);
	BindingArgs.MenuExtender->AddMenuExtension(
		"Properties",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateSP(this, &SControlRigVariableBinding::FillLocalVariableMenu));

	this->ChildSlot
	[
		PropertyAccessEditor.MakePropertyBindingWidget(Blueprint, BindingArgs)
	];
}

FText SControlRigVariableBinding::GetBindingText(URigVMPin* ModelPin) const
{
	if (ModelPin)
	{
		const FString VariablePath = ModelPin->GetBoundVariablePath();
		return FText::FromString(VariablePath);
	}
	return FText();
}

FText SControlRigVariableBinding::GetBindingText() const
{
	if (ModelPins.Num() > 0)
	{
		const FText FirstText = GetBindingText(ModelPins[0]);
		for(int32 Index = 1; Index < ModelPins.Num(); Index++)
		{
			if(!GetBindingText(ModelPins[Index]).EqualTo(FirstText))
			{
				return ControlRigVariableBindingMultipleValues;
			}
		}
		return FirstText;
	}
	else if(FunctionReferenceNode && !InnerVariableName.IsNone())
	{
		const FName BoundVariable = FunctionReferenceNode->GetOuterVariableName(InnerVariableName);
		if(!BoundVariable.IsNone())
		{
			return FText::FromName(BoundVariable);
		}
	}
	return FText();
}

const FSlateBrush* SControlRigVariableBinding::GetBindingImage() const
{
	static FName TypeIcon(TEXT("Kismet.VariableList.TypeIcon"));
	static FName ArrayTypeIcon(TEXT("Kismet.VariableList.ArrayTypeIcon"));

	if (ModelPins.Num() > 0)
	{
		if(ModelPins[0]->IsArray())
		{
			return FAppStyle::GetBrush(ArrayTypeIcon);
		}
	}
	return FAppStyle::GetBrush(TypeIcon);
}

FLinearColor SControlRigVariableBinding::GetBindingColor() const
{
	if (Blueprint)
	{
		const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();
		FName BoundVariable(NAME_None);

		if(ModelPins.Num() > 0)
		{
			BoundVariable = *ModelPins[0]->GetBoundVariableName();
		}
		else if(FunctionReferenceNode && !InnerVariableName.IsNone())
		{
			BoundVariable = FunctionReferenceNode->GetOuterVariableName(InnerVariableName);
			if(BoundVariable.IsNone())
			{
				return FLinearColor::Red;
			}
		}

		for (const FBPVariableDescription& VariableDescription : Blueprint->NewVariables)
		{
			if (VariableDescription.VarName == BoundVariable)
			{
				return Schema->GetPinTypeColor(VariableDescription.VarType);
			}
		}

		if (ModelPins.Num() > 0)
		{
			URigVMGraph* Model = ModelPins[0]->GetGraph();
			if(Model == nullptr)
			{
				return  FLinearColor::Red;
			}

			const TArray<FRigVMGraphVariableDescription>& LocalVariables =  Model->GetLocalVariables(true);
			for(const FRigVMGraphVariableDescription& LocalVariable : LocalVariables)
			{
				const FRigVMExternalVariable ExternalVariable = LocalVariable.ToExternalVariable();
				if(!ExternalVariable.IsValid(true))
				{
					continue;
				}

				if (ExternalVariable.Name == BoundVariable)
				{
					const FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromExternalVariable(ExternalVariable);
					return Schema->GetPinTypeColor(PinType);
				}
			}
		}
	}
	return FLinearColor::White;
}

bool SControlRigVariableBinding::OnCanBindProperty(FProperty* InProperty) const
{
	if (InProperty == BindingArgs.Property)
	{
		return true;
	}

	if (InProperty)
	{
		const FRigVMExternalVariable ExternalVariable = FRigVMExternalVariable::Make(InProperty, nullptr);
		if(ModelPins.Num() > 0)
		{
			return ModelPins[0]->CanBeBoundToVariable(ExternalVariable);
		}
		else if(FunctionReferenceNode && !InnerVariableName.IsNone())
		{
			TArray<FRigVMExternalVariable> InnerVariables = FunctionReferenceNode->GetContainedGraph()->GetExternalVariables();
			for(const FRigVMExternalVariable& InnerVariable : InnerVariables)
			{
				if(InnerVariable.Name == InnerVariableName)
				{
					if(!InnerVariable.bIsReadOnly && ExternalVariable.bIsReadOnly)
					{
						return false;
					}
					if(InnerVariable.bIsArray != ExternalVariable.bIsArray)
					{
						return false;
					}
					if(InnerVariable.TypeObject && InnerVariable.TypeObject != ExternalVariable.TypeObject)
					{
						return false;
					}
					else if(InnerVariable.TypeName != ExternalVariable.TypeName)
					{
						return false;
					}
					return true;
				}
			}
		}
	}

	return false;
}

bool SControlRigVariableBinding::OnCanBindToClass(UClass* InClass) const
{
	if (InClass)
	{
		return InClass->ClassGeneratedBy == Blueprint;
	}
	return true;
}

void SControlRigVariableBinding::OnAddBinding(FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain)
{
	if (Blueprint)
	{
		TArray<FString> Parts;
		for (const FBindingChainElement& ChainElement : InBindingChain)
		{
			ensure(ChainElement.Field);
			Parts.Add(ChainElement.Field.GetName());
		}

		if(ModelPins.Num() > 0)
		{
			for(URigVMPin* ModelPin : ModelPins)
			{
				Blueprint->GetController(ModelPin->GetGraph())->BindPinToVariable(ModelPin->GetPinPath(), FString::Join(Parts, TEXT(".")), true /* undo */, true /* python */);
			}
		}
		else if(FunctionReferenceNode && !InnerVariableName.IsNone())
		{
			const FName BoundVariableName = *FString::Join(Parts, TEXT("."));
			Blueprint->GetController(FunctionReferenceNode->GetGraph())->SetRemappedVariable(FunctionReferenceNode, InnerVariableName, BoundVariableName);
		}
	}
}

bool SControlRigVariableBinding::OnCanRemoveBinding(FName InPropertyName)
{
	return bCanRemoveBinding;
}

void SControlRigVariableBinding::OnRemoveBinding(FName InPropertyName)
{
	if (Blueprint)
	{
		if(ModelPins.Num() > 0)
		{
			for(URigVMPin* ModelPin : ModelPins)
			{
				Blueprint->GetController(ModelPin->GetGraph())->UnbindPinFromVariable(ModelPin->GetPinPath(), true /* undo */, true /* python */);
			}
		}
		else if(FunctionReferenceNode && !InnerVariableName.IsNone())
		{
			Blueprint->GetController(FunctionReferenceNode->GetGraph())->SetRemappedVariable(FunctionReferenceNode, InnerVariableName, NAME_None);
		}
	}
}

void SControlRigVariableBinding::FillLocalVariableMenu(FMenuBuilder& MenuBuilder)
{
	if(ModelPins.Num() == 0)
	{
		return;
	}

	URigVMGraph* Model = ModelPins[0]->GetGraph();
	if(Model == nullptr)
	{
		return;
	}

	int32 ValidLocalVariables = 0;
	const TArray<FRigVMGraphVariableDescription>& LocalVariables =  Model->GetLocalVariables(true);
	for(const FRigVMGraphVariableDescription& LocalVariable : LocalVariables)
	{
		const FRigVMExternalVariable ExternalVariable = LocalVariable.ToExternalVariable();
		if(!ExternalVariable.IsValid(true))
		{
			continue;
		}

		if(!ModelPins[0]->CanBeBoundToVariable(ExternalVariable))
		{
			continue;
		}

		ValidLocalVariables++;
	}

	if(ValidLocalVariables == 0)
	{
		return;
	}
	
	MenuBuilder.BeginSection("LocalVariables", LOCTEXT("LocalVariables", "Local Variables"));
	{
		static FName PropertyIcon(TEXT("Kismet.VariableList.TypeIcon"));
		const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();

		for(const FRigVMGraphVariableDescription& LocalVariable : LocalVariables)
		{
			const FRigVMExternalVariable ExternalVariable = LocalVariable.ToExternalVariable();
			if(!ExternalVariable.IsValid(true))
			{
				continue;
			}

			if(!ModelPins[0]->CanBeBoundToVariable(ExternalVariable))
			{
				continue;
			}
			
			const FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromExternalVariable(ExternalVariable);

			MenuBuilder.AddMenuEntry(
				FUIAction(FExecuteAction::CreateSP(this, &SControlRigVariableBinding::HandleBindToLocalVariable, LocalVariable)),
				SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSpacer)
						.Size(FVector2D(18.0f, 0.0f))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(1.0f, 0.0f)
					[
						SNew(SImage)
						.Image(FBlueprintEditorUtils::GetIconFromPin(PinType, true))
						.ColorAndOpacity(Schema->GetPinTypeColor(PinType))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromName(LocalVariable.Name))
					]);
		}
	}
	MenuBuilder.EndSection(); // Local Variables
}

void SControlRigVariableBinding::HandleBindToLocalVariable(FRigVMGraphVariableDescription InLocalVariable)
{
	if(ModelPins.IsEmpty() || (Blueprint == nullptr))
	{
		return;
	}

	for(URigVMPin* ModelPin : ModelPins)
	{
		URigVMGraph* Model = ModelPin->GetGraph();
		if(Model == nullptr)
		{
			continue;
		}

		URigVMController* Controller = Blueprint->GetOrCreateController(Model);
		if(Controller == nullptr)
		{
			continue;
		}

		Controller->BindPinToVariable(ModelPin->GetPinPath(), InLocalVariable.Name.ToString(), true, true);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SControlRigGraphPinVariableBinding::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	this->ModelPins = InArgs._ModelPins;
	this->Blueprint = InArgs._Blueprint;

	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SControlRigGraphPinVariableBinding::GetDefaultValueWidget()
{
	return SNew(SControlRigVariableBinding)
		.Blueprint(Blueprint)
		.ModelPins(ModelPins);
}

#undef LOCTEXT_NAMESPACE