// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonWriter.h"

class Error;

class FJsonSerializer
{
public:

	enum class EFlags
	{
		None = 0,
		StoreNumbersAsStrings = 1,
	};

	template <class CharType>
	static bool Deserialize(const TSharedRef<TJsonReader<CharType>>& Reader, TArray<TSharedPtr<FJsonValue>>& OutArray, EFlags InOptions = EFlags::None)
	{
		return Deserialize(*Reader, OutArray, InOptions);
	}

	template <class CharType>
	static bool Deserialize(TJsonReader<CharType>& Reader, TArray<TSharedPtr<FJsonValue>>& OutArray, EFlags InOptions = EFlags::None)
	{
		StackState State;
		if (!Deserialize(Reader, /*OUT*/State, InOptions))
		{
			return false;
		}

		// Empty array is ok.
		if (State.Type != EJson::Array)
		{
			return false;
		}

		OutArray = State.Array;

		return true;
	}

	template <class CharType>
	static bool Deserialize(const TSharedRef<TJsonReader<CharType>>& Reader, TSharedPtr<FJsonObject>& OutObject, EFlags InOptions = EFlags::None)
	{
		return Deserialize(*Reader, OutObject, InOptions);
	}
	template <class CharType>
	static bool Deserialize(TJsonReader<CharType>& Reader, TSharedPtr<FJsonObject>& OutObject, EFlags InOptions = EFlags::None)
	{
		StackState State;
		if (!Deserialize(Reader, /*OUT*/State, InOptions))
		{
			return false;
		}

		if (!State.Object.IsValid())
		{
			return false;
		}

		OutObject = State.Object;

		return true;
	}

	template <class CharType>
	static bool Deserialize(const TSharedRef<TJsonReader<CharType>>& Reader, TSharedPtr<FJsonValue>& OutValue, EFlags InOptions = EFlags::None)
	{
		return Deserialize(*Reader, OutValue, InOptions);
	}

	template <class CharType>
	static bool Deserialize(TJsonReader<CharType>& Reader, TSharedPtr<FJsonValue>& OutValue, EFlags InOptions = EFlags::None)
	{
		StackState State;
		if (!Deserialize(Reader, /*OUT*/State, InOptions))
		{
			return false;
		}

		switch (State.Type)
		{
		case EJson::Object:
			if (!State.Object.IsValid())
			{
				return false;
			}
			OutValue = MakeShared<FJsonValueObject>(State.Object);
			break;
		case EJson::Array:
			OutValue = MakeShared<FJsonValueArray>(State.Array);
			break;
		default:
			// FIXME: would be nice to handle non-composite root values but StackState Deserialize just drops them on the floor
			return false;
		}
		return true;
	}

	/**
	 * Serialize the passed array of json values into the writer.
	 * This will effectively serialize all of the values enclosed in [] square brackets.
	 * Example:
	 *  - Writer state: [123 <writer position>
	 *    Parameter: Array: ["foo", "bar", "", 456]
	 *    Serialization result: [123, ["foo", "bar", "", 456] <writer position>
	 *
	 * @param Array		    The json array we are serializing
	 * @param Writer		The writer the array is written into.
	 * @param bCloseWriter	When set to true the Writer will be closed after the serialization.
	 * @return				Returns true if the serialization was successful, false otherwise.
	 */
	template <class CharType, class PrintPolicy>
	static bool Serialize(const TArray<TSharedPtr<FJsonValue>>& Array, const TSharedRef<TJsonWriter<CharType, PrintPolicy>>& Writer, bool bCloseWriter = true)
	{
		return Serialize(Array, *Writer, bCloseWriter);
	}

	/**
	 * Serialize the passed array of json values into the writer.
	 * This will effectively serialize all of the values enclosed in [] square brackets.
	 * Example:
	 *  - Writer state: [123 <writer position>
	 *    Parameter: Array: ["foo", "bar", "", 456]
	 *    Serialization result: [123, ["foo", "bar", "", 456] <writer position>
	 *
	 * @param Array		    The json array we are serializing
	 * @param Writer		The writer the array is written into.
	 * @param bCloseWriter	When set to true the Writer will be closed after the serialization.
	 * @return				Returns true if the serialization was successful, false otherwise.
	 */
	template <class CharType, class PrintPolicy>
	static bool Serialize(const TArray<TSharedPtr<FJsonValue>>& Array, TJsonWriter<CharType, PrintPolicy>& Writer, bool bCloseWriter = true )
	{
		const TSharedRef<FElement> StartingElement = MakeShared<FElement>(Array);
		return FJsonSerializer::Serialize<CharType, PrintPolicy>(StartingElement, Writer, bCloseWriter);
	}

