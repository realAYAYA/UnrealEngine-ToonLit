// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClassViewerFilter.h"
#include "IDataInterface.h"

namespace UE::DataInterfaceGraphEditor
{
	// Filter class for things which implement IChooserColumn
	class FDataInterfaceClassFilter : public IClassViewerFilter
	{
	public:
		FDataInterfaceClassFilter(FName InTypeName, const UScriptStruct* InStructType) : TypeName(InTypeName), StructType(InStructType) { };
		virtual ~FDataInterfaceClassFilter() override {};
	
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs ) override
		{
			if (InClass->ImplementsInterface(UDataInterface::StaticClass()))
			{
				// pass filter if the class has "" return type (for wrappers)
				// pass all classes if the requested typename is ""
				// otherwise fail if the TypeNames don't match
				// for Object type, check type hierarchy. (todo)
			
				IDataInterface* Object = static_cast<IDataInterface*>(InClass->GetDefaultObject()->GetInterfaceAddress(UDataInterface::StaticClass()));
				if (Object->GetReturnTypeName() == TypeName || Object->GetReturnTypeName() == "" || TypeName == "")
				{
					if (const UScriptStruct* ReturnStructType = Object->GetReturnTypeStruct())
					{
						return true; // kytodo: compare the struct types to see if they are compatible
					}
					else
					{
						return true;
					}
				}
			}

			return false;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return false;
		}
	private:
		FName TypeName;
		const UScriptStruct* StructType;
	};
}