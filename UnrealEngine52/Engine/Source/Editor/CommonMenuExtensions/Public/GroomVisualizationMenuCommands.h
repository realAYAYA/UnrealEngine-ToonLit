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
enum class EGroomViewMode : uint8;

class COMMONMENUEXTENSIONS_API FGroomVisualizationMenuCommands : public TCommands<FGroomVisualizationMenuCommands>
{
public:
	struct FGroomVisualizationRecord
	{
		FName Name;
		TSharedPtr<FUICommandInfo> Command;
		EGroomViewMode Mode;
	};

	typedef TMultiMap<FName, FGroomVisualizationRecord> TGroomVisualizationModeCommandMap;
	typedef TGroomVisualizationModeCommandMap::TConstIterator TCommandConstIterator;

	FGroomVisualizationMenuCommands();

	TCommandConstIterator CreateCommandConstIterator() const;

	static void BuildVisualisationSubMenu(FMenuBuilder& Menu);
	static void BuildVisualisationSubMenuForGroomEditor(FMenuBuilder& Menu);

	virtual void RegisterCommands() override;

	void BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const;

	inline bool IsPopulated() const
	{
		return CommandMap.Num() > 0;
	}

private:
	void BuildCommandMap();
	bool AddCommandTypeToMenu(FMenuBuilder& Menu, const EGroomViewMode Type) const;

	static void InternalBuildVisualisationSubMenu(FMenuBuilder& Menu, bool bIsGroomEditor);
	static void ChangeGroomVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);
	static bool IsGroomVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);

private:
	TGroomVisualizationModeCommandMap CommandMap;
};