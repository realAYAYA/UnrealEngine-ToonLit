// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigPythonLogDetails.h"
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

#define LOCTEXT_NAMESPACE "ControlRigCompilerDetails"

void FControlRigPythonLogDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
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

void FControlRigPythonLogDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (InStructPropertyHandle->IsValidHandle())
	{
		uint32 NumChildren = 0;
		InStructPropertyHandle->GetNumChildren(NumChildren);

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			StructBuilder.AddProperty(InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef());
		}

		StructBuilder.AddCustomRow(LOCTEXT("Commands", "Python Commands"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Commands")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FControlRigPythonLogDetails::OnCopyPythonScriptClicked)
					.ContentPadding(FMargin(2))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("CopyPythonScript", "Copy Python Script"))
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SButton)
					.OnClicked(this, &FControlRigPythonLogDetails::OnRunPythonContextClicked)
					.ContentPadding(FMargin(2))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(LOCTEXT("RunPythonContext", "Run Python Context"))
					]
				]
			];
	}
}

FReply FControlRigPythonLogDetails::OnCopyPythonScriptClicked()
{
	if (BlueprintBeingCustomized)
	{
		FString NewName = BlueprintBeingCustomized->GetPathName();
		int32 DotIndex = NewName.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (DotIndex != INDEX_NONE)
		{
			NewName = NewName.Left(DotIndex);
		}
		
		TArray<FString> Commands = BlueprintBeingCustomized->GeneratePythonCommands(NewName);
		FString FullScript = FString::Join(Commands, TEXT("\n"));
		FPlatformApplicationMisc::ClipboardCopy(*FullScript);
	}
	return FReply::Handled();
}

FReply FControlRigPythonLogDetails::OnRunPythonContextClicked()
{
	if (BlueprintBeingCustomized)
	{
		FString BlueprintName = BlueprintBeingCustomized->GetPathName();
		int32 DotIndex = BlueprintName.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (DotIndex != INDEX_NONE)
		{
			BlueprintName = BlueprintName.Left(DotIndex);
		}
		
		TArray<FString> PyCommands = {
			TEXT("import unreal"),
			FString::Printf(TEXT("blueprint = unreal.load_object(name = '%s', outer = None)"), *BlueprintName),
			TEXT("library = blueprint.get_local_function_library()"),
			TEXT("library_controller = blueprint.get_controller(library)"),
			TEXT("hierarchy = blueprint.hierarchy"),
			TEXT("hierarchy_controller = hierarchy.get_controller()")};

		for (FString& Command : PyCommands)
		{
			RigVMPythonUtils::Print(BlueprintBeingCustomized->GetFName().ToString(), Command);
		
			// Run the Python commands
			IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
