// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/MetasoundInputFormatInterfaces.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Metasound.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundDataReference.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendSearchEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"


#if WITH_DEV_AUTOMATION_TESTS
namespace EngineTestMetaSoundPatchBuilderPrivate
{
	UMetaSoundPatchBuilder& CreatePatchBuilderChecked(FAutomationTestBase& Test, FName BuilderName, const TArray<FName>& InterfaceNamesToAdd)
	{
		EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
		UMetaSoundPatchBuilder* Builder = UMetaSoundBuilderSubsystem::GetChecked().CreatePatchBuilder(BuilderName, Result);
		checkf(Builder, TEXT("Failed to create MetaSoundPatchBuilder '%s', required for all further testing."), *BuilderName.ToString());
		Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded, TEXT("Builder created but CreatePatchBuilder did not result in 'Succeeded' state"));

		for (const FName& InterfaceName : InterfaceNamesToAdd)
		{
			Builder->AddInterface(InterfaceName, Result);
			Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded, FString::Printf(TEXT("Failed to add initial interface '%s' to MetaSound patch"), *InterfaceName.ToString()));
		}

		return *Builder;
	}

	UMetaSoundPatch* CreatePatchFromBuilder(FAutomationTestBase& Test, const FString& PatchName, const TArray<FName>& InterfaceNamesToAdd)
	{
		UMetaSoundPatchBuilder& Builder = CreatePatchBuilderChecked(Test, FName(PatchName + TEXT(" Builder")), InterfaceNamesToAdd);

		UMetaSoundPatch* InputPatch = CastChecked<UMetaSoundPatch>(Builder.Build(nullptr, FMetaSoundBuilderOptions { FName(PatchName) }).GetObject());
		Test.AddErrorIfFalse(InputPatch != nullptr, FString::Printf(TEXT("Failed to build MetaSound patch '%s'"), *PatchName));
		return InputPatch;
	}
} // namespace EngineTestMetaSoundPatchBuilderPrivate


DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FMetaSoundSourceBuilderDisconnectInputLatentCommand, FAutomationTestBase&, Test, UMetaSoundBuilderBase*, Builder, FMetaSoundBuilderNodeInputHandle, InputToDisconnect);

bool FMetaSoundSourceBuilderDisconnectInputLatentCommand::Update()
{
	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	if (Builder)
	{
		Builder->DisconnectNodeInput(InputToDisconnect, Result);
		Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded, TEXT("Failed to disconnect MetaSound node's input"));
	}

	return Result == EMetaSoundBuilderResult::Succeeded;
}

DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(FMetaSoundSourceBuilderSetLiteralLatentCommand, FAutomationTestBase&, Test, UMetaSoundBuilderBase*, Builder, FMetaSoundBuilderNodeInputHandle, NodeInput, FMetasoundFrontendLiteral, NewValue);

bool FMetaSoundSourceBuilderSetLiteralLatentCommand::Update()
{
	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	if (Builder)
	{
		Builder->SetNodeInputDefault(NodeInput, NewValue, Result);
		Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded, TEXT("Failed to disconnect MetaSound node's input"));
	}

	return Result == EMetaSoundBuilderResult::Succeeded;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FMetaSoundSourceBuilderRemoveNodeDefaultLiteralLatentCommand, FAutomationTestBase&, Test, UMetaSoundBuilderBase*, Builder, FMetaSoundBuilderNodeInputHandle, NodeInput);

bool FMetaSoundSourceBuilderRemoveNodeDefaultLiteralLatentCommand::Update()
{
	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	if (Builder)
	{
		Builder->RemoveNodeInputDefault(NodeInput, Result);
		Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded, TEXT("Failed to disconnect MetaSound node's input"));
	}

	return Result == EMetaSoundBuilderResult::Succeeded;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FMetaSoundSourceBuilderCreateAndConnectTriGeneratorNodeLatentCommand, FAutomationTestBase&, Test, UMetaSoundSourceBuilder*, Builder, FMetaSoundBuilderNodeInputHandle, AudioOutNodeInput);

bool FMetaSoundSourceBuilderCreateAndConnectTriGeneratorNodeLatentCommand::Update()
{
	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	if (Builder)
	{
		// Tri Oscillator Node
		FMetaSoundNodeHandle TriNode = Builder->AddNodeByClassName({ "UE", "Triangle", "Audio" }, 1, Result);
		Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded && TriNode.IsSet(), TEXT("Failed to create node by class name 'UE:Triangle:Audio"));

		FMetaSoundBuilderNodeOutputHandle TriNodeAudioOutput = Builder->FindNodeOutputByName(TriNode, "Audio", Result);
		Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded, TEXT("Failed to find Triangle Oscillator node output 'Audio'"));

		Builder->ConnectNodes(TriNodeAudioOutput, AudioOutNodeInput, Result);
		Test.AddErrorIfFalse(Result == EMetaSoundBuilderResult::Succeeded, TEXT("Failed to connect 'Audio' Triangle Oscillator output to MetaSound graph's 'Mono Output'"));
	}

	return Result == EMetaSoundBuilderResult::Succeeded;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBuilderRemoveFromRootLatentCommand, UMetaSoundBuilderBase*, Builder);

