// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/NodeSpawners/RigVMEdGraphFunctionRefNodeSpawner.h"
#include "EdGraph/NodeSpawners/RigVMEdGraphUnitNodeSpawner.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "K2Node_Variable.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMBlueprintUtils.h"
#include "ScopedTransaction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEdGraphFunctionRefNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#include "SGraphActionMenu.h"
#include "GraphEditorSettings.h" 
#endif

#define LOCTEXT_NAMESPACE "RigVMEdGraphFunctionRefNodeSpawner"

// 3 possible creation types:
// - CreateFromFunction(URigVMLibraryNode) --> This is a local function. Valid Header.
// - CreateFromAssetData(const FAssetData& InAssetData, const FRigVMGraphFunctionHeader& InPublicFunction) --> Public function. Valid Header.
// - CreateFromAssetData(const FAssetData& InAssetData, const FRigVMOldPublicFunctionData& InPublicFunction) --> Public function. Valid AssetData and Header.Name

URigVMEdGraphFunctionRefNodeSpawner* URigVMEdGraphFunctionRefNodeSpawner::CreateFromFunction(URigVMLibraryNode* InFunction)
{
	check(InFunction);

	URigVMEdGraphFunctionRefNodeSpawner* NodeSpawner = NewObject<URigVMEdGraphFunctionRefNodeSpawner>(GetTransientPackage());
	NodeSpawner->ReferencedPublicFunctionHeader = InFunction->GetFunctionHeader();
	NodeSpawner->NodeClass = URigVMEdGraphNode::StaticClass();
	NodeSpawner->bIsLocalFunction = true;

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;

	const FString Category = InFunction->GetNodeCategory();

	MenuSignature.MenuName = FText::FromString(InFunction->GetName());
	MenuSignature.Tooltip = InFunction->GetToolTipText();

	static const FString LocalFunctionString = TEXT("Local Function");
	if(MenuSignature.Tooltip.IsEmpty())
	{
		MenuSignature.Tooltip = FText::FromString(LocalFunctionString);
	}
	else
	{
		static constexpr TCHAR Format[] = TEXT("%s\n\n%s");
		MenuSignature.Tooltip = FText::FromString(FString::Printf(Format, *MenuSignature.Tooltip.ToString(), *LocalFunctionString));
	}
	
	MenuSignature.Category = FText::FromString(Category);
	MenuSignature.Keywords = FText::FromString(InFunction->GetNodeKeywords());

	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	if (MenuSignature.Keywords.IsEmpty())
	{
		// want to set it to something so we won't end up back in this condition
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}

	MenuSignature.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");

#if WITH_EDITOR
	if (InFunction->IsMutable())
	{
		MenuSignature.IconTint = GetDefault<UGraphEditorSettings>()->FunctionCallNodeTitleColor;
	}
	else
	{
		MenuSignature.IconTint = GetDefault<UGraphEditorSettings>()->PureFunctionCallNodeTitleColor;
	}
#endif
	
	return NodeSpawner;
}

URigVMEdGraphFunctionRefNodeSpawner* URigVMEdGraphFunctionRefNodeSpawner::CreateFromAssetData(const FAssetData& InAssetData, const FRigVMGraphFunctionHeader& InPublicFunction)
{
	URigVMEdGraphFunctionRefNodeSpawner* NodeSpawner = NewObject<URigVMEdGraphFunctionRefNodeSpawner>(GetTransientPackage());
	NodeSpawner->ReferencedPublicFunctionHeader = InPublicFunction;
	NodeSpawner->NodeClass = URigVMEdGraphNode::StaticClass();
	NodeSpawner->bIsLocalFunction = false;

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;

	MenuSignature.MenuName = FText::FromName(InPublicFunction.Name);
	MenuSignature.Category = FText::FromString(InPublicFunction.Category);
	MenuSignature.Keywords = FText::FromString(InPublicFunction.Keywords);
	MenuSignature.Tooltip = InPublicFunction.GetTooltip();

	const FString PackagePathString = InAssetData.PackageName.ToString();
	if(MenuSignature.Tooltip.IsEmpty())
	{
		MenuSignature.Tooltip = FText::FromString(PackagePathString);
	}
	else
	{
		static constexpr TCHAR Format[] = TEXT("%s\n\n%s");
		MenuSignature.Tooltip = FText::FromString(FString::Printf(Format, *MenuSignature.Tooltip.ToString(), *PackagePathString));
	}

	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	if (MenuSignature.Keywords.IsEmpty())
	{
		// want to set it to something so we won't end up back in this condition
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}

	MenuSignature.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");

#if WITH_EDITOR
	if (InPublicFunction.IsMutable())
	{
		MenuSignature.IconTint = GetDefault<UGraphEditorSettings>()->FunctionCallNodeTitleColor;
	}
	else
	{
		MenuSignature.IconTint = GetDefault<UGraphEditorSettings>()->PureFunctionCallNodeTitleColor;
	}
#endif
	
	return NodeSpawner;
}

