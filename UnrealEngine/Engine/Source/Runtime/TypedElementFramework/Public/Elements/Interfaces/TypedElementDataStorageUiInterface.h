// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Elements/Common/TypedElementQueryConditions.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Framework/TypedElementMetaData.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/SharedPointer.h"
#include "UObject/Interface.h"

#include "TypedElementDataStorageUiInterface.generated.h"

class ITypedElementDataStorageUiInterface;
class SWidget;

/**
 * Base class used to construct Typed Element widgets with.
 * See below for the options to register a constructor with the Data Storage.
 */
USTRUCT()
struct FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	TYPEDELEMENTFRAMEWORK_API explicit FTypedElementWidgetConstructor(const UScriptStruct* InTypeInfo);
	explicit FTypedElementWidgetConstructor(EForceInit) {} //< For compatibility and shouldn't be directly used.

	virtual ~FTypedElementWidgetConstructor() = default;

	/** Initializes a new constructor based on the provided arguments.. */
	TYPEDELEMENTFRAMEWORK_API virtual bool Initialize(const TypedElementDataStorage::FMetaDataView& InArguments,
		TArray<TWeakObjectPtr<const UScriptStruct>> InMatchedColumnTypes, const TypedElementDataStorage::FQueryConditions& InQueryConditions);

	/** Retrieves the type information for the constructor type. */
	TYPEDELEMENTFRAMEWORK_API virtual const UScriptStruct* GetTypeInfo() const;
	/** Retrieves the columns, if any, that were matched to this constructor when it was created. */
	TYPEDELEMENTFRAMEWORK_API virtual const TArray<TWeakObjectPtr<const UScriptStruct>>& GetMatchedColumns() const;
	/** Retrieves the query conditions that need to match for this widget constructor to produce a widget. */
	TYPEDELEMENTFRAMEWORK_API virtual const TypedElementDataStorage::FQueryConditions* GetQueryConditions() const;

	/** Returns a list of additional columns the widget requires to be added to its rows. */
	TYPEDELEMENTFRAMEWORK_API virtual TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const;
	
	/**
	 *	Calls Construct() to create the internal widget, and then stores it in a container before returning.
	 *	In most cases you want to call this to first create the initial TEDS widget, to ensure the internal widget is
	 *	automatically created/destroyed if the row matches/unmatches the required columns.
	 *	
	 *	Construct() can be called later to (re)create the internal widget if ever required.
	 *	@see Construct
	 */
	TYPEDELEMENTFRAMEWORK_API TSharedPtr<SWidget> ConstructFinalWidget(
		TypedElementRowHandle Row, /** The row the widget will be stored in. */
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		const TypedElementDataStorage::FMetaDataView& Arguments);

	/**
	 * Constructs the widget according to the provided information. Information is collected by calling
	 * the below functions CreateWidget and AddColumns. It's recommended to overload those
	 * functions to build widgets according to a standard recipe and to reduce the amount of code needed.
	 * If a complexer situation is called for this function can also be directly overwritten.
	 * In most cases, you want to call ConstructFinalWidget to create the actual widget.
	 */
	TYPEDELEMENTFRAMEWORK_API virtual TSharedPtr<SWidget> Construct(
		TypedElementRowHandle Row, /** The row the widget will be stored in. */
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		const TypedElementDataStorage::FMetaDataView& Arguments);

protected:
	/** Create a new instance of the target widget. This is a required function. */
	TYPEDELEMENTFRAMEWORK_API virtual TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments);
	/** Set any values in columns if needed. The columns provided through GetAdditionalColumnsList() will have already been created. */
	TYPEDELEMENTFRAMEWORK_API virtual bool SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row);
	/** 
	 * Last opportunity to configure anything in the widget or the row. This step can be needed to initialize widgets with data stored
	 * in columns.
	 */
	TYPEDELEMENTFRAMEWORK_API virtual bool FinalizeWidget(
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		TypedElementRowHandle Row,
		const TSharedPtr<SWidget>& Widget);

	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes;
	const TypedElementDataStorage::FQueryConditions* QueryConditions = nullptr;
	const UScriptStruct* TypeInfo = nullptr;
};

template<>
struct TStructOpsTypeTraits<FTypedElementWidgetConstructor> : public TStructOpsTypeTraitsBase2<FTypedElementWidgetConstructor>
{
	enum
	{
		WithNoInitConstructor = true,
		WithPureVirtual = true,
	};
};

UINTERFACE(MinimalAPI)
class UTypedElementDataStorageUiInterface : public UInterface
{
	GENERATED_BODY()
};

class ITypedElementDataStorageUiInterface
{
	GENERATED_BODY()

public:
	enum class EPurposeType : uint8
	{
		/** General purpose name which allows multiple factory registrations. */
		Generic,
		/**
		 * Only one factory can be registered with this purpose. If multiple factories are registered only the last factory
		 * will be stored.
		 */
		UniqueByName,
		/**
		 * Only one factory can be registered with this purpose for a specific combination of columns. If multiple factories
		 * are registered only the last factory will be stored.
		 */
		UniqueByNameAndColumn
	};

	enum class EMatchApproach : uint8
	{
		/** 
		 * Looks for the longest chain of columns matching widget factories. The matching columns are removed and the process
		 * is repeated until there are no more columns or no matches are found.
		 */
		LongestMatch,
		/** A single widget factory is reduced which matches the requested columns exactly. */
		ExactMatch,
		/** Each column is matched to widget factory. Only widget factories that use a single column are used. */
		SingleMatch
	};

