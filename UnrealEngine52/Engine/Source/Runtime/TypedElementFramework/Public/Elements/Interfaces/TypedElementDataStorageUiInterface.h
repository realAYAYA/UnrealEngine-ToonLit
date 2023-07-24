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
struct TYPEDELEMENTFRAMEWORK_API FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	virtual ~FTypedElementWidgetConstructor() = default;

	/**
	 * Constructs the widget according to the provided information. Information is collected by calling
	 * the below functions ApplyArguments, CreateWidget and AddColumns. It's recommeded to overload those
	 * functions to build widgets according to a standard recipe and to reduce the amount of code needed.
	 * If a complexer situation is called for this function can also be directly overwritten.
	 */
	virtual TSharedPtr<SWidget> Construct(
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
	virtual void ApplyArguments(TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments);
	/** Create a new instance of the target widget. This is a required function. */
	virtual TSharedPtr<SWidget> CreateWidget() PURE_VIRTUAL(FTypedElementWidgetConstructor::CreateWidget, return nullptr; );
	/**
	 * Add any columns to the provided row in the data storage to support the widget. By default a column will be added
	 * that keeps track of the lifecycle of the widget.
	 */
	virtual void AddColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget);
};

template<>
struct TStructOpsTypeTraits<FTypedElementWidgetConstructor> : public TStructOpsTypeTraitsBase2<FTypedElementWidgetConstructor>
{
	enum
	{
		WithPureVirtual = true,
	};
};

UINTERFACE(MinimalAPI)
class UTypedElementDataStorageUiInterface : public UInterface
{
	GENERATED_BODY()
};

class TYPEDELEMENTFRAMEWORK_API ITypedElementDataStorageUiInterface
{
	GENERATED_BODY()

public:
	using WidgetCreatedCallback = TFunctionRef<void(const TSharedRef<SWidget>& NewWidget, TypedElementRowHandle Row)>;

	/** 
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version registers a generic type. Construction using these are typically cheaper as they can avoid
	 * copying the Constructor and take up less memory. The downside is that they can't store additional configuration
	 * options.
	 */
	virtual void RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor) = 0;
	
	/**
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version uses a pre-created instance of the Constructor. The benefit of this is that it store
	 * configuration options. The downside is that it takes up more memory and requires copying when it's
	 * used.
	 */
	template<typename ConstructorType>
	void RegisterWidgetFactory(FName Purpose, TUniquePtr<ConstructorType>&& Constructor);
	
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

protected:
	/**
	 * Registers a widget factory that will be called when the purpose it's registered under is requested.
	 * This version uses a pre-created instance of the Constructor. The benefit of this is that it store
	 * configuration options. The downside is that it takes up more memory and requires copying when it's
	 * used.
	 */
	virtual void RegisterWidgetFactory(FName Purpose, const UScriptStruct* ConstructorType,
		TUniquePtr<FTypedElementWidgetConstructor>&& Constructor) = 0;
};


// Implementations
template<typename ConstructorType>
void ITypedElementDataStorageUiInterface::RegisterWidgetFactory(FName Purpose, TUniquePtr<ConstructorType>&& Constructor)
{
	static_assert(TIsDerivedFrom<ConstructorType, FTypedElementWidgetConstructor>::Value, 
		"Provided Typed Element Widget Constructor doesn't derive from FTypedElementWidgetConstructor.");
	RegisterWidgetFactory(Purpose, ConstructorType::StaticStruct(), MoveTemp(Constructor));
}