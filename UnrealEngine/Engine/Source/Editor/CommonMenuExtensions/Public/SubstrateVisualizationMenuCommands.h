// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "SubstrateVisualizationData.h"

class FEditorViewportClient;
class FMenuBuilder;
class FUICommandInfo;
class FUICommandList;

class COMMONMENUEXTENSIONS_API FSubstrateVisualizationMenuCommands : public TCommands<FSubstrateVisualizationMenuCommands>
{
public:
	struct FSubstrateVisualizationRecord
	{
		FName Name;
		TSharedPtr<FUICommandInfo> Command;
		FSubstrateViewMode ViewMode = FSubstrateViewMode::MaterialProperties;
	};

	typedef TMultiMap<FName, FSubstrateVisualizationRecord> TSubstrateVisualizationModeCommandMap;
	typedef TSubstrateVisualizationModeCommandMap::TConstIterator TCommandConstIterator;

	FSubstrateVisualizationMenuCommands();

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
	bool AddCommandTypeToMenu(FMenuBuilder& Menu, const FSubstrateViewMode ViewMode) const;

	static void ChangeSubstrateVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);
	static bool IsSubstrateVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);

private:
	TSubstrateVisualizationModeCommandMap CommandMap;
};