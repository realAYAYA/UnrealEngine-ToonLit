// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Misc/Paths.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorUtil.h"

class FDataflowEditorStyle final : public FSlateStyleSet
{
public:
	FDataflowEditorStyle() : FSlateStyleSet("DataflowEditorStyle")
	{
		const FVector2D Icon16x16(16.f, 16.f);
		const FVector2D Icon28x14(28.f, 14.f);
		const FVector2D Icon20x20(20.f, 20.f);
		const FVector2D Icon40x40(40.f, 40.f);
		const FVector2D Icon64x64(64.f, 64.f);

		const FVector2D ToolbarIconSize = Icon20x20;

		SetContentRoot(IPluginManager::Get().FindPlugin("Dataflow")->GetBaseDir() / TEXT("Resources"));
		Set("ClassIcon.Dataflow", new FSlateVectorImageBrush(RootToContentDir(TEXT("DataflowAsset_16.svg")), Icon16x16));
		Set("ClassThumbnail.Dataflow", new FSlateVectorImageBrush(RootToContentDir(TEXT("DataflowAsset_64.svg")), Icon64x64));

		Set("Dataflow.Render.Unknown", new FSlateImageBrush(RootToContentDir(TEXT("Slate/Switch_Undetermined_56x_28x.png")), Icon28x14));
		Set("Dataflow.Render.Disabled", new FSlateImageBrush(RootToContentDir(TEXT("Slate/Switch_OFF_56x_28x.png")), Icon28x14));
		Set("Dataflow.Render.Enabled", new FSlateImageBrush(RootToContentDir(TEXT("Slate/Switch_ON_56x_28x.png")), Icon28x14));

		Set("Dataflow.Cached.False", new FSlateImageBrush(RootToContentDir(TEXT("Slate/status_grey.png")), Icon16x16));
		Set("Dataflow.Cached.True", new FSlateImageBrush(RootToContentDir(TEXT("Slate/status_green.png")), Icon16x16));

		Set("Dataflow.SelectObject", new FSlateImageBrush(RootToContentDir(TEXT("Slate/Dataflow_SelectObject_40x.png")), Icon40x40));
		Set("Dataflow.SelectFace", new FSlateImageBrush(RootToContentDir(TEXT("Slate/Dataflow_SelectFace_40x.png")), Icon40x40));
		Set("Dataflow.SelectVertex", new FSlateImageBrush(RootToContentDir(TEXT("Slate/Dataflow_SelectVertex_40x.png")), Icon40x40));

		// @todo(brice) Remove Example Tools
		//const FString AttributeEditorPropertyName = "DataflowEditor." + FDataflowEditorCommandsImpl::BeginAttributeEditorToolIdentifier;
		//Set(*AttributeEditorPropertyName, new FSlateImageBrush(RootToContentDir(TEXT("Slate/Dataflow_SelectObject_20x.png")), Icon20x20));
		//const FString MeshSelectionPropertyName = "DataflowEditor." + FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier;
		//Set(*MeshSelectionPropertyName, new FSlateImageBrush(RootToContentDir(TEXT("Slate/Dataflow_SelectVertex_20x.png")), Icon20x20));

		FString PropertyNameString = "DataflowEditor." + FDataflowEditorCommandsImpl::AddWeightMapNodeIdentifier;
		Set(*PropertyNameString, new IMAGE_BRUSH_SVG("Icons/PaintMaps", ToolbarIconSize));

		DefaultMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
		VertexMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/Dataflow/DataflowVertexMaterial")));
		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FDataflowEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}


	/** Default Rendering Material for Mesh surfaces */
	UMaterial* DefaultMaterial = nullptr;
	UMaterial* VertexMaterial = nullptr;


public:

	static FDataflowEditorStyle& Get()
	{
		static FDataflowEditorStyle Inst;
		return Inst;
	}

};

