// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Engine/Engine.h"
#include "MetasoundAssetBase.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/Object.h"
#include "Templates/Function.h"


// Forward Declarations
class UAssetManager;


namespace Metasound
{
	/** Interface for an entry into the Metasound-UObject Registry. 
	 *
	 * An entry provides information linking a FMetasoundFrontendClassInterface to a UClass.
	 * It also provides methods for accessing the FMetasoundAssetBase from a UObject.
	 */
	class IMetasoundUObjectRegistryEntry
	{
	public:
		virtual ~IMetasoundUObjectRegistryEntry() = default;

		/** Interface version associated with this entry. */
		virtual const FMetasoundFrontendVersion& GetInterfaceVersion() const = 0;

		/** UClass associated with this entry. */
		virtual UClass* GetUClass() const = 0;

		/** Returns true if the UObject is of a Class which is a child of this UClass associated with this entry. */
		virtual bool IsChildClass(const UObject* InObject) const = 0;

		/** Returns true if the UClass is a child of this UClass associated with this entry. */
		virtual bool IsChildClass(const UClass* InClass) const = 0;

		/** Attempts to cast the UObject to an FMetasoundAssetBase */
		virtual FMetasoundAssetBase* Cast(UObject* InObject) const = 0;

		/** Attempts to cast the UObject to an FMetasoundAssetBase */
		virtual const FMetasoundAssetBase* Cast(const UObject* InObject) const = 0;

		/** Creates a new object of the UClass type. */
		virtual UObject* NewObject(UPackage* InPackage, const FName& InName) const = 0;

	private:
		IMetasoundUObjectRegistryEntry() = default;

		/** Only the TMetasoundUObjectRegistryEntry can construct this class. */
		template<typename UClassType>
		friend class TMetasoundUObjectRegistryEntry;
	};

	/** An entry into the Metasound-UObject registry.
	 *
	 * @Tparam UClassType A class which derives from UObject and FMetasoundAssetBase.
	 */
	template<typename UClassType>
	class TMetasoundUObjectRegistryEntry : public IMetasoundUObjectRegistryEntry
	{
		// Ensure that this is a subclass of FMetasoundAssetBase and UObject.
		static_assert(std::is_base_of<FMetasoundAssetBase, UClassType>::value, "UClass must be derived from FMetasoundAssetBase");
		static_assert(std::is_base_of<UObject, UClassType>::value, "UClass must be derived from UObject");

	public:
		TMetasoundUObjectRegistryEntry(const FMetasoundFrontendVersion& InInterfaceVersion)
		:	InterfaceVersion(InInterfaceVersion)
		{
		}

		virtual ~TMetasoundUObjectRegistryEntry() = default;

		virtual const FMetasoundFrontendVersion& GetInterfaceVersion() const override
		{
			return InterfaceVersion;
		}

		UClass* GetUClass() const override
		{
			return UClassType::StaticClass();
		}

		bool IsChildClass(const UObject* InObject) const override
		{
			if (nullptr != InObject)
			{
				return InObject->IsA(UClassType::StaticClass());
			}
			return false;
		}

		bool IsChildClass(const UClass* InClass) const override
		{
			if (nullptr != InClass)
			{
				return InClass->IsChildOf(UClassType::StaticClass());
			}
			return false;
		}

		FMetasoundAssetBase* Cast(UObject* InObject) const override
		{
			if (nullptr == InObject)
			{
				return nullptr;
			}
			return static_cast<FMetasoundAssetBase*>(CastChecked<UClassType>(InObject));
		}

		const FMetasoundAssetBase* Cast(const UObject* InObject) const override
		{
			if (nullptr == InObject)
			{
				return nullptr;
			}
			return static_cast<const FMetasoundAssetBase*>(CastChecked<const UClassType>(InObject));
		}

		UObject* NewObject(UPackage* InPackage, const FName& InName) const override
		{
			return ::NewObject<UClassType>(InPackage, InName);
		}

	private:

		FMetasoundFrontendVersion InterfaceVersion;
	};


	/** IMetasoundUObjectRegistry contains IMetasoundUObjectRegistryEntries.
	 *
	 * Registered UObject classes can utilize the Metasound Editor. It also enables
	 * the creation of a UObject directly from a FMetasoundFrontendDocument.
	 */
	class METASOUNDENGINE_API IMetasoundUObjectRegistry
	{
		public:
			virtual ~IMetasoundUObjectRegistry() = default;

			/** Return static singleton instance of the registry. */
			static IMetasoundUObjectRegistry& Get();

			/** Adds an entry to the registry. */
			virtual void RegisterUClassInterface(TUniquePtr<IMetasoundUObjectRegistryEntry>&& InEntry) = 0;

			/** Returns all RegistryEntries with the given name */
			virtual TArray<const IMetasoundUObjectRegistryEntry*> FindInterfaceEntriesByName(FName InName) const = 0;

			/** Returns all UClasses registered to the interface version. */
			virtual TArray<UClass*> FindSupportedInterfaceClasses(const FMetasoundFrontendVersion& InInterfaceVersion) const = 0;

			/** Creates a new object from a MetaSound document.
			 *
			 * @param InClass - A registered UClass to create.
			 * @param InDocument - The FMetasoundFrontendDocument to use when creating the class.
			 * @param InInterfaceVersion - The version of the FMetasoundFrontendClassInterface to use when creating the class.
			 * @param InPath - If in editor, the created asset will be stored at this content path.
			 *
			 * @return A new object. A nullptr on error.
			 */
			virtual UObject* NewObject(UClass* InClass, const FMetasoundFrontendDocument& InDocument, const FString& InPath) const = 0;

			/** Iterate all registered UClasses that serve as MetaSound assets.*/
			virtual void IterateRegisteredUClasses(TFunctionRef<void(UClass&)> InFunc) const = 0;

			/** Returns true if the InObject is of a class or child class which is registered. */
			virtual bool IsRegisteredClass(UObject* InObject) const = 0;

			/** Returns casts the UObject to a FMetasoundAssetBase if the UObject is of a registered type.
			 * If the UObject's UClass is not registered, then a nullptr is returned. 
			 */
			virtual FMetasoundAssetBase* GetObjectAsAssetBase(UObject* InObject) const = 0;

			/** Returns casts the UObject to a FMetasoundAssetBase if the UObject is of a registered type.
			 * If the UObject's UClass is not registered, then a nullptr is returned. 
			 */
			virtual const FMetasoundAssetBase* GetObjectAsAssetBase(const UObject* InObject) const = 0;
	};
} // namespace Metasound