URigVMEdGraphFunctionRefNodeSpawner* URigVMEdGraphFunctionRefNodeSpawner::CreateFromAssetData(const FAssetData& InAssetData, const FRigVMOldPublicFunctionData& InPublicFunction)
{
	URigVMEdGraphFunctionRefNodeSpawner* NodeSpawner = NewObject<URigVMEdGraphFunctionRefNodeSpawner>(GetTransientPackage());
	NodeSpawner->NodeClass = URigVMEdGraphNode::StaticClass();
	NodeSpawner->bIsLocalFunction = false;

	FRigVMGraphFunctionHeader& Header = NodeSpawner->ReferencedPublicFunctionHeader;
	Header.Name = InPublicFunction.Name;
	Header.Arguments.Reserve(InPublicFunction.Arguments.Num());
	for (const FRigVMOldPublicFunctionArg& Arg : InPublicFunction.Arguments)
	{
		FRigVMGraphFunctionArgument NewArgument;
		NewArgument.Name = Arg.Name;
		NewArgument.Direction = Arg.Direction;
		NewArgument.bIsArray = Arg.bIsArray;
		NewArgument.CPPType = Arg.CPPType;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		NewArgument.CPPTypeObject = FSoftObjectPath(Arg.CPPTypeObjectPath);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		Header.Arguments.Add(NewArgument);
	}
	NodeSpawner->AssetPath = InAssetData.ToSoftObjectPath();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;

	const FString Category = InPublicFunction.Category;

	MenuSignature.MenuName = FText::FromName(InPublicFunction.Name);
	MenuSignature.Category = FText::FromString(Category);
	MenuSignature.Keywords = FText::FromString(InPublicFunction.Keywords);

	if(const URigVMBlueprint* ReferencedBlueprint = Cast<URigVMBlueprint>(InAssetData.FastGetAsset(false)))
	{
		if(const URigVMFunctionLibrary* FunctionLibrary = ReferencedBlueprint->GetLocalFunctionLibrary())
		{
			if(const URigVMLibraryNode* FunctionNode = FunctionLibrary->FindFunction(InPublicFunction.Name))
			{
				MenuSignature.Tooltip = FunctionNode->GetToolTipText();
			}
		}
	}

	const FString ObjectPathString = InAssetData.GetObjectPathString();
	if(MenuSignature.Tooltip.IsEmpty())
	{
		MenuSignature.Tooltip = FText::FromString(ObjectPathString);
	}
	else
	{
		static constexpr TCHAR Format[] = TEXT("%s\n\n%s");
		MenuSignature.Tooltip = FText::FromString(FString::Printf(Format, *MenuSignature.Tooltip.ToString(), *ObjectPathString));
	}

	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	if (MenuSignature.Keywords.IsEmpty())
	{
		// want to set it to something so we won't end up back in this condition
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}

	MenuSignature.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");

#if WITH_EDITOR
	if (InPublicFunction.IsMutable())
	{
		MenuSignature.IconTint = GetDefault<UGraphEditorSettings>()->FunctionCallNodeTitleColor;
	}
	else
	{
		MenuSignature.IconTint = GetDefault<UGraphEditorSettings>()->PureFunctionCallNodeTitleColor;
	}
#endif
	
	return NodeSpawner;
}

void URigVMEdGraphFunctionRefNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

FBlueprintNodeSignature URigVMEdGraphFunctionRefNodeSpawner::GetSpawnerSignature() const
{
	FString SignatureString = TEXT("Invalid RigFunction");
	if (ReferencedPublicFunctionHeader.IsValid())
	{
		SignatureString = 
			FString::Printf(TEXT("RigFunction=%s"),
			*ReferencedPublicFunctionHeader.GetHash());
	}
	else
	{
		SignatureString = 
			FString::Printf(TEXT("RigFunction=%s:%s"),
			*AssetPath.ToString(),
			*ReferencedPublicFunctionHeader.Name.ToString());
	}

	if(bIsLocalFunction)
	{
		SignatureString += TEXT(" (local)");
	}

	return FBlueprintNodeSignature(SignatureString);
}

