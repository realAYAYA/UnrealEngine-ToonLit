// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigCompilerDetails.h"
#include "Widgets/SWidget.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ControlRig.h"
#include "IPropertyUtilities.h"
#include "IPythonScriptPlugin.h"
#include "RigVMPythonUtils.h"
#include "ControlRigVisualGraphUtils.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "RigVMCompiler/RigVMCodeGenerator.h"

#define LOCTEXT_NAMESPACE "ControlRigCompilerDetails"

#if UE_RIGVM_DEBUG_EXECUTION
//CVar to specify if we should create a float control for each curve in the curve container
//By default we don't but it may be useful to do so for debugging
static TAutoConsoleVariable<int32> CVarControlRigDebugVMExecutionStringEnabled(
	TEXT("ControlRig.DebugVMExecutionStringEnabled"),
	0,
	TEXT("If nonzero we allow to copy the execution of a VM execution."),
	ECVF_Default);
#endif

void FRigVMCompileSettingsDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		InStructPropertyHandle->CreatePropertyValueWidget()
	];

	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	ensure(Objects.Num() == 1); // This is in here to ensure we are only showing the modifier details in the blueprint editor

	for (UObject* Object : Objects)
	{
		if (Object->IsA<UControlRigBlueprint>())
		{
			BlueprintBeingCustomized = Cast<UControlRigBlueprint>(Object);
		}
	}
}

void FRigVMCompileSettingsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (InStructPropertyHandle->IsValidHandle())
	{
		uint32 NumChildren = 0;
		InStructPropertyHandle->GetNumChildren(NumChildren);

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			StructBuilder.AddProperty(InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef());
		}

		StructBuilder.AddCustomRow(LOCTEXT("MemoryInspection", "Memory Inspection"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Memory Inspection")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FRigVMCompileSettingsDetails::OnInspectMemory, ERigVMMemoryType::Literal)
					.ContentPadding(FMargin(2))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("InspectLiteralMemory", "Inspect Literal Memory"))
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FRigVMCompileSettingsDetails::OnInspectMemory, ERigVMMemoryType::Work)
					.ContentPadding(FMargin(2))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("InspectWorkMemory", "Inspect Work Memory"))
					]
				]
			];

		TSharedPtr<SVerticalBox> DebugBox;
		StructBuilder.AddCustomRow(LOCTEXT("DebuggingTools", "Debugging Tools"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Debugging")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SAssignNew(DebugBox, SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FRigVMCompileSettingsDetails::OnCopyASTClicked)
					.ContentPadding(FMargin(2))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("CopyASTToClipboard", "Copy AST Graph"))
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FRigVMCompileSettingsDetails::OnCopyByteCodeClicked)
					.ContentPadding(FMargin(2))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("CopyByteCodeToClipboard", "Copy ByteCode"))
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FRigVMCompileSettingsDetails::OnCopyGeneratedCodeClicked)
					.ContentPadding(FMargin(2))
					.Visibility_Lambda([]()
					{
						return UControlRig::AreNativizedVMsDisabled() ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("CopyGeneratedCodeToClipboard", "Copy Nativized C++ Code"))
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FRigVMCompileSettingsDetails::OnCopyHierarchyGraphClicked)
					.ContentPadding(FMargin(2))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("CopyHierarchyGraphToClipboard", "Copy Hierarchy Graph"))
					]
				]
			];
#if UE_RIGVM_DEBUG_EXECUTION
		if (CVarControlRigDebugVMExecutionStringEnabled->GetBool() == true)
		{
			DebugBox->AddSlot()
			[
				SNew(SButton)
				.OnClicked(this, &FRigVMCompileSettingsDetails::OnCopyVMExecutionClicked)
				.ContentPadding(FMargin(2))
				.Content()
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.Text(LOCTEXT("CopyVMExecution", "Copy VM Execution"))
				]
			];
		}
#endif
	}
}

FReply FRigVMCompileSettingsDetails::OnInspectMemory(ERigVMMemoryType InMemoryType)
{
	if (BlueprintBeingCustomized)
	{
		if(UControlRig* DebuggedRig = Cast<UControlRig>(BlueprintBeingCustomized->GetObjectBeingDebugged()))
		{
			if(FRigVMMemoryStorageStruct* MemoryStorage = DebuggedRig->GetMemoryByType(InMemoryType))
			{
				TArray<FRigVMMemoryStorageStruct*> InStructs = { MemoryStorage };
				BlueprintBeingCustomized->RequestInspectMemoryStorage(InStructs);
			}
		}
	}
	return FReply::Handled();
}