bool FBuilderRemoveFromRootLatentCommand::Update()
{
	if (Builder)
	{
		Builder->RemoveFromRoot();
		return true;
	}
	return false;
}

// Creates a collection of MetaSound patches from builders and exercises bindings all input and output interfaces by connecting and disconnecting using various builder API calls.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioMetaSoundBuilderInterfaceBindingConnectAndDisconnect, "Audio.Metasound.InterfaceBindingConnectAndDisconnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAudioMetaSoundBuilderInterfaceBindingConnectAndDisconnect::RunTest(const FString& Parameters)
{
	using namespace EngineTestMetaSoundPatchBuilderPrivate;
	using namespace Metasound::Engine;

	const TArray<FName> InputInterfaces =
	{
		InputFormatMonoInterface::GetVersion().Name,
		InputFormatStereoInterface::GetVersion().Name,
		InputFormatQuadInterface::GetVersion().Name,
		InputFormatFiveDotOneInterface::GetVersion().Name,
		InputFormatSevenDotOneInterface::GetVersion().Name
	};
	const TArray<FName> OutputInterfaces =
	{
		OutputFormatMonoInterface::GetVersion().Name,
		OutputFormatStereoInterface::GetVersion().Name,
		OutputFormatQuadInterface::GetVersion().Name,
		OutputFormatFiveDotOneInterface::GetVersion().Name,
		OutputFormatSevenDotOneInterface::GetVersion().Name
	};

	auto MakePatchName = [](FName InterfaceName) { return FString::Printf(TEXT("%s Patch"), *InterfaceName.ToString()); };
	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	for (const FName& InputInterface : InputInterfaces)
	{
		if (UMetaSoundPatch* InputBuilder = CreatePatchFromBuilder(*this, MakePatchName(InputInterface), { InputInterface }))
		{
			for (const FName& OutputInterface : OutputInterfaces)
			{
				if (UMetaSoundPatch* OutputBuilder = CreatePatchFromBuilder(*this, MakePatchName(OutputInterface), { OutputInterface }))
				{
					const FString BindBuilderName = FString::Printf(TEXT("%s to %s Binding"), *OutputInterface.ToString(), *InputInterface.ToString());
					UMetaSoundPatchBuilder& BindingBuilder = CreatePatchBuilderChecked(*this, *BindBuilderName, { InputInterface, OutputInterface });
					FMetaSoundNodeHandle InputInterfaceNode = BindingBuilder.AddNode(TScriptInterface<IMetaSoundDocumentInterface>(InputBuilder), Result);
					FMetaSoundNodeHandle OutputInterfaceNode = BindingBuilder.AddNode(TScriptInterface<IMetaSoundDocumentInterface>(OutputBuilder), Result);
					BindingBuilder.ConnectNodesByInterfaceBindings(OutputInterfaceNode, InputInterfaceNode, Result);
					AddErrorIfFalse(
						Result == EMetaSoundBuilderResult::Succeeded,
						FString::Printf(TEXT("Failed to connect nodes via binding with interface '%s' to '%s'"), *OutputInterface.ToString(), *InputInterface.ToString())
					);

					BindingBuilder.DisconnectNodesByInterfaceBindings(OutputInterfaceNode, InputInterfaceNode, Result);
					AddErrorIfFalse(
						Result == EMetaSoundBuilderResult::Succeeded,
						FString::Printf(TEXT("Failed to disconnect nodes via binding with interface '%s' to '%s'"), *OutputInterface.ToString(), *InputInterface.ToString())
					);

					BindingBuilder.ConnectNodeInputsToMatchingGraphInterfaceInputs(InputInterfaceNode, Result);
					AddErrorIfFalse(
						Result == EMetaSoundBuilderResult::Succeeded,
						FString::Printf(TEXT("Failed to connect input node with matching graph interface inputs: interface '%s'"), *InputInterface.ToString())
					);

					BindingBuilder.ConnectNodeOutputsToMatchingGraphInterfaceOutputs(OutputInterfaceNode, Result);
					AddErrorIfFalse(
						Result == EMetaSoundBuilderResult::Succeeded,
						FString::Printf(TEXT("Failed to connect output node with matching graph interface outputs: interface '%s'"), *OutputInterface.ToString())
					);
				}
			}
		}
	}

	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
