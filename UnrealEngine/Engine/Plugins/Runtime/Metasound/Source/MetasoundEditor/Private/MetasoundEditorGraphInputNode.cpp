// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorGraphInputNode.h"

#include "Algo/Count.h"
#include "Algo/Transform.h"
#include "AudioParameterControllerInterface.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditorSettings.h"
#include "MetasoundDataReference.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorGraphValidation.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundPrimitives.h"
#include "Misc/Guid.h"
#include "Sound/SoundWave.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorGraphInputNode)

UMetasoundEditorGraphMember* UMetasoundEditorGraphInputNode::GetMember() const
{
	return Input;
}

FMetasoundFrontendClassName UMetasoundEditorGraphInputNode::GetClassName() const
{
	if (ensure(Input))
	{
		return Input->ClassName;
	}

	return Super::GetClassName();
}

void UMetasoundEditorGraphInputNode::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	if (Input)
	{
		if (UMetasoundEditorGraphMemberDefaultLiteral* DefaultLiteral = Input->GetLiteral())
		{
			DefaultLiteral->UpdatePreviewInstance(InParameterName, InParameterInterface);
		}
	}
}

FGuid UMetasoundEditorGraphInputNode::GetNodeID() const
{
	if (Input)
	{
		return Input->NodeID;
	}

	return Super::GetNodeID();
}

FLinearColor UMetasoundEditorGraphInputNode::GetNodeTitleColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		return EditorSettings->InputNodeTitleColor;
	}

	return Super::GetNodeTitleColor();
}

FSlateIcon UMetasoundEditorGraphInputNode::GetNodeTitleIcon() const
{
	static const FName NativeIconName = "MetasoundEditor.Graph.Node.Class.Input";
	return FSlateIcon("MetaSoundStyle", NativeIconName);
}

void UMetasoundEditorGraphInputNode::Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult)
{
#if WITH_EDITOR
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	Super::Validate(OutResult);

	FNodeHandle NodeHandle = GetNodeHandle();
	const FMetasoundFrontendClassMetadata& Metadata = NodeHandle->GetClassMetadata();

	const FMetasoundFrontendVersion& MetasoundFrontendVersion = NodeHandle->GetInterfaceVersion();

	FName InterfaceNameToValidate = MetasoundFrontendVersion.Name;
	FMetasoundFrontendInterface InterfaceToValidate;
	if (ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceNameToValidate, InterfaceToValidate))
	{
		const FName& NodeName = NodeHandle->GetNodeName();
		FText RequiredText;
		if (InterfaceToValidate.IsMemberInputRequired(NodeName, RequiredText))
		{
			TArray<FConstInputHandle> InputHandles = NodeHandle->GetConstInputs();
			if (ensure(!InputHandles.IsEmpty()))
			{
				const FConstInputHandle& InputHandle = InputHandles.Last();
				if (!InputHandle->IsConnected())
				{
					OutResult.SetMessage(EMessageSeverity::Warning, *RequiredText.ToString());
				}
			}
		}
	}
#endif // #if WITH_EDITOR
}

void UMetasoundEditorGraphInputNode::SetNodeID(FGuid InNodeID)
{
	if (Input)
	{
		Input->NodeID = InNodeID;
	}
}

