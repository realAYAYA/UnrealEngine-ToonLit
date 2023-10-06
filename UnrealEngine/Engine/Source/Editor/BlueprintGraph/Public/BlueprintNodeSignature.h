// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"

class UEdGraphNode;

struct BLUEPRINTGRAPH_API FBlueprintNodeSignature
{
public:
	FBlueprintNodeSignature() {}
	FBlueprintNodeSignature(FString const& UserString);
	FBlueprintNodeSignature(TSubclassOf<UEdGraphNode> NodeClass);

	/**
	 * 
	 * 
	 * @param  NodeClass	
	 * @return 
	 */
	void SetNodeClass(TSubclassOf<UEdGraphNode> NodeClass);

	/**
	 * 
	 * 
	 * @param  SignatureObj	
	 * @return 
	 */
	void AddSubObject(FFieldVariant SignatureObj);

	/**
	 * 
	 * 
	 * @param  Value	
	 * @return 
	 */
	void AddKeyValue(FString const& KeyValue);

	/**
	 * 
	 * 
	 * @param  SignatureKey	
	 * @param  Value	
	 * @return 
	 */
	void AddNamedValue(FName SignatureKey, FString const& Value);

	/**
	 * 
	 * 
	 * @return 
	 */
	bool IsValid() const;

	/**
	 * 
	 * 
	 * @return 
	 */
	FString const& ToString() const;

	/**
	*
	*
	* @return
	*/
	FGuid const& AsGuid() const;

private:
	/**
	 * 
	 * 
	 * @return 
	 */
	void MarkDirty();

private:
	typedef TMap<FName, FString> FSignatureSet;
	FSignatureSet SignatureSet;

	mutable FGuid   CachedSignatureGuid;
	mutable FString CachedSignatureString;
};