FReply FRigVMCompileSettingsDetails::OnCopyASTClicked()
{
	if (BlueprintBeingCustomized)
	{
		if (BlueprintBeingCustomized->GetDefaultModel())
		{
			FString DotContent = BlueprintBeingCustomized->GetDefaultModel()->GetRuntimeAST()->DumpDot();
			FPlatformApplicationMisc::ClipboardCopy(*DotContent);
		}
	}
	return FReply::Handled();
}

FReply FRigVMCompileSettingsDetails::OnCopyByteCodeClicked()
{
	if (BlueprintBeingCustomized)
	{
		if (BlueprintBeingCustomized->GetDefaultModel())
		{
			if(UControlRig* ControlRig = Cast<UControlRig>(BlueprintBeingCustomized->GetObjectBeingDebugged()))
			{
				FString ByteCodeContent = ControlRig->GetVM()->DumpByteCodeAsText(ControlRig->GetRigVMExtendedExecuteContext());
				FPlatformApplicationMisc::ClipboardCopy(*ByteCodeContent);
			}
		}
	}
	return FReply::Handled();
}

FReply FRigVMCompileSettingsDetails::OnCopyHierarchyGraphClicked()
{
	if (BlueprintBeingCustomized)
	{
		if(UControlRig* ControlRig = Cast<UControlRig>(BlueprintBeingCustomized->GetObjectBeingDebugged()))
		{
			FName EventName = FRigUnit_BeginExecution::EventName;
			if(!ControlRig->GetEventQueue().IsEmpty())
			{
				EventName = ControlRig->GetEventQueue()[0];
			}
			
			const FString DotGraphContent = FControlRigVisualGraphUtils::DumpRigHierarchyToDotGraph(ControlRig->GetHierarchy(), EventName);
			FPlatformApplicationMisc::ClipboardCopy(*DotGraphContent);
		}
	}
	return FReply::Handled();
}

FReply FRigVMCompileSettingsDetails::OnCopyGeneratedCodeClicked()
{
	if (BlueprintBeingCustomized)
	{
		const FString ClassName = FString::Printf(TEXT("%sVM"), *BlueprintBeingCustomized->GetName());
		if (BlueprintBeingCustomized->GeneratedClass)
		{
			if (UControlRig* CDO = Cast<UControlRig>(BlueprintBeingCustomized->GeneratedClass->GetDefaultObject()))
			{
				if(CDO->GetVM())
				{
					CDO->GetVM()->ClearExternalVariables(CDO->GetRigVMExtendedExecuteContext());
					TArray<FRigVMExternalVariable> ExternalVariables = CDO->GetExternalVariablesImpl(false);
					for(const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
					{
						CDO->GetVM()->AddExternalVariable(CDO->GetRigVMExtendedExecuteContext(), ExternalVariable);
					}
					
					FRigVMExtendedExecuteContext& CDOContext = CDO->GetRigVMExtendedExecuteContext();

					FRigVMCodeGenerator CodeGenerator(ClassName,
						TEXT("TestModule"), BlueprintBeingCustomized->GetDefaultModel(), CDO->GetVM(), CDOContext,
						CDO->GetPublicContextStruct(), BlueprintBeingCustomized->PinToOperandMap);
					const FString Content = CodeGenerator.DumpHeader(CDOContext) + TEXT("\r\n\r\n") + CodeGenerator.DumpSource(CDOContext);
					FPlatformApplicationMisc::ClipboardCopy(*Content);
				}
			}
		}
	}
	return FReply::Handled();
}

#if UE_RIGVM_DEBUG_EXECUTION
FReply FRigVMCompileSettingsDetails::OnCopyVMExecutionClicked()
{
	if (BlueprintBeingCustomized)
	{
		if (UControlRig* ControlRig = Cast<UControlRig>(BlueprintBeingCustomized->GetObjectBeingDebugged()))
		{
			FString DebugString = ControlRig->GetDebugExecutionString();
			FPlatformApplicationMisc::ClipboardCopy(*DebugString);
		}
	}
	return FReply::Handled();
}
#endif

#undef LOCTEXT_NAMESPACE
