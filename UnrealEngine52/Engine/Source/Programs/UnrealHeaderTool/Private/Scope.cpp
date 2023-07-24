// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scope.h"
#include "UnrealHeaderTool.h"
#include "ParserHelper.h"
#include "UnrealTypeDefinitionInfo.h"
#include "ClassMaps.h"

FScope::FScope(FScope* InParent)
	: Parent(InParent)
{ }

FScope::FScope()
	: Parent(nullptr)
{

}

void FScope::AddType(FUnrealFieldDefinitionInfo& Type)
{
	TypeMap.Add(Type.GetFName(), &Type);
}

void FScope::GatherTypes(TArray<FUnrealFieldDefinitionInfo*>& Types)
{
	for (TPair<FName, FUnrealFieldDefinitionInfo*>& TypePair : TypeMap)
	{
		FUnrealFieldDefinitionInfo* FieldDef = TypePair.Value;
		if (FUnrealClassDefinitionInfo* ClassDef = UHTCast<FUnrealClassDefinitionInfo>(FieldDef))
		{
			// Inner scopes.
			ClassDef->GetScope()->GatherTypes(Types);
			Types.Add(ClassDef);
		}
		else if (FUnrealEnumDefinitionInfo* EnumDef = UHTCast<FUnrealEnumDefinitionInfo>(FieldDef))
		{
			Types.Add(EnumDef);
		}
		else if (FUnrealScriptStructDefinitionInfo* ScriptStructDef = UHTCast<FUnrealScriptStructDefinitionInfo>(FieldDef))
		{
			Types.Add(ScriptStructDef);
		}
		else if (FUnrealFunctionDefinitionInfo* FunctionDef = UHTCast<FUnrealFunctionDefinitionInfo>(FieldDef))
		{
			if (FunctionDef->IsDelegateFunction())
			{
				bool bAdded = false;
				if (FunctionDef->GetSuperFunction() == nullptr)
				{
					Types.Add(FunctionDef);
					bAdded = true;
				}
				check(bAdded);
			}
		}
	}
}

FUnrealFieldDefinitionInfo* FScope::FindTypeByName(FName Name)
{
	if (!Name.IsNone())
	{
		TDeepScopeTypeIterator<FUnrealFieldDefinitionInfo, false> TypeIterator(this);

		while (TypeIterator.MoveNext())
		{
			FUnrealFieldDefinitionInfo* Type = *TypeIterator;
			if (Type->GetFName() == Name)
			{
				return Type;
			}
		}
	}

	return nullptr;
}

const FUnrealFieldDefinitionInfo* FScope::FindTypeByName(FName Name) const
{
	if (!Name.IsNone())
	{
		TScopeTypeIterator<FUnrealFieldDefinitionInfo, true> TypeIterator = GetTypeIterator();

		while (TypeIterator.MoveNext())
		{
			FUnrealFieldDefinitionInfo* Type = *TypeIterator;
			if (Type->GetFName() == Name)
			{
				return Type;
			}
		}
	}

	return nullptr;
}

bool FScope::IsFileScope() const
{
	return Parent == nullptr;
}

bool FScope::ContainsTypes() const
{
	return TypeMap.Num() > 0;
}

FFileScope* FScope::GetFileScope()
{
	FScope* CurrentScope = this;
	while (!CurrentScope->IsFileScope())
	{
		CurrentScope = const_cast<FScope*>(CurrentScope->GetParent());
	}

	return CurrentScope->AsFileScope();
}

FFileScope::FFileScope(FName InName, FUnrealSourceFile* InSourceFile)
	: SourceFile(InSourceFile), Name(InName)
{ }

void FFileScope::IncludeScope(FFileScope* IncludedScope)
{
	IncludedScopes.Add(IncludedScope);
}

FUnrealSourceFile* FFileScope::GetSourceFile() const
{
	return SourceFile;
}

FName FFileScope::GetName() const
{
	return Name;
}

FName FStructScope::GetName() const
{
	return StructDef.GetFName();
}
