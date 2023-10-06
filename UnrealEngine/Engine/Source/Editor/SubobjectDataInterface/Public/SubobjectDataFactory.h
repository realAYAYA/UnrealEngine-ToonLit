// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "SubobjectData.h"
#include "SubobjectDataHandle.h"

/** Options for creating a new subobject data via the factory */
struct SUBOBJECTDATAINTERFACE_API FCreateSubobjectParams
{
	UObject* Context = nullptr;
	
	FSubobjectDataHandle ParentHandle = FSubobjectDataHandle::InvalidHandle;
	
	bool bIsInheritedSCS = false;
};

/**
* A subobject data factory will handle the creation of subobject data. 
* To create a subobject data factory, override this class and register it 
* with the subobject data subsystem. 
*/
class SUBOBJECTDATAINTERFACE_API ISubobjectDataFactory
{
public:

	static FName GetDefaultFactoryID()
	{
		static FName ID(TEXT("DefaultSubobjectDataFactory"));
		return ID;
	}

	virtual FName GetID() const = 0;
	
	virtual TSharedPtr<FSubobjectData> CreateSubobjectData(const FCreateSubobjectParams& Params) = 0;

	virtual bool ShouldCreateSubobjectData(const FCreateSubobjectParams& Params) const = 0;
};

/**
 * The factory manager keeps a map of all registered factories that can be used
 * to spawn Subobject Data. To add your own factory, create a subclass of ISubobjectDataFactory.
 *
 * See BaseSubobjectDataFactory.h for an example.
 */
class SUBOBJECTDATAINTERFACE_API FSubobjectFactoryManager
{
public:
	explicit FSubobjectFactoryManager() = default;
	~FSubobjectFactoryManager() = default;

	/***
	 * Map the given factory to this manager with its ID if it does not already
	 * exist in the list
	 * 
	 * @return True if the factory was registered successfully
	 */
	bool RegisterFactory(const TSharedPtr<ISubobjectDataFactory>& Factory)
	{
		if(Factory.IsValid())
		{
			const FName ID = Factory->GetID();
			if (!IsFactoryRegistered(ID))
			{
				RegisteredFactories.Add(ID, Factory);
				return true;
			}
		}
		return false;
	}

	/**
	 * Remove the factory with the given ID from the registered map.
	 * @return True if it was removed. 
	 */
	bool UnregisterFactory(FName ID)
	{
		return RegisteredFactories.Remove(ID) > 0;
	}

	/** returns true if the given factory ID is registered */
	bool IsFactoryRegistered(FName ID) const 
	{
		return RegisteredFactories.Contains(ID);
	}

	/**
	 * Find a factory that returns true from ShouldCreateSubobjectData based on
	 * the given parameters. If none are found, then the default will be returned. 
	 */
	ISubobjectDataFactory* FindFactoryToUse(const FCreateSubobjectParams& Params)
	{
		for(const TPair<FName, TSharedPtr<ISubobjectDataFactory>>& Pair : RegisteredFactories)
		{
			const TSharedPtr<ISubobjectDataFactory>& Factory = Pair.Value;
			if(Factory->ShouldCreateSubobjectData(Params))
			{
				return Factory.Get();
			}
		}
		
		TSharedPtr<ISubobjectDataFactory>* Default = RegisteredFactories.Find(ISubobjectDataFactory::GetDefaultFactoryID());
		return Default ? (*Default).Get() : nullptr;
	}
	
private:
	/** Map of all registered factories */
	TMap<FName, TSharedPtr<ISubobjectDataFactory>> RegisteredFactories;
};