// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InstancedStruct.h"
#include "IObjectChooser.h"

#include "ChooserPropertyAccess.generated.h"

#if WITH_EDITOR
struct FBindingChainElement;
#endif

UINTERFACE(MinimalAPI)
class UHasContextClass : public UInterface
{
	GENERATED_BODY()
};

DECLARE_MULTICAST_DELEGATE(FContextClassChanged)


UENUM()
enum class EObjectChooserResultType
{
	ObjectResult,
	ClassResult,
};

class IHasContextClass
{
	GENERATED_BODY()
public:
	FContextClassChanged OnContextClassChanged;
	virtual TConstArrayView<FInstancedStruct> GetContextData() const { return TConstArrayView<FInstancedStruct>(); }
};

USTRUCT()
struct FChooserPropertyBinding 
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FName> PropertyBindingChain;
	
	UPROPERTY()
	int ContextIndex = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString DisplayName;
#endif
};

USTRUCT()
struct FChooserEnumPropertyBinding : public FChooserPropertyBinding
{
	GENERATED_BODY()
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<const UEnum> Enum = nullptr;
#endif
};

USTRUCT()
struct FChooserObjectPropertyBinding : public FChooserPropertyBinding
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UClass> AllowedClass = nullptr;
#endif
};

USTRUCT()
struct FChooserStructPropertyBinding : public FChooserPropertyBinding
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UScriptStruct> StructType = nullptr;
#endif
};

DECLARE_MULTICAST_DELEGATE_OneParam(FChooserOutputObjectTypeChanged, const UClass* OutputObjectType);
UENUM()
enum class EContextObjectDirection
{
	Read,
	Write,
	ReadWrite
};

USTRUCT()
struct FContextObjectTypeBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="Type")
	EContextObjectDirection Direction = EContextObjectDirection::Read;
};


USTRUCT()
struct FContextObjectTypeClass : public FContextObjectTypeBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Type")
	TObjectPtr<UClass> Class;
};

USTRUCT()
struct FContextObjectTypeStruct : public FContextObjectTypeBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="Type")
	TObjectPtr<UScriptStruct> Struct;
};


namespace UE::Chooser
{
	CHOOSER_API bool ResolvePropertyChain(const void*& Container, const UStruct*& StructType, const TArray<FName>& PropertyBindingChain);
	CHOOSER_API bool ResolvePropertyChain(FChooserEvaluationContext& Context, const FChooserPropertyBinding& Binding, const void*& OutContainer, const UStruct*& OutStructType);

#if WITH_EDITOR
	CHOOSER_API void CopyPropertyChain(const TArray<FBindingChainElement>& InBindingChain, FChooserPropertyBinding& OutPropertyBinding);
#endif
}