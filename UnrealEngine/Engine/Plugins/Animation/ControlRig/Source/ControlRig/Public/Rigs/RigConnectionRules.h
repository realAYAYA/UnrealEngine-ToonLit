// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "UObject/StructOnScope.h"
#include "RigConnectionRules.generated.h"

struct FRigBaseElement;
struct FRigConnectionRule;
class FRigElementKeyRedirector;
class URigHierarchy;
struct FRigModuleInstance;
struct FRigBaseElement;
struct FRigTransformElement;
struct FRigConnectorElement;

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigConnectionRuleStash
{
	GENERATED_BODY()

	FRigConnectionRuleStash();
	FRigConnectionRuleStash(const FRigConnectionRule* InRule);

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);
	
	friend uint32 GetTypeHash(const FRigConnectionRuleStash& InRuleStash);

	bool IsValid() const;
	UScriptStruct* GetScriptStruct() const;
	TSharedPtr<FStructOnScope> Get() const;
	const FRigConnectionRule* Get(TSharedPtr<FStructOnScope>& InOutStorage) const;

	bool operator == (const FRigConnectionRuleStash& InOther) const;

	bool operator != (const FRigConnectionRuleStash& InOther) const
	{
		return !(*this == InOther);
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Rule)
	FString ScriptStructPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Rule)
	FString ExportedText;
};

struct CONTROLRIG_API FRigConnectionRuleInput
{
public:
	
	FRigConnectionRuleInput()
	: Hierarchy(nullptr)
	, Module(nullptr)
	, Redirector(nullptr)
	{
	}

	const URigHierarchy* GetHierarchy() const
	{
		return Hierarchy;
	}
	
	const FRigModuleInstance* GetModule() const
	{
		return Module;
	}
	
	const FRigElementKeyRedirector* GetRedirector() const
	{
		return Redirector;
	}

	const FRigConnectorElement* FindPrimaryConnector(FText* OutErrorMessage = nullptr) const;
   	TArray<const FRigConnectorElement*> FindSecondaryConnectors(bool bOptional, FText* OutErrorMessage = nullptr) const;

	const FRigTransformElement* ResolveConnector(const FRigConnectorElement* InConnector, FText* OutErrorMessage) const;
	const FRigTransformElement* ResolvePrimaryConnector(FText* OutErrorMessage = nullptr) const;

private:

	const URigHierarchy* Hierarchy;
	const FRigModuleInstance* Module;
	const FRigElementKeyRedirector* Redirector;

	friend class UModularRigRuleManager;
};

USTRUCT(meta=(Hidden))
struct CONTROLRIG_API FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigConnectionRule() {}
	virtual ~FRigConnectionRule() {}

	virtual UScriptStruct* GetScriptStruct() const { return FRigConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const;
};

USTRUCT(BlueprintType, DisplayName="And Rule")
struct CONTROLRIG_API FRigAndConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigAndConnectionRule()
	{}

	template<typename TypeA, typename TypeB>
	FRigAndConnectionRule(const TypeA& InA, const TypeB& InB)
	{
		ChildRules.Emplace(&InA);
		ChildRules.Emplace(&InB);
	}

	virtual ~FRigAndConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigAndConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	TArray<FRigConnectionRuleStash> ChildRules;
};

USTRUCT(BlueprintType, DisplayName="Or Rule")
struct CONTROLRIG_API FRigOrConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigOrConnectionRule()
	{}

	template<typename TypeA, typename TypeB>
	FRigOrConnectionRule(const TypeA& InA, const TypeB& InB)
	{
		ChildRules.Emplace(&InA);
		ChildRules.Emplace(&InB);
	}

	virtual ~FRigOrConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigOrConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	TArray<FRigConnectionRuleStash> ChildRules;
};

USTRUCT(BlueprintType, DisplayName="Type Rule")
struct CONTROLRIG_API FRigTypeConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigTypeConnectionRule()
		: ElementType(ERigElementType::Socket)
	{}

	FRigTypeConnectionRule(ERigElementType InElementType)
	: ElementType(InElementType)
	{}

	virtual ~FRigTypeConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigTypeConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	ERigElementType ElementType;
};

USTRUCT(BlueprintType, DisplayName="Tag Rule")
struct CONTROLRIG_API FRigTagConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigTagConnectionRule()
		: Tag(NAME_None)
	{}

	FRigTagConnectionRule(const FName& InTag)
	: Tag(InTag)
	{}

	virtual ~FRigTagConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigTagConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	FName Tag;
};

USTRUCT(BlueprintType, DisplayName="Child of Primary")
struct CONTROLRIG_API FRigChildOfPrimaryConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigChildOfPrimaryConnectionRule()
	{}

	virtual ~FRigChildOfPrimaryConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigChildOfPrimaryConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;
};
/*
USTRUCT(BlueprintType, DisplayName="On Chain Rule")
struct CONTROLRIG_API FRigOnChainRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigOnChainRule()
	: MinNumBones(2)
	, MaxNumBones(0)
	{}

	FRigOnChainRule(int32 InMinNumBones = 2, int32 InMaxNumBones = 0)
	: MinNumBones(InMinNumBones)
	, MaxNumBones(InMaxNumBones)
	{}

	virtual ~FRigOnChainRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigOnChainRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MinNumBones;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MaxNumBones;
};
*/