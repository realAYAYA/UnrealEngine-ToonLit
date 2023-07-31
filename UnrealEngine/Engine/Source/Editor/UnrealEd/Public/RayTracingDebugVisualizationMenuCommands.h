// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/Commands.h"

class FEditorViewportClient;

class UNREALED_API FRayTracingDebugVisualizationMenuCommands : public TCommands<FRayTracingDebugVisualizationMenuCommands>
{
public:
	struct FRayTracingDebugVisualizationRecord
	{
		uint32 Index;
		FName Name;
		TSharedPtr<FUICommandInfo> Command;

		FRayTracingDebugVisualizationRecord()
			: Index(0),
				Name(),
				Command()
		{
		}
	};

	FRayTracingDebugVisualizationMenuCommands();

	static void BuildVisualisationSubMenu(FMenuBuilder& Menu);

	virtual void RegisterCommands() override;

	void BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const;

	static bool DebugModeShouldBeTonemapped(const FName& RayTracingDebugModeName);

private:
	void BuildCommandMap();

	void CreateRayTracingDebugVisualizationCommands();

	void AddRayTracingDebugVisualizationCommandsToMenu(FMenuBuilder& menu) const;

	static void ChangeRayTracingDebugVisualizationMode(const TSharedPtr<FEditorViewportClient>& Client, FName InName);
	static bool IsRayTracingDebugVisualizationModeSelected(const TSharedPtr<FEditorViewportClient>& Client, FName InName);

	TArray<FRayTracingDebugVisualizationRecord> RayTracingDebugVisualizationCommands;

	static TArray<FText> RayTracingDebugModeNames;
};