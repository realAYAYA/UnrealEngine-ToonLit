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

class COMMONMENUEXTENSIONS_API FNaniteVisualizationMenuCommands : public TCommands<FNaniteVisualizationMenuCommands>
{
public:
	enum class FNaniteVisualizationType : uint8
	{
		Overview,
		Standard,
		Advanced,
	};

	struct FNaniteVisualizationRecord
	{
		FName Name;
		TSharedPtr<FUICommandInfo> Command;
		FNaniteVisualizationType Type;

		FNaniteVisualizationRecord()
		: Name()
		, Command()
		, Type(FNaniteVisualizationType::Overview)
		{
		}
	};

	typedef TMultiMap<FName, FNaniteVisualizationRecord> TNaniteVisualizationModeCommandMap;
	typedef TNaniteVisualizationModeCommandMap::TConstIterator TCommandConstIterator;

	FNaniteVisualizationMenuCommands();

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
	bool AddCommandTypeToMenu(FMenuBuilder& Menu, const FNaniteVisualizationType Type) const;

	static void ChangeNaniteVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);
	static bool IsNaniteVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);

private:
	TNaniteVisualizationModeCommandMap CommandMap;
};