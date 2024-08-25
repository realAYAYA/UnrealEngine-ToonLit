// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EdGraphNode.h"

#include "DecoratorBase/DecoratorHandle.h"
#include "Graph/RigUnit_AnimNextDecoratorStack.h"
#include "Graph/RigDecorator_AnimNextCppDecorator.h"
#include "RigVMModel/RigVMController.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "AnimNextGraph_EdGraphNode"

void UAnimNextGraph_EdGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	URigVMEdGraphNode::GetNodeContextMenuActions(Menu, Context);

	if (IsDecoratorStack())
	{
		FToolMenuSection& Section = Menu->AddSection("AnimNextDecoratorNodeActions", LOCTEXT("AnimNextDecoratorNodeActionsMenuHeader", "Anim Next Decorator Actions"));

		UAnimNextGraph_EdGraphNode* NonConstThis = const_cast<UAnimNextGraph_EdGraphNode*>(this);

		Section.AddSubMenu(
			TEXT("AddDecoratorMenu"),
			LOCTEXT("AddDecoratorMenu", "Add Decorator"),
			LOCTEXT("AddDecoratorMenuTooltip", "Add the chosen decorator to currently selected node"),
			FNewToolMenuDelegate::CreateUObject(NonConstThis, &UAnimNextGraph_EdGraphNode::BuildAddDecoratorContextMenu));
	}
}

void UAnimNextGraph_EdGraphNode::ConfigurePin(UEdGraphPin* EdGraphPin, const URigVMPin* ModelPin) const
{
	Super::ConfigurePin(EdGraphPin, ModelPin);

	// Decorator handles always remain as a RigVM input pins so that we can still link things to them even if they are hidden
	// We handle visibility for those explicitly here
	const bool bIsInputPin = ModelPin->GetDirection() == ERigVMPinDirection::Input;
	const bool bIsDecoratorHandle = ModelPin->GetCPPTypeObject() == FAnimNextDecoratorHandle::StaticStruct();
	if (bIsInputPin && bIsDecoratorHandle)
	{
		if (const URigVMPin* DecoratorPin = ModelPin->GetParentPin())
		{
			if (DecoratorPin->IsDecoratorPin())
			{
				check(DecoratorPin->GetScriptStruct() == FRigDecorator_AnimNextCppDecorator::StaticStruct());

				TSharedPtr<FStructOnScope> DecoratorScope = DecoratorPin->GetDecoratorInstance();
				const FRigDecorator_AnimNextCppDecorator* VMDecorator = (const FRigDecorator_AnimNextCppDecorator*)DecoratorScope->GetStructMemory();

				const UScriptStruct* DecoratorStruct = VMDecorator->GetDecoratorSharedDataStruct();
				check(DecoratorStruct != nullptr);

				const FProperty* PinProperty = DecoratorStruct->FindPropertyByName(ModelPin->GetFName());
				EdGraphPin->bHidden = PinProperty->HasMetaData(FRigVMStruct::HiddenMetaName);
			}
		}
	}
}

bool UAnimNextGraph_EdGraphNode::IsDecoratorStack() const
{
	if (const URigVMUnitNode* VMNode = Cast<URigVMUnitNode>(GetModelNode()))
	{
		const UScriptStruct* ScriptStruct = VMNode->GetScriptStruct();
		return ScriptStruct == FRigUnit_AnimNextDecoratorStack::StaticStruct();
	}

	return false;
}

void UAnimNextGraph_EdGraphNode::BuildAddDecoratorContextMenu(UToolMenu* SubMenu)
{
	using namespace UE::AnimNext;

	const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();
	TArray<const FDecorator*> Decorators = DecoratorRegistry.GetDecorators();

	URigVMController* VMController = GetController();
	URigVMNode* VMNode = GetModelNode();

	const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();

	for (const FDecorator* Decorator : Decorators)
	{
		UScriptStruct* ScriptStruct = Decorator->GetDecoratorSharedDataStruct();

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			if (!CppDecoratorStructInstance.CanBeAddedToNode(VMNode, nullptr))
			{
				continue;	// This decorator isn't supported on this node
			}

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Decorator->GetDecoratorName() : DisplayNameMetadata;

		const FText ToolTip = ScriptStruct->GetToolTipText();

		FToolMenuEntry DecoratorEntry = FToolMenuEntry::InitMenuEntry(
			*Decorator->GetDecoratorName(),
			FText::FromString(DisplayName),
			ToolTip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda(
				[this, Decorator, VMController, VMNode, CppDecoratorStruct, DefaultValue, DisplayName]()
				{
					VMController->AddDecorator(
						VMNode->GetFName(),
						*CppDecoratorStruct->GetPathName(),
						*DisplayName,
						DefaultValue, INDEX_NONE, true, true);
				})
			)
		);

		SubMenu->AddMenuEntry(NAME_None, DecoratorEntry);
	}
}

#undef LOCTEXT_NAMESPACE
