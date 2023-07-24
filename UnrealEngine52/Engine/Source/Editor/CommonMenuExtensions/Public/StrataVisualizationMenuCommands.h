// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FEditorViewportClient;
class FMenuBuilder;
class FUICommandInfo;
class FUICommandList;

class COMMONMENUEXTENSIONS_API FStrataVisualizationMenuCommands : public TCommands<FStrataVisualizationMenuCommands>
{
public:
	enum class FStrataVisualizationType : uint8
	{
		MaterialProperties,
		MaterialCount,
		AdvancedMaterialProperties,
	};

	struct FStrataVisualizationRecord
	{
		FName Name;
		TSharedPtr<FUICommandInfo> Command;
		FStrataVisualizationType Type = FStrataVisualizationType::MaterialProperties;
	};

	typedef TMultiMap<FName, FStrataVisualizationRecord> TStrataVisualizationModeCommandMap;
	typedef TStrataVisualizationModeCommandMap::TConstIterator TCommandConstIterator;

	FStrataVisualizationMenuCommands();

	TCommandConstIterator CreateCommandConstIterator() const;

	static void BuildVisualisationSubMenu(FMenuBuilder& Menu);

	virtual void RegisterCommands() override;

	void BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const;
	
	inline bool IsPopulated() const
	{
		return CommandMap.Num() > 0;
	}

private:
	void BuildCommandMap();
	bool AddCommandTypeToMenu(FMenuBuilder& Menu, const FStrataVisualizationType Type) const;

	static void ChangeStrataVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);
	static bool IsStrataVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);

private:
	TStrataVisualizationModeCommandMap CommandMap;
};