	/**
	 * Serialize the passed Json object into the writer.
	 * This will effectively serialize all of the identifier:value pairs of the object enclosed in {} curly brackets.
	 * Example:
	 *  - Writer state: [123 <writer position>
	 *    Parameter: Object: {"foo": "bar", "baz": "", "": 456}
	 *    Serialization result: [123, {"foo": "bar", "baz": "", "": 456} <writer position>
	 *
	 * @param Object		The json object we are serializing
	 * @param Writer		The writer the object is written into.
	 * @param bCloseWriter	When set to true the Writer will be closed after the serialization.
	 * @return				Returns true if the serialization was successful, false otherwise.
	 */
	template <class CharType, class PrintPolicy>
	static bool Serialize(const TSharedRef<FJsonObject>& Object, const TSharedRef<TJsonWriter<CharType, PrintPolicy>>& Writer, bool bCloseWriter = true )
	{
		return Serialize(Object, *Writer, bCloseWriter);
	}

	/**
	 * Serialize the passed Json object into the writer.
	 * This will effectively serialize all of the identifier:value pairs of the object enclosed in {} brackets.
	 * Example:
	 *  - Writer state: [123 <writer position>
	 *    Parameter: Object: {"foo": "bar", "baz": "", "": 456}
	 *    Serialization result: [123, {"foo": "bar", "baz": "", "": 456} <writer position>
	 * 
     * @param Object		The json object we are serializing
	 * @param Writer		The writer the object is written into.
	 * @param bCloseWriter	When set to true the Writer will be closed after the serialization.
	 * @return				Returns true if the serialization was successful, false otherwise.
	 */
	template <class CharType, class PrintPolicy>
	static bool Serialize(const TSharedRef<FJsonObject>& Object, TJsonWriter<CharType, PrintPolicy>& Writer, bool bCloseWriter = true)
	{
		const TSharedRef<FElement> StartingElement = MakeShared<FElement>(Object);
		return FJsonSerializer::Serialize<CharType, PrintPolicy>(StartingElement, Writer, bCloseWriter);
	}

	/**
	 * Serialize the passed Json value and identifier into the writer.
	 * Empty string identifiers will be ignored when the writer is not writing inside of a json object and only the value will be serialized.
	 * If the writer is in a state where it's currently writing inside of a json object, then the identifier will always be serialized.
	 * Examples:
	 *	- Writer state: { "foo": "bar" <writer position>
	 *    Parameters: Identifier: ""
	 *                Value: "baz"
	 *    Serialization result: { "foo": "bar", "": "baz" <writer position> //empty identifier is serialized as a valid key for the key:value pair "":"baz"
	 *
	 * - Writer state: { "foo": ["bar" <writer position>
	 *   Parameters: Identifier: ""
	 *               Value: "baz"
	 *   Serialization result: { foo: ["bar", "baz" <writer position> //empty identifier is ignored since we are writing into an array and not an object.
	 *
	 * @param Value			The json value we are serializing
	 * @param Identifier	The identifier of the value, empty identifiers are ignored outside of json objects.
	 * @param Writer		The writer the value and identifier are written into.
	 * @param bCloseWriter	When set to true the Writer will be closed after the serialization.
	 * @return				Returns true if the serialization was successful, false otherwise.
	 */
	template <class CharType, class PrintPolicy>
	static bool Serialize(const TSharedPtr<FJsonValue>& Value, const FString& Identifier, const TSharedRef<TJsonWriter<CharType, PrintPolicy>>& Writer, bool bCloseWriter = true)
	{
		return Serialize(Value, Identifier, *Writer, bCloseWriter);
	}

