// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "Templates/SharedPointer.h"

class IDetailTreeNode;
class IPropertyHandle;
class SWidget;

/**
 * A static class containing utility functions for widgets.
 */
class FAvaRundownPageRemoteControlWidgetUtils final
{
public:
	enum class EFindNodeMethod : uint8
	{
		Name, // Find a node by providing its property name
		Path // Find a node by providing a property path
	};

public:
	static TSharedRef<SWidget> CreateNodeValueWidget(const TSharedPtr<IDetailTreeNode>& InNode);
	static bool FindPropertyHandleRecursive(const TSharedPtr<IPropertyHandle>& InPropertyHandle, const FString& InPropertyNameOrPath, EFindNodeMethod InFindMethod);
	static TSharedPtr<IDetailTreeNode> FindTreeNodeRecursive(const TSharedRef<IDetailTreeNode>& InRootNode, const FString& InPropertyNameOrPath, EFindNodeMethod InFindMethod);
	static TSharedPtr<IDetailTreeNode> FindNode(const TArray<TSharedRef<IDetailTreeNode>>& InRootNodes, const FString& InQualifiedPropertyName, EFindNodeMethod InFindMethod);
};