FBlueprintActionUiSpec URigVMEdGraphFunctionRefNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* URigVMEdGraphFunctionRefNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	URigVMEdGraphNode* NewNode = nullptr;

	// if we are trying to build the real function ref - but we haven't loaded the asset yet...
	if(!FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph))
	{
		if (!ReferencedPublicFunctionHeader.IsValid() && AssetPath.IsValid())
		{
			if (URigVMBlueprint* Blueprint = Cast<URigVMBlueprint>(AssetPath.TryLoad()))
			{
				ReferencedPublicFunctionHeader = Blueprint->GetLocalFunctionLibrary()->FindFunction(ReferencedPublicFunctionHeader.Name)->GetFunctionHeader();			
			}
		}
	}

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	if(ReferencedPublicFunctionHeader.IsValid() && !bIsTemplateNode)
	{
#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->CancelTransaction(0);
		}
#endif

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);
		NewNode = SpawnNode(ParentGraph, Blueprint, ReferencedPublicFunctionHeader, Location);
	}
	else
	{
		// we are only going to get here if we are spawning a template node
		TArray<FPinInfo> Pins;
		for(const FRigVMGraphFunctionArgument& Arg : ReferencedPublicFunctionHeader.Arguments)
		{
			Pins.Emplace(Arg.Name, Arg.Direction, Arg.CPPType, Arg.CPPTypeObject.Get());
		}

		NewNode = SpawnTemplateNode(ParentGraph, Pins);
	}

	return NewNode;
}

URigVMEdGraphNode* URigVMEdGraphFunctionRefNodeSpawner::SpawnNode(UEdGraph* ParentGraph, UBlueprint* Blueprint, FRigVMGraphFunctionHeader& InFunction, FVector2D const Location)
{
	URigVMEdGraphNode* NewNode = nullptr;
	URigVMBlueprint* RigBlueprint = Cast<URigVMBlueprint>(Blueprint);
	URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(ParentGraph);

	if (RigBlueprint != nullptr && RigGraph != nullptr)
	{
		FName Name = FRigVMBlueprintUtils::ValidateName(RigBlueprint, InFunction.Name.ToString());
		URigVMController* Controller = RigBlueprint->GetController(ParentGraph);

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		if (URigVMFunctionReferenceNode* ModelNode = Controller->AddFunctionReferenceNodeFromDescription(InFunction, Location, Name.ToString(), true, true))
		{
			NewNode = Cast<URigVMEdGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
			check(NewNode);

			if (NewNode)
			{
				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);

				URigVMEdGraphUnitNodeSpawner::HookupMutableNode(ModelNode, RigBlueprint);
			}

			for(URigVMNode* OtherModelNode : ModelNode->GetGraph()->GetNodes())
			{
				if(OtherModelNode == ModelNode)
				{
					continue;
				}
				
				URigVMFunctionReferenceNode* ExistingFunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(OtherModelNode);
				if(ExistingFunctionReferenceNode == nullptr)
				{
					continue;
				}

				if(!(ExistingFunctionReferenceNode->GetReferencedFunctionHeader().LibraryPointer == InFunction.LibraryPointer))
				{
					continue;
				}

				for(TPair<FName, FName> MappedVariablePair : ExistingFunctionReferenceNode->GetVariableMap())
				{
					if(!MappedVariablePair.Value.IsNone())
					{
						Controller->SetRemappedVariable(ModelNode, MappedVariablePair.Key, MappedVariablePair.Value, true);
					}
				}
			}

			Controller->CloseUndoBracket();
		}
		else
		{
			Controller->CancelUndoBracket();
		}
	}
	return NewNode;
}

bool URigVMEdGraphFunctionRefNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	if(URigVMEdGraphNodeSpawner::IsTemplateNodeFilteredOut(Filter))
	{
		return true;
	}
	
	if(bIsLocalFunction)
	{
		if(ReferencedPublicFunctionHeader.IsValid())
		{
			for (UBlueprint* Blueprint : Filter.Context.Blueprints)
			{
				if(Cast<IRigVMGraphFunctionHost>(Blueprint->GeneratedClass) != ReferencedPublicFunctionHeader.GetFunctionHost())
				{
					return true;
				}
			}
		}
	}
	const FString ReferencedAssetObjectPathString = ReferencedPublicFunctionHeader.LibraryPointer.LibraryNode.GetAssetName();
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if(Blueprint->GetPathName() == ReferencedAssetObjectPathString)
		{
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

