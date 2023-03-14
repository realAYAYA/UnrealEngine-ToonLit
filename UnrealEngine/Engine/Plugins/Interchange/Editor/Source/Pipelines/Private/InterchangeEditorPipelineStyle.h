// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IPluginManager.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * Implements the visual style of the text asset editor UI.
 */
class FInterchangeEditorPipelineStyle final
	: public FSlateStyleSet
{
public:

	/** Default constructor. */
	FInterchangeEditorPipelineStyle()
		: FSlateStyleSet("InterchangeEditorPipelineStyle")
	{
		const FVector2D Icon16x16(16.0f, 16.0f);

		const FString BaseDir = IPluginManager::Get().FindPlugin("InterchangeEditor")->GetBaseDir();
		SetContentRoot(BaseDir / TEXT("Content"));

		//GraphInspectorIcon icons
		FSlateImageBrush* LodBrush16 = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Interchange_Lod_Icon_16"), TEXT(".png")), Icon16x16);
		Set("SceneGraphIcon.LodGroup", LodBrush16);
		FSlateImageBrush* JointBrush16 = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Interchange_Joint_Icon_16"), TEXT(".png")), Icon16x16);
		Set("SceneGraphIcon.Joint", JointBrush16);
		FSlateImageBrush* StaticMeshBrush16 = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Interchange_StaticMesh_Icon_16"), TEXT(".png")), Icon16x16);
		Set("MeshIcon.Static", StaticMeshBrush16);
		FSlateImageBrush* SkeletalMeshBrush16 = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Interchange_SkeletalMesh_Icon_16"), TEXT(".png")), Icon16x16);
		Set("MeshIcon.Skinned", SkeletalMeshBrush16);

		//PipelineConfigurationIcon icons
		FSlateImageBrush* PipelineBrush16 = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Interchange_Pipeline_Icon_16"), TEXT(".png")), Icon16x16);
		Set("PipelineConfigurationIcon.Pipeline", PipelineBrush16);
		FSlateImageBrush* PipelineStackBrush16 = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Interchange_PipelineStack_Icon_16"), TEXT(".png")), Icon16x16);
		Set("PipelineConfigurationIcon.PipelineStack", PipelineStackBrush16);
		FSlateImageBrush* PipelineStackDefaultBrush16 = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Interchange_PipelineStackDefault_Icon_16"), TEXT(".png")), Icon16x16);
		Set("PipelineConfigurationIcon.PipelineStackDefault", PipelineStackDefaultBrush16);

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	/** Destructor. */
	~FInterchangeEditorPipelineStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
};
