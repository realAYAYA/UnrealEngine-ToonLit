// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FName;
class FString;
template <typename FuncType> class TFunctionRef;

class UConversationGraphNode;
class UConversationGraph;
class UConversationDatabase;
class UEdGraphPin;

class COMMONCONVERSATIONGRAPH_API FConversationCompiler
{
public:
	static int32 GetNumGraphs(UConversationDatabase* ConversationAsset);
	static UConversationGraph* GetGraphFromBank(UConversationDatabase* ConversationAsset, int32 Index);
	static void RebuildBank(UConversationDatabase* ConversationAsset);
	static UConversationGraph* AddNewGraph(UConversationDatabase* ConversationAsset, const FString& DesiredName);

	static int32 GetCompilerVersion();

	static void ScanAndRecompileOutOfDateCompiledConversations();

private:
	FConversationCompiler() {}

	// Creates a new graph but does not add it
	static UConversationGraph* CreateNewGraph(UConversationDatabase* ConversationAsset, FName GraphName);

	// Skips over knots.
	static void ForeachConnectedOutgoingConversationNode(UEdGraphPin* Pin, TFunctionRef<void(UConversationGraphNode*)> Predicate);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
