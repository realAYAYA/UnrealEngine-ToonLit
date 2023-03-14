// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusComputeGraph.h"

#include "Components/MeshComponent.h"
#include "Internationalization/Regex.h"
#include "IOptimusComputeKernelProvider.h"
#include "IOptimusShaderTextProvider.h"
#include "IOptimusValueProvider.h"
#include "OptimusDeformer.h"
#include "OptimusCoreModule.h"
#include "OptimusNode.h"
#include "OptimusObjectVersion.h"
#include "Misc/UObjectToken.h"

#define LOCTEXT_NAMESPACE "OptimusComputeGraph"

void UOptimusComputeGraph::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FOptimusObjectVersion::GUID);
}

void UOptimusComputeGraph::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::AddBindingsToGraph)
	{
		// Add default bindings for legacy data.
		Bindings.Add(UMeshComponent::StaticClass());
		DataInterfaceToBinding.AddZeroed(DataInterfaces.Num());
	}
}

static EOptimusDiagnosticLevel ProcessCompilationMessage(UOptimusDeformer* InOwner,	UOptimusNode const* InKernelNode, FString const& InMessage)
{
	FOptimusCompilerDiagnostic Diagnostic;

	// "/Engine/Generated/ComputeFramework/Kernel_LinearBlendSkinning.usf(19,39-63):  error X3013: 'DI000_ReadNumVertices': no matching 1 parameter function"	
	// "OptimusNode_ComputeKernel_2(1,42):  error X3004: undeclared identifier 'a'"

	// TODO: Parsing diagnostics rightfully belongs at the shader compiler level, especially if the shader compiler is rewriting.
	static const FRegexPattern MessagePattern(TEXT(R"(^\s*(.*?)\((\d+),(\d+)(-(\d+))?\):\s*(error|warning)\s+[A-Z0-9]+:\s*(.*)$)"));
	FRegexMatcher Matcher(MessagePattern, InMessage);

	if (!Matcher.FindNext())
	{
		Diagnostic.Level = EOptimusDiagnosticLevel::Info;
		Diagnostic.Message = FText::FromString(InMessage);
		//Diagnostic.Object = InKernelNode;
	}
	else
	{
		const FString SeverityStr = Matcher.GetCaptureGroup(6);
		if (SeverityStr == TEXT("warning"))
		{
			Diagnostic.Level = EOptimusDiagnosticLevel::Warning;
		}
		else if (SeverityStr == TEXT("error"))
		{
			Diagnostic.Level = EOptimusDiagnosticLevel::Error;
		}

		const FString Path = Matcher.GetCaptureGroup(1);
		Diagnostic.Object = StaticFindObject(nullptr, nullptr, *Path, true);

		const FString MessageStr = Matcher.GetCaptureGroup(7);
		const bool bShowPathInMessage = Diagnostic.Object == nullptr;
		Diagnostic.Message = FText::FromString(bShowPathInMessage ? FString::Printf(TEXT("%s: %s"), *Path, *MessageStr) : MessageStr);

		const int32 LineNumber = FCString::Atoi(*Matcher.GetCaptureGroup(2));
		const int32 ColumnStart = FCString::Atoi(*Matcher.GetCaptureGroup(3));
		const FString ColumnEndStr = Matcher.GetCaptureGroup(5);
		const int32 ColumnEnd = ColumnEndStr.IsEmpty() ? ColumnStart : FCString::Atoi(*ColumnEndStr);
		Diagnostic.Line = LineNumber;
		Diagnostic.ColumnStart = ColumnStart;
		Diagnostic.ColumnEnd = ColumnEnd;
	}

	if (InOwner)
	{
		InOwner->GetCompileMessageDelegate().Broadcast(Diagnostic);
	}

	return Diagnostic.Level;
}

void UOptimusComputeGraph::OnKernelCompilationComplete(int32 InKernelIndex, const TArray<FString>& InCompileOutputMessages)
{
	// Find the Optimus objects from the raw kernel index.
	if (KernelToNode.IsValidIndex(InKernelIndex))
	{
		UOptimusDeformer* Owner = Cast<UOptimusDeformer>(GetOuter());
		
		// Make sure the node hasn't been GC'd.
		if (UOptimusNode* Node = const_cast<UOptimusNode*>(KernelToNode[InKernelIndex].Get()))
		{
			EOptimusDiagnosticLevel DiagnosticLevel = EOptimusDiagnosticLevel::None;

			for (FString const& CompileOutputMessage : InCompileOutputMessages)
			{
				EOptimusDiagnosticLevel MessageDiagnosticLevel = ProcessCompilationMessage(Owner, Node, CompileOutputMessage);
				if (MessageDiagnosticLevel > DiagnosticLevel)
				{
					DiagnosticLevel = MessageDiagnosticLevel;
				}
			}

			Node->SetDiagnosticLevel(DiagnosticLevel);
		}
	}
}

#undef LOCTEXT_NAMESPACE