	using WidgetCreatedCallback = TFunctionRef<void(const TSharedRef<SWidget>& NewWidget, TypedElementRowHandle Row)>;
	using WidgetConstructorCallback = TFunctionRef<bool(TUniquePtr<FTypedElementWidgetConstructor>, TConstArrayView<TWeakObjectPtr<const UScriptStruct>>)>;
	using WidgetPurposeCallback = TFunctionRef<void(FName, EPurposeType, const FText&)>;

	/**
	 * Register a widget purpose. Widget purposes indicates how widgets can be used and categorizes/organizes the available 
	 * widget factories. If the same purpose is registered multiple times, only the first will be recorded and later registrations
	 * will be silently ignored.
	 */
	virtual void RegisterWidgetPurpose(FName Purpose, EPurposeType Type, FText Description) = 0;
	/** 
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version registers a generic type. Construction using these are typically cheaper as they can avoid
	 * copying the Constructor and take up less memory. The downside is that they can't store additional configuration
	 * options. If the purpose has not been registered the factory will not be recorded and a warning will be printed.
	 * If registration is successful true will be returned otherwise false.
	 */
	virtual bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor) = 0;
	/**
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version registers a generic type. Construction using these are typically cheaper as they can avoid
	 * copying the Constructor and take up less memory. The downside is that they can't store additional configuration
	 * options. If the purpose has not been registered the factory will not be recorded and a warning will be printed.
	 * If registration is successful true will be returned otherwise false.
	 */
	template<typename ConstructorType>
	bool RegisterWidgetFactory(FName Purpose);
	/**
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version registers a generic type. Construction using these are typically cheaper as they can avoid
	 * copying the Constructor and take up less memory. The downside is that they can't store additional configuration
	 * options. If the purpose has not been registered the factory will not be recorded and a warning will be printed.
	 * The provided columns will be used when matching the factory during widget construction.
	 * If registration is successful true will be returned otherwise false.
	 */
	virtual bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor,
		TypedElementDataStorage::FQueryConditions Columns) = 0;
	/**
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version registers a generic type. Construction using these are typically cheaper as they can avoid
	 * copying the Constructor and take up less memory. The downside is that they can't store additional configuration
	 * options. If the purpose has not been registered the factory will not be recorded and a warning will be printed.
	 * The provided columns will be used when matching the factory during widget construction.
	 * If registration is successful true will be returned otherwise false.
	 */
	template<typename ConstructorType>
	bool RegisterWidgetFactory(FName Purpose, TypedElementDataStorage::FQueryConditions Columns);
	/**
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version uses a previously created instance of the Constructor. The benefit of this is that it store
	 * configuration options. The downside is that it takes up more memory and requires copying when it's
	 * used. If the purpose has not been registered the factory will not be recorded and a warning will be printed.
	 * If registration is successful true will be returned otherwise false.
	 */
	virtual bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor) = 0;
	/**
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version uses a previously created instance of the Constructor. The benefit of this is that it store
	 * configuration options. The downside is that it takes up more memory and requires copying when it's
	 * used. If the purpose has not been registered the factory will not be recorded and a warning will be printed.
	 * The provided columns will be used when matching the factory during widget construction.
	 * If registration is successful true will be returned otherwise false.
	 */
	virtual bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor, 
		TypedElementDataStorage::FQueryConditions Columns) = 0;
	
	/** 
	 * Creates widget constructors for the requested purpose.
	 * The provided arguments will be used to configure the constructor. Settings made this way will be applied to all
	 * widgets created from the constructor, if applicable.
	 */
	virtual void CreateWidgetConstructors(FName Purpose, 
		const TypedElementDataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) = 0;
	/** 
	 * Finds matching widget constructors for provided columns, preferring longer matches over shorter matches.
	 * The provided list of columns will be updated to contain all columns that couldn't be matched.
	 * The provided arguments will be used to configure the constructor. Settings made this way will be applied to all
	 * widgets created from the constructor, if applicable.
	 */
	virtual void CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const TypedElementDataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) = 0;

	/**
	 * Creates all the widgets registered under the provided name. This may be a large number of widgets for a wide name
	 * or exactly one when the exact name of the widget is registered. Arguments can be provided, but widgets are free
	 * to ignore them.
	 */
	virtual void ConstructWidgets(FName Purpose, const TypedElementDataStorage::FMetaDataView& Arguments,
		const WidgetCreatedCallback& ConstructionCallback) = 0;

	/** 
	 * Creates a single widget using the provided constructor. 
	 * The provided row will be used to store the widget information on. If columns have already been added to the row, the 
	 * constructor is free to use that to configure the widget. Arguments are used by the constructor to configure the widget.
	 */
	virtual TSharedPtr<SWidget> ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
		const TypedElementDataStorage::FMetaDataView& Arguments) = 0;

	/** Calls the provided callback for all known registered widget purposes. */
	virtual void ListWidgetPurposes(const WidgetPurposeCallback& Callback) const = 0;
};


//
// Implementations
//

template<typename ConstructorType>
bool ITypedElementDataStorageUiInterface::RegisterWidgetFactory(FName Purpose)
{
	return this->RegisterWidgetFactory(Purpose, ConstructorType::StaticStruct());
}

template<typename ConstructorType>
bool ITypedElementDataStorageUiInterface::RegisterWidgetFactory(FName Purpose, TypedElementDataStorage::FQueryConditions Columns)
{
	return this->RegisterWidgetFactory(Purpose, ConstructorType::StaticStruct(), Columns);
}
