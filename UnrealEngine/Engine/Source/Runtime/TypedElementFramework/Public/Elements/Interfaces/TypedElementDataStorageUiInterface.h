// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/Interface.h"

#include "TypedElementDataStorageUiInterface.generated.h"

class ITypedElementDataStorageUiInterface;
class SWidget;

/**
 * Base class used to construct Typed Element widgets with.
 * It's recommended to expose any construction variables as properties so they
 * can either be set by a user or set using the passed in arguments. The
 * Arguments can be directly used in case complex operations need to be done that
 * prevent automatically setting construction variables.
 * See below for the options to register a constructor with the Data Storage. For
 * either registration case a new instance/copy of the constructor is created so
 * arguments can be safely applied.
 */
USTRUCT()
struct FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	TYPEDELEMENTFRAMEWORK_API explicit FTypedElementWidgetConstructor(const UScriptStruct* InTypeInfo);
	explicit FTypedElementWidgetConstructor(EForceInit) {} //< For compatibility and shouldn't be directly used.

	virtual ~FTypedElementWidgetConstructor() = default;

	/** Retrieves the type information for the constructor type. */
	TYPEDELEMENTFRAMEWORK_API virtual const UScriptStruct* GetTypeInfo() const;

	/** Returns a list of additional columns the widget requires. */
	TYPEDELEMENTFRAMEWORK_API virtual TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const;

	/** 
	 * Whether or not an instance of this constructor can be reused. Setting this to true means that the Data Storage will
	 * avoid recreating new instances of the constructor and instead keeps reusing the created version. The default is
	 * 'false'. Override this function and set it to true if there's no internal state that could interfere with creating
	 * multiple widgets from the same constructor, e.g. data stored from passed in arguments.
	 */
	TYPEDELEMENTFRAMEWORK_API virtual bool CanBeReused() const;

	/**
	 * Constructs the widget according to the provided information. Information is collected by calling
	 * the below functions ApplyArguments, CreateWidget and AddColumns. It's recommended to overload those
	 * functions to build widgets according to a standard recipe and to reduce the amount of code needed.
	 * If a complexer situation is called for this function can also be directly overwritten.
	 */
	TYPEDELEMENTFRAMEWORK_API virtual TSharedPtr<SWidget> Construct(
		TypedElementRowHandle Row, /** The row the widget will be stored in. */
		ITypedElementDataStorageInterface* DataStorage,
		ITypedElementDataStorageUiInterface* DataStorageUi,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments);

protected:
	/**
	 * Uses the type system to apply the provided arguments to constructor's properties. Overwrite this 
	 * function if there are non-properties that can be configured through the arguments or if the configuration
	 * contains arguments that are too complex to initialize (fully) through the type system.
	 */
	TYPEDELEMENTFRAMEWORK_API virtual bool ApplyArguments(TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments);
	/** Create a new instance of the target widget. This is a required function. */
	TYPEDELEMENTFRAMEWORK_API virtual TSharedPtr<SWidget> CreateWidget() PURE_VIRTUAL(FTypedElementWidgetConstructor::CreateWidget, return nullptr; );
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

	const UScriptStruct* TypeInfo{ nullptr };
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
	 * The provided columns will be used when matching the factory during widget construction.
	 * If registration is successful true will be returned otherwise false.
	 */
	virtual bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor,
		TArray<TWeakObjectPtr<const UScriptStruct>> Columns) = 0;
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
		TArray<TWeakObjectPtr<const UScriptStruct>> Columns) = 0;
	
	/** Creates widget constructors for the requested purpose. */
	virtual void CreateWidgetConstructors(FName Purpose, 
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments, const WidgetConstructorCallback& Callback) = 0;
	/** 
	 * Finds matching widget constructors for provided columns, preferring longer matches over shorter matches.
	 * The provided list of columns will be updated to contain all columns that couldn't be matched.
	 */
	virtual void CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments, const WidgetConstructorCallback& Callback) = 0;

	/**
	 * Creates all the widgets registered under the provided name. This may be a large number of widgets for a wide name
	 * or exactly one when the exact name of the widget is registered. Arguments can be provided, but widgets are free
	 * to ignore them.
	 */
	virtual void ConstructWidgets(FName Purpose, TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments, 
		const WidgetCreatedCallback& ConstructionCallback) = 0;

	/** Creates a single widget using the provided constructor. Arguments can optionally be used to intialize the constructor. */
	virtual TSharedPtr<SWidget> ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments) = 0;

	/** Calls the provided callback for all known registered widget purposes. */
	virtual void ListWidgetPurposes(const WidgetPurposeCallback& Callback) const = 0;
};
