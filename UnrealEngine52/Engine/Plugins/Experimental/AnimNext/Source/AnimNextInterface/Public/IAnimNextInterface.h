// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IAnimNextInterface.generated.h"

class IAnimNextInterface;
namespace UE::AnimNext::Interface
{
struct FParam;
struct FContext;
}

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class ANIMNEXTINTERFACE_API UAnimNextInterface : public UInterface
{
	GENERATED_BODY()
};

// Empty anim interface to support 'any' type
class ANIMNEXTINTERFACE_API IAnimNextInterface
{
	GENERATED_BODY()

public:
	/**
	 * Gets data using context to retrieve input parameters and result
	 * @return false if type are incompatible, or if nested calls fail, otherwise true
	 */
	bool GetData(const UE::AnimNext::Interface::FContext& Context) const;

	/**
	 * Gets data and stores the value in OutResult, checking whether types are compatible
	* @return false if nested calls fail, otherwise true
	*/
	bool GetDataChecked(const UE::AnimNext::Interface::FContext& Context) const;

	/**
	 * Gets data and stores the value in OutResult.
	 * @return false if type are incompatible, or if nested calls fail, otherwise true
	 */
	bool GetData(const UE::AnimNext::Interface::FContext& Context, UE::AnimNext::Interface::FParam& OutResult) const;

	/**
	 * Gets data and stores the value in OutResult, checking whether types are compatible
	 * @return false if nested calls fail, otherwise true
	 */
	bool GetDataChecked(const UE::AnimNext::Interface::FContext& Context, UE::AnimNext::Interface::FParam& OutResult) const;

	/** Get the name of the return type */
	FName GetReturnTypeName() const
	{
		return GetReturnTypeNameImpl();
	}

	/** Get the struct of the return type, if any */
	const UScriptStruct* GetReturnTypeStruct() const
	{
		return GetReturnTypeStructImpl();
	}

private:
	/** Get data if the types are compatible */
	bool GetDataIfCompatibleInternal(const UE::AnimNext::Interface::FContext& InContext) const;
	
	/** Get a value from an anim interface with no dynamic or static type checking. Internal use only. */
	bool GetDataRawInternal(const UE::AnimNext::Interface::FContext& InContext) const;

protected:
	/** Get the return type name, used for dynamic type checking */
	virtual FName GetReturnTypeNameImpl() const = 0;

	/** Get the return type struct, used for dynamic type checking. If the result is not a struct, this should return nullptr */
	virtual const UScriptStruct* GetReturnTypeStructImpl() const { return nullptr; }
	
	/** Get the value for this interface. @return true if successful, false if unsuccessful. */
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const = 0;
};

