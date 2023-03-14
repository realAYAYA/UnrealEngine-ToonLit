// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

struct FBasePropertyOverrideNode
{
public:
	FBasePropertyOverrideNode(FName InParameterName, FName InParameterID, float InParameterValue, bool bInOverride) :
		ParameterName(InParameterName),
		ParameterID(InParameterID),
		ParameterValue(InParameterValue),
		bOverride(bInOverride)
	{

	}
	FName ParameterName;
	FName ParameterID;
	float ParameterValue;
	bool bOverride;

	TArray<TSharedRef<FBasePropertyOverrideNode>>* Children;
};

struct FStaticMaterialLayerParameterNode
{
public:
	FStaticMaterialLayerParameterNode(FName InParameterName, FString InParameterValue, bool bInOverride):
		ParameterName(InParameterName),
		ParameterValue(InParameterValue),
		bOverride(bInOverride)
	{

	}
	FName ParameterName;
	FString ParameterValue;
	bool bOverride;
};

struct FStaticSwitchParameterNode
{
public:
	FStaticSwitchParameterNode(FName InParameterName, bool InParameterValue, bool bInOverride) :
		ParameterName(InParameterName),
		ParameterValue(InParameterValue),
		bOverride(bInOverride)
	{

	}
	FName ParameterName;
	bool ParameterValue;
	bool bOverride;

	TArray<TSharedRef<FStaticSwitchParameterNode>>* Children;
};

struct FStaticComponentMaskParameterNode
{
public:
	FStaticComponentMaskParameterNode(FName InParameterName, bool InR, bool InG, bool InB, bool InA, bool bInOverride) :
		ParameterName(InParameterName),
		R(InR),
		G(InG),
		B(InB),
		A(InA),
		bOverride(bInOverride)
	{

	}
	FName ParameterName;
	bool R;
	bool G;
	bool B;
	bool A;
	bool bOverride;
};

typedef TSharedRef<FBasePropertyOverrideNode, ESPMode::ThreadSafe> FBasePropertyOverrideNodeRef;

typedef TSharedRef<FStaticMaterialLayerParameterNode, ESPMode::ThreadSafe> FStaticMaterialLayerParameterNodeRef;

typedef TSharedRef<FStaticSwitchParameterNode, ESPMode::ThreadSafe> FStaticSwitchParameterNodeRef;

typedef TSharedRef<FStaticComponentMaskParameterNode, ESPMode::ThreadSafe> FStaticComponentMaskParameterNodeRef;

typedef TSharedRef<struct FAnalyzedMaterialNode, ESPMode::ThreadSafe> FAnalyzedMaterialNodeRef;

typedef TSharedPtr<struct FAnalyzedMaterialNode, ESPMode::ThreadSafe> FAnalyzedMaterialNodePtr;

struct FAnalyzedMaterialNode
{
public:
	/**
	* Add the given node to our list of children for this material (this node will keep a strong reference to the instance)
	*/
	FAnalyzedMaterialNodeRef* AddChildNode(FAnalyzedMaterialNodeRef InChildNode)
	{
		ChildNodes.Add(InChildNode);
		return &ChildNodes.Last();
	}

	/**
	* @return The node entries for the material's children
	*/
	TArray<FAnalyzedMaterialNodeRef>& GetChildNodes()
	{
		return ChildNodes;
	}

	TArray<FAnalyzedMaterialNodeRef>* GetChildNodesPtr()
	{
		return &ChildNodes;
	}

	int ActualNumberOfChildren() const
	{
		return ChildNodes.Num();
	}

	int TotalNumberOfChildren() const
	{
		int32 TotalChildren = 0;

		for(int i = 0; i < ChildNodes.Num(); ++i)
		{
			TotalChildren += ChildNodes[i]->TotalNumberOfChildren();
		}

		return TotalChildren + ChildNodes.Num();
	}

	FBasePropertyOverrideNodeRef FindBasePropertyOverride(FName ParameterName)
	{
		FBasePropertyOverrideNodeRef* BasePropertyOverride = BasePropertyOverrides.FindByPredicate([&](const FBasePropertyOverrideNodeRef& Entry) { return Entry->ParameterName == ParameterName; });
		check(BasePropertyOverride != nullptr);
		return *BasePropertyOverride;
	}

	FStaticMaterialLayerParameterNodeRef FindMaterialLayerParameter(FName ParameterName)
	{
		FStaticMaterialLayerParameterNodeRef* MaterialLayerParameter = MaterialLayerParameters.FindByPredicate([&](const FStaticMaterialLayerParameterNodeRef& Entry) { return Entry->ParameterName == ParameterName; });
		check(MaterialLayerParameter != nullptr);
		return *MaterialLayerParameter;
	}

	FStaticSwitchParameterNodeRef FindStaticSwitchParameter(FName ParameterName)
	{
		FStaticSwitchParameterNodeRef* StaticSwitchParameter = StaticSwitchParameters.FindByPredicate([&](const FStaticSwitchParameterNodeRef& Entry) { return Entry->ParameterName == ParameterName; });
		check(StaticSwitchParameter != nullptr);
		return *StaticSwitchParameter;
	}

	FStaticComponentMaskParameterNodeRef FindStaticComponentMaskParameter(FName ParameterName)
	{
		FStaticComponentMaskParameterNodeRef* StaticComponentMaskParameter = StaticComponentMaskParameters.FindByPredicate([&](const FStaticComponentMaskParameterNodeRef& Entry) { return Entry->ParameterName == ParameterName; });
		check(StaticComponentMaskParameter != nullptr);
		return *StaticComponentMaskParameter;
	}

	FString Path;
	FSoftObjectPath ObjectPath;
	FAnalyzedMaterialNodePtr Parent;
	FAssetData AssetData;

	TArray<FBasePropertyOverrideNodeRef> BasePropertyOverrides;
	TArray<FStaticMaterialLayerParameterNodeRef> MaterialLayerParameters;
	TArray<FStaticSwitchParameterNodeRef> StaticSwitchParameters;
	TArray<FStaticComponentMaskParameterNodeRef> StaticComponentMaskParameters;

protected:
	TArray<FAnalyzedMaterialNodeRef> ChildNodes;
};