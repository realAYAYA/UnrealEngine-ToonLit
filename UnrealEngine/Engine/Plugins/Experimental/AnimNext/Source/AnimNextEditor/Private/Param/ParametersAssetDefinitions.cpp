// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametersAssetDefinitions.h"
#include "AnimNextParameterBlockEditor.h"
#include "AnimNextParameterLibraryEditor.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterLibrary.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

EAssetCommandResult UAssetDefinition_AnimNextParameterLibrary::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UAnimNextParameterLibrary* Asset : OpenArgs.LoadObjects<UAnimNextParameterLibrary>())
	{
		TSharedRef<UE::AnimNext::Editor::FParameterLibraryEditor> Editor = MakeShared<UE::AnimNext::Editor::FParameterLibraryEditor>();
		Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
	}

	return EAssetCommandResult::Handled;
}

FText UAssetDefinition_AnimNextParameter::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextParameter* Parameter = CastChecked<UAnimNextParameter>(Object);
	if(UAnimNextParameterLibrary* Library = CastChecked<UAnimNextParameterLibrary>(Object->GetOuter()))
	{
		return FText::Format(LOCTEXT("LibraryParameterDisplayFormat", "{0}.{1}"), FText::FromString(Library->GetName()), FText::FromString(Parameter->GetName()));
	}

	return FText::FromString(Parameter->GetName());
}

EAssetCommandResult UAssetDefinition_AnimNextParameterBlock::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UAnimNextParameterBlock* Asset : OpenArgs.LoadObjects<UAnimNextParameterBlock>())
	{
		TSharedRef<UE::AnimNext::Editor::FParameterBlockEditor> Editor = MakeShared<UE::AnimNext::Editor::FParameterBlockEditor>();
		Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
	}

	return EAssetCommandResult::Handled;
}

FText UAssetDefinition_AnimNextParameterBlockBinding::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextParameterBlockBinding* Binding = CastChecked<UAnimNextParameterBlockBinding>(Object);
	return FText::Format(LOCTEXT("BindingDisplayFormat", "{0} Binding"), FText::FromName(Binding->ParameterName));
}

#undef LOCTEXT_NAMESPACE