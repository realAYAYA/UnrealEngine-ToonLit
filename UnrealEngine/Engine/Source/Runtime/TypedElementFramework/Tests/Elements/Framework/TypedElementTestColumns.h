// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementTestColumns.generated.h"

USTRUCT(meta = (DisplayName = "ColumnA"))
struct FTestColumnA final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnB"))
struct FTestColumnB final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnC"))
struct FTestColumnC final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnD"))
struct FTestColumnD final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnE"))
struct FTestColumnE final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnF"))
struct FTestColumnF final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "ColumnG"))
struct FTestColumnG final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TagA"))
struct FTestTagColumnA final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TagB"))
struct FTestTagColumnB final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TagC"))
struct FTestTagColumnC final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "TagD"))
struct FTestTagColumnD final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};