	/**
	 * Serialize the passed Json value and identifier into the writer.
	 * Empty string identifiers will be ignored when the writer is not writing inside of a json object and only the value will be serialized.
	 * If the writer is in a state where it's currently writing inside of a json object, then the identifier will always be serialized.
	 * Examples:
	 *	- Writer state: { "foo": "bar" <writer position>
	 *    Parameters: Identifier: ""
	 *                Value: "baz"
	 *    Serialization result: { "foo": "bar", "": "baz" <writer position> //empty identifier is serialized as a valid key for the key:value pair "":"baz"
	 * 
	 * - Writer state: { "foo": ["bar" <writer position>
	 *   Parameters: Identifier: ""
	 *               Value: "baz"
	 *   Serialization result: { foo: ["bar", "baz" <writer position> //empty identifier is ignored since we are writing into an array and not an object.
	 * 
	 * @param Value			The json value we are serializing
	 * @param Identifier	The identifier of the value, empty identifiers are ignored outside of json objects.
	 * @param Writer		The writer the value and identifier are written into.
	 * @param bCloseWriter	When set to true the Writer will be closed after the serialization.
	 * @return				Returns true if the serialization was successful, false otherwise.
	 */
	template <class CharType, class PrintPolicy>
	static bool Serialize(const TSharedPtr<FJsonValue>& Value, const FString& Identifier, TJsonWriter<CharType, PrintPolicy>& Writer, bool bCloseWriter = true)
	{
		const TSharedRef<FElement> StartingElement = MakeShared<FElement>(Identifier, Value);
		return FJsonSerializer::Serialize<CharType, PrintPolicy>(StartingElement, Writer, bCloseWriter);
	}

private:

	struct StackState
	{
		EJson Type;
		FString Identifier;
		TArray<TSharedPtr<FJsonValue>> Array;
		TSharedPtr<FJsonObject> Object;
	};

	struct FElement
	{
		FElement( const TSharedPtr<FJsonValue>& InValue )
			: Identifier()
			, Value(InValue)
		{ }

		FElement( const TSharedRef<FJsonObject>& Object )
			: Identifier()
			, Value(MakeShared<FJsonValueObject>(Object))
		{ }

		FElement( const TArray<TSharedPtr<FJsonValue>>& Array )
			: Identifier()
			, Value(MakeShared<FJsonValueArray>(Array))
		{ }

		FElement( const FString& InIdentifier, const TSharedPtr< FJsonValue >& InValue )
			: Identifier( InIdentifier )
			, Value( InValue )
			, bIsKeyValuePair( true )
		{

		}

		FString Identifier;
		TSharedPtr< FJsonValue > Value;
		bool bHasBeenProcessed = false;
		bool bIsKeyValuePair = false;
	};

private:

	template <class CharType>
	static bool Deserialize(TJsonReader<CharType>& Reader, StackState& OutStackState, EFlags InOptions)
	{
		TArray<TSharedRef<StackState>> ScopeStack; 
		TSharedPtr<StackState> CurrentState;

		TSharedPtr<FJsonValue> NewValue;
		EJsonNotation Notation;

		while (Reader.ReadNext(Notation))
		{
			FString Identifier = Reader.GetIdentifier();
			NewValue.Reset();

			switch( Notation )
			{
			case EJsonNotation::ObjectStart:
				{
					if (CurrentState.IsValid())
					{
						ScopeStack.Push(CurrentState.ToSharedRef());
					}

					CurrentState = MakeShared<StackState>();
					CurrentState->Type = EJson::Object;
					CurrentState->Identifier = Identifier;
					CurrentState->Object = MakeShared<FJsonObject>();
				}
				break;

			case EJsonNotation::ObjectEnd:
				{
					if (ScopeStack.Num() > 0)
					{
						Identifier = CurrentState->Identifier;
						NewValue = MakeShared<FJsonValueObject>(CurrentState->Object);
						CurrentState = ScopeStack.Pop();
					}
				}
				break;

			case EJsonNotation::ArrayStart:
				{
					if (CurrentState.IsValid())
					{
						ScopeStack.Push(CurrentState.ToSharedRef());
					}

					CurrentState = MakeShared<StackState>();
					CurrentState->Type = EJson::Array;
					CurrentState->Identifier = Identifier;
				}
				break;

			case EJsonNotation::ArrayEnd:
				{
					if (ScopeStack.Num() > 0)
					{
						Identifier = CurrentState->Identifier;
						NewValue = MakeShared<FJsonValueArray>(CurrentState->Array);
						CurrentState = ScopeStack.Pop();
					}
				}
				break;

			case EJsonNotation::Boolean:
				NewValue = MakeShared<FJsonValueBoolean>(Reader.GetValueAsBoolean());
				break;

			case EJsonNotation::String:
				NewValue = MakeShared<FJsonValueString>(Reader.GetValueAsString());
				break;

			case EJsonNotation::Number:
				if (EnumHasAnyFlags(InOptions, EFlags::StoreNumbersAsStrings))
				{
					NewValue = MakeShared<FJsonValueNumberString>(Reader.GetValueAsNumberString());
				}
				else
				{
					NewValue = MakeShared<FJsonValueNumber>(Reader.GetValueAsNumber());
				}
				break;

			case EJsonNotation::Null:
				NewValue = MakeShared<FJsonValueNull>();
				break;

			case EJsonNotation::Error:
				return false;
				break;
			}

			if (NewValue.IsValid() && CurrentState.IsValid())
			{
				if (CurrentState->Type == EJson::Object)
				{
					CurrentState->Object->SetField(Identifier, NewValue);
				}
				else
				{
					CurrentState->Array.Add(NewValue);
				}
			}
		}

		if (!CurrentState.IsValid() || !Reader.GetErrorMessage().IsEmpty())
		{
			return false;
		}

		OutStackState = *CurrentState.Get();

		return true;
	}

