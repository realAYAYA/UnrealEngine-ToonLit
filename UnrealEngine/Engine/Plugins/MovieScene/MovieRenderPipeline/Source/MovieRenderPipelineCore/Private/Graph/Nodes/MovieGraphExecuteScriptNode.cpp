// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphExecuteScriptNode.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"

#if WITH_EDITOR
FText UMovieGraphExecuteScriptNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "ExecuteScriptNode_Description", "Execute Script");
}

FText UMovieGraphExecuteScriptNode::GetMenuCategory() const
{
	static const FText NodeCategory_Globals = NSLOCTEXT("MovieGraphNodes", "NodeCategory_Globals", "Globals");
	return NodeCategory_Globals;
}

FLinearColor UMovieGraphExecuteScriptNode::GetNodeTitleColor() const
{
	return FLinearColor(0.1f, 0.1f, 0.85f);
}

FSlateIcon UMovieGraphExecuteScriptNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon ExecuteScriptIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Log.TabIcon");

	OutColor = FLinearColor::White;
	return ExecuteScriptIcon;
}
#endif	// WITH_EDITOR

UMovieGraphScriptBase* UMovieGraphExecuteScriptNode::AllocateScriptInstance() const
{
	UClass* NewInstanceType = Script.TryLoadClass<UMovieGraphScriptBase>();
	if (NewInstanceType)
	{
		UMovieGraphScriptBase* NewInstance = NewObject<UMovieGraphScriptBase>(GetTransientPackage(), NewInstanceType);
		return NewInstance;
	}

	return nullptr;
}