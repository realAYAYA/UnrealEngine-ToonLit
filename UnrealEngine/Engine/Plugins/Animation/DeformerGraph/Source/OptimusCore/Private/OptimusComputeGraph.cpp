// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusComputeGraph.h"

#include "Components/MeshComponent.h"
#include "ComputeFramework/ComputeKernelCompileResult.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "IOptimusComputeKernelProvider.h"
#include "IOptimusShaderTextProvider.h"
#include "IOptimusValueProvider.h"
#include "OptimusDeformer.h"
#include "OptimusCoreModule.h"
#include "OptimusHelpers.h"
#include "OptimusNode.h"
#include "OptimusObjectVersion.h"
#include "Misc/UObjectToken.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusComputeGraph)

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

static EOptimusDiagnosticLevel ProcessCompilationMessage(UOptimusDeformer* InOwner,	UOptimusNode const* InKernelNode, FString const& InKernelFriendlyName, FComputeKernelCompileMessage const& InMessage)
{
	FOptimusCompilerDiagnostic Diagnostic;

	if (InMessage.Type == FComputeKernelCompileMessage::EMessageType::Error)
	{
		Diagnostic.Level = EOptimusDiagnosticLevel::Error;
	}
	else if (InMessage.Type == FComputeKernelCompileMessage::EMessageType::Warning)
	{
		Diagnostic.Level = EOptimusDiagnosticLevel::Warning;
	}
	else if (InMessage.Type == FComputeKernelCompileMessage::EMessageType::Info)
	{
		Diagnostic.Level = EOptimusDiagnosticLevel::Info;
	}

	Diagnostic.Line = InMessage.Line;
	Diagnostic.ColumnStart = InMessage.ColumnStart;
	Diagnostic.ColumnEnd = InMessage.ColumnEnd;

	// If error path is a UObject, then stpre a reference to the related object.
	FString Path = InMessage.VirtualFilePath;
	if (Optimus::ConvertShaderFilePathToObjectPath(Path))
	{
		Diagnostic.Object = StaticFindObject(nullptr, nullptr, *Path, true);
		if (UObject const* ObjectPtr = Diagnostic.Object.Get())
		{
			// Special case path display if object is the current kernel.
			Path = (ObjectPtr == InKernelNode) ? InKernelFriendlyName : ObjectPtr->GetName();
		}
	}
	
	// If error path is a real file, then store its absolute path on disk.
	if (!InMessage.RealFilePath.IsEmpty())
	{
		Diagnostic.AbsoluteFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InMessage.RealFilePath);
	}

	FString Message;
	if (!Path.IsEmpty())
	{
		Message = Path;
		if (InMessage.Line != -1)
		{
			if (InMessage.ColumnStart == InMessage.ColumnEnd)
			{
				Message += FString::Printf(TEXT(" (%d,%d)"), InMessage.Line, InMessage.ColumnStart);
			}
			else
			{
				Message += FString::Printf(TEXT(" (%d,%d-%d)"), InMessage.Line, InMessage.ColumnStart, InMessage.ColumnEnd);
			}
		}
		Message += TEXT(": ");
	}
	Message += InMessage.Text;
	Diagnostic.Message = FText::FromString(Message);

	if (InOwner)
	{
		InOwner->GetCompileMessageDelegate().Broadcast(Diagnostic);
	}

	return Diagnostic.Level;
}

void UOptimusComputeGraph::OnKernelCompilationComplete(int32 InKernelIndex, FComputeKernelCompileResults const& InCompileResults)
{
	// Find the Optimus objects from the raw kernel index.
	if (KernelToNode.IsValidIndex(InKernelIndex))
	{
		UOptimusDeformer* Owner = Cast<UOptimusDeformer>(GetOuter());
		
		// Make sure the node hasn't been GC'd.
		if (UOptimusNode* Node = const_cast<UOptimusNode*>(KernelToNode[InKernelIndex].Get()))
		{
			FString FriendlyName = Owner->GetName() / GetFName().GetPlainNameString() / Node->GetDisplayName().ToString();
			EOptimusDiagnosticLevel DiagnosticLevel = EOptimusDiagnosticLevel::None;

			for (FComputeKernelCompileMessage const& CompileOutputMessage : InCompileResults.Messages)
			{
				EOptimusDiagnosticLevel MessageDiagnosticLevel = ProcessCompilationMessage(Owner, Node, FriendlyName, CompileOutputMessage);
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