	template <class CharType, class PrintPolicy>
	static bool Serialize(const TSharedRef<FElement>& StartingElement, TJsonWriter<CharType, PrintPolicy>& Writer, bool bCloseWriter)
	{
		TArray<TSharedRef<FElement>> ElementStack;
		ElementStack.Push(StartingElement);

		while (ElementStack.Num() > 0)
		{
			TSharedRef<FElement> Element = ElementStack.Pop();
			// Empty keys are valid identifiers only when writing inside an object.
			const bool bWriteValueOnly = !Element->bIsKeyValuePair || (Element->Identifier.IsEmpty() && Writer.GetCurrentElementType() != EJson::Object);
			check(Element->Value->Type != EJson::None);

			switch (Element->Value->Type)
			{
			case EJson::Number:	
				{
					if (bWriteValueOnly)
					{
						if (Element->Value->PreferStringRepresentation())
						{
							Writer.WriteRawJSONValue(Element->Value->AsString());
						}
						else
						{
							Writer.WriteValue(Element->Value->AsNumber());
						}
					}
					else
					{
						if (Element->Value->PreferStringRepresentation())
						{
							Writer.WriteRawJSONValue(Element->Identifier, Element->Value->AsString());
						}
						else
						{
							Writer.WriteValue(Element->Identifier, Element->Value->AsNumber());
						}
					}
				}
				break;

			case EJson::Boolean:					
				{
					if (bWriteValueOnly)
					{
						Writer.WriteValue(Element->Value->AsBool());
					}
					else
					{
						Writer.WriteValue(Element->Identifier, Element->Value->AsBool());
					}
				}
				break;

			case EJson::String:
				{
					if (bWriteValueOnly)
					{
						Writer.WriteValue(Element->Value->AsString());
					}
					else
					{
						Writer.WriteValue(Element->Identifier, Element->Value->AsString());
					}
				}
				break;

			case EJson::Null:
				{
					if (bWriteValueOnly)
					{
						Writer.WriteNull();
					}
					else
					{
						Writer.WriteNull(Element->Identifier);
					}
				}
				break;

			case EJson::Array:
				{
					if (Element->bHasBeenProcessed)
					{
						Writer.WriteArrayEnd();
					}
					else
					{
						Element->bHasBeenProcessed = true;
						ElementStack.Push(Element);

						if (bWriteValueOnly)
						{
							Writer.WriteArrayStart();
						}
						else
						{
							Writer.WriteArrayStart(Element->Identifier);
						}

						TArray<TSharedPtr<FJsonValue>> Values = Element->Value->AsArray();

						for (int Index = Values.Num() - 1; Index >= 0; --Index)
						{
							ElementStack.Push(MakeShared<FElement>(Values[Index]));
						}
					}
				}
				break;

			case EJson::Object:
				{
					if (Element->bHasBeenProcessed)
					{
						Writer.WriteObjectEnd();
					}
					else
					{
						Element->bHasBeenProcessed = true;
						ElementStack.Push(Element);

						if (bWriteValueOnly)
						{
							Writer.WriteObjectStart();
						}
						else
						{
							Writer.WriteObjectStart(Element->Identifier);
						}

						TArray<FString> Keys; 
						TArray<TSharedPtr<FJsonValue>> Values;
						TSharedPtr<FJsonObject> ElementObject = Element->Value->AsObject();
						ElementObject->Values.GenerateKeyArray(Keys);
						ElementObject->Values.GenerateValueArray(Values);

						check(Keys.Num() == Values.Num());

						for (int Index = Values.Num() - 1; Index >= 0; --Index)
						{
							ElementStack.Push(MakeShared<FElement>(Keys[Index], Values[Index]));
						}
					}
				}
				break;

			default: 
				UE_LOG(LogJson, Fatal,TEXT("Could not print Json Value, unrecognized type."));
			}
		}

		if (bCloseWriter)
		{
			return Writer.Close();
		}
		else
		{
			return true;
		}
	}
};
