// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Property.cpp: FProperty implementation
=============================================================================*/

#include "CoreMinimal.h"
#include "Hash/Blake3.h"
#include "Misc/AsciiSet.h"
#include "Misc/Guid.h"
#include "Misc/StringBuilder.h"
#include "Math/RandomStream.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"
#include "UObject/PropertyHelper.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SoftObjectPath.h"
#include "Math/Box2D.h"
#include "Math/InterpCurvePoint.h"
#include "UObject/ReleaseObjectVersion.h"

DEFINE_LOG_CATEGORY(LogProperty);

// List the core ones here as they have already been included (and can be used without CoreUObject!)
template<typename T>
struct TVector3StructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithStructuredSerializer = true,
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FVector3f> : public TVector3StructOpsTypeTraits<FVector3f> {};
template<> struct TStructOpsTypeTraits<FVector3d> : public TVector3StructOpsTypeTraits<FVector3d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector3f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector3d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector);	// Aliased

template<typename T>
struct TIntPointStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FInt32Point> : public TIntPointStructOpsTypeTraits<FInt32Point> {};
template<> struct TStructOpsTypeTraits<FInt64Point> : public TIntPointStructOpsTypeTraits<FInt64Point> {};
template<> struct TStructOpsTypeTraits<FUint32Point> : public TIntPointStructOpsTypeTraits<FUint32Point> {};
template<> struct TStructOpsTypeTraits<FUint64Point> : public TIntPointStructOpsTypeTraits<FUint64Point> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int32Point);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int64Point);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint32Point);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint64Point);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", IntPoint);		// Aliased
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", UintPoint);		// Aliased

template<typename T>
struct TIntVectorStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FInt32Vector2> : public TIntVectorStructOpsTypeTraits<FInt32Vector2> {};
template<> struct TStructOpsTypeTraits<FInt64Vector2> : public TIntVectorStructOpsTypeTraits<FInt64Vector2> {};
template<> struct TStructOpsTypeTraits<FUint32Vector2> : public TIntVectorStructOpsTypeTraits<FUint32Vector2> {};
template<> struct TStructOpsTypeTraits<FUint64Vector2> : public TIntVectorStructOpsTypeTraits<FUint64Vector2> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int32Vector2);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int64Vector2);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint32Vector2);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint64Vector2);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", IntVector2);		// Aliased
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", UintVector2);	// Aliased

template<> struct TStructOpsTypeTraits<FInt32Vector3> : public TIntVectorStructOpsTypeTraits<FInt32Vector3> {};
template<> struct TStructOpsTypeTraits<FInt64Vector3> : public TIntVectorStructOpsTypeTraits<FInt64Vector3> {};
template<> struct TStructOpsTypeTraits<FUint32Vector3> : public TIntVectorStructOpsTypeTraits<FUint32Vector3> {};
template<> struct TStructOpsTypeTraits<FUint64Vector3> : public TIntVectorStructOpsTypeTraits<FUint64Vector3> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int32Vector);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int64Vector);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint32Vector);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint64Vector);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", IntVector);		// Aliased
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", UintVector);		// Aliased

template<> struct TStructOpsTypeTraits<FInt32Vector4> : public TIntVectorStructOpsTypeTraits<FInt32Vector4> {};
template<> struct TStructOpsTypeTraits<FInt64Vector4> : public TIntVectorStructOpsTypeTraits<FInt64Vector4> {};
template<> struct TStructOpsTypeTraits<FUint32Vector4> : public TIntVectorStructOpsTypeTraits<FUint32Vector4> {};
template<> struct TStructOpsTypeTraits<FUint64Vector4> : public TIntVectorStructOpsTypeTraits<FUint64Vector4> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int32Vector4);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Int64Vector4);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint32Vector4);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Uint64Vector4); 
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", IntVector4);		// Aliased
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", UintVector4);	// Aliased

template<typename T>
struct TVector2StructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FVector2f> : public TVector2StructOpsTypeTraits<FVector2f> {};
template<> struct TStructOpsTypeTraits<FVector2d> : public TVector2StructOpsTypeTraits<FVector2d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector2f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector2d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector2D);

template<typename T>
struct TVector4StructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FVector4f> : public TVector4StructOpsTypeTraits<FVector4f> {};
template<> struct TStructOpsTypeTraits<FVector4d> : public TVector4StructOpsTypeTraits<FVector4d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector4f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector4d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Vector4);	// Aliased

template<typename T>
struct TPlaneStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FPlane4f> : public TPlaneStructOpsTypeTraits<FPlane4f> {};
template<> struct TStructOpsTypeTraits<FPlane4d> : public TPlaneStructOpsTypeTraits<FPlane4d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Plane4f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Plane4d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Plane);	// Aliased

template<typename T>
struct TRotatorStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};

template<> struct TStructOpsTypeTraits<FRotator3f> : public TRotatorStructOpsTypeTraits<FRotator3f> {};
template<> struct TStructOpsTypeTraits<FRotator3d> : public TRotatorStructOpsTypeTraits<FRotator3d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Rotator3f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Rotator3d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Rotator);	// Aliased

template<typename T>
struct TBox3StructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FBox3f> : public TBox3StructOpsTypeTraits<FBox3f> {};
template<> struct TStructOpsTypeTraits<FBox3d> : public TBox3StructOpsTypeTraits<FBox3d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Box3f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Box3d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Box);		// Aliased

template<typename T>
struct TBox2StructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializeFromMismatchedTag = true,		
	};
};
template<> struct TStructOpsTypeTraits<FBox2f> : public TBox2StructOpsTypeTraits<FBox2f> {};
template<> struct TStructOpsTypeTraits<FBox2d> : public TBox2StructOpsTypeTraits<FBox2d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Box2f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Box2d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Box2D);

template<typename T>
struct TMatrixStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};

template<> struct TStructOpsTypeTraits<FMatrix44f> : public TMatrixStructOpsTypeTraits<FMatrix44f> {};
template<> struct TStructOpsTypeTraits<FMatrix44d> : public TMatrixStructOpsTypeTraits<FMatrix44d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Matrix44f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Matrix44d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Matrix);	// Aliased

template<typename T>
struct TBoxSphereBoundsStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FBoxSphereBounds3f> : public TBoxSphereBoundsStructOpsTypeTraits<FBoxSphereBounds3f> {};
template<> struct TStructOpsTypeTraits<FBoxSphereBounds3d> : public TBoxSphereBoundsStructOpsTypeTraits<FBoxSphereBounds3d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", BoxSphereBounds3f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", BoxSphereBounds3d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", BoxSphereBounds);	// Aliased

template<>
struct TStructOpsTypeTraits<FOrientedBox> : public TStructOpsTypeTraitsBase2<FOrientedBox>
{
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", OrientedBox);

template<>
struct TStructOpsTypeTraits<FLinearColor> : public TStructOpsTypeTraitsBase2<FLinearColor>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithStructuredSerializer = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", LinearColor);

template<>
struct TStructOpsTypeTraits<FColor> : public TStructOpsTypeTraitsBase2<FColor>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
		WithSerializer = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Color);


template<typename T>
struct TQuatStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum 
	{
		//quat is somewhat special in that it initialized w to one
		WithNoInitConstructor = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithIdentical = true,
		WithSerializer = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FQuat4f> : public TQuatStructOpsTypeTraits<FQuat4f> {};
template<> struct TStructOpsTypeTraits<FQuat4d> : public TQuatStructOpsTypeTraits<FQuat4d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Quat4f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Quat4d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Quat);		// Aliased to one of FQuat4f/FQuat4d

template<>
struct TStructOpsTypeTraits<FTwoVectors> : public TStructOpsTypeTraitsBase2<FTwoVectors>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithZeroConstructor = true,
		WithSerializer = true,
		WithNoDestructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", TwoVectors);

template<>
struct TStructOpsTypeTraits<FInterpCurvePointFloat> : public TStructOpsTypeTraitsBase2<FInterpCurvePointFloat>
{
	enum
	{
		WithNoInitConstructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", InterpCurvePointFloat);

template<>
struct TStructOpsTypeTraits<FInterpCurvePointVector2D> : public TStructOpsTypeTraitsBase2<FInterpCurvePointVector2D>
{
	enum
	{
		WithNoInitConstructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", InterpCurvePointVector2D);

template<>
struct TStructOpsTypeTraits<FInterpCurvePointVector> : public TStructOpsTypeTraitsBase2<FInterpCurvePointVector>
{
	enum
	{
		WithNoInitConstructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", InterpCurvePointVector);

template<>
struct TStructOpsTypeTraits<FInterpCurvePointQuat> : public TStructOpsTypeTraitsBase2<FInterpCurvePointQuat>
{
	enum
	{
		WithNoInitConstructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", InterpCurvePointQuat);

template<>
struct TStructOpsTypeTraits<FInterpCurvePointTwoVectors> : public TStructOpsTypeTraitsBase2<FInterpCurvePointTwoVectors>
{
	enum
	{
		WithNoInitConstructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", InterpCurvePointTwoVectors);

template<>
struct TStructOpsTypeTraits<FInterpCurvePointLinearColor> : public TStructOpsTypeTraitsBase2<FInterpCurvePointLinearColor>
{
	enum
	{
		WithNoInitConstructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", InterpCurvePointLinearColor);

template<>
struct TStructOpsTypeTraits<FGuid> : public TStructOpsTypeTraitsBase2<FGuid>
{
	enum 
	{
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithZeroConstructor = true,
		WithSerializer = true,
		WithStructuredSerializer = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Guid);

template<typename T>
struct TTransformStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		//WithSerializer = true,
		WithIdentical = true,
		WithSerializeFromMismatchedTag = true,
	};
};
template<> struct TStructOpsTypeTraits<FTransform3f> : public TTransformStructOpsTypeTraits<FTransform3f> {};
template<> struct TStructOpsTypeTraits<FTransform3d> : public TTransformStructOpsTypeTraits<FTransform3d> {};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Transform3f);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Transform3d);
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Transform); // Aliased

template<>
struct TStructOpsTypeTraits<FRandomStream> : public TStructOpsTypeTraitsBase2<FRandomStream>
{
	enum 
	{
		WithExportTextItem = true,
		WithNoInitConstructor = true,
		WithZeroConstructor = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", RandomStream);

template<>
struct TStructOpsTypeTraits<FDateTime> : public TStructOpsTypeTraitsBase2<FDateTime>
{
	enum 
	{
		WithCopy = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializer = true,
		WithNetSerializer = true,
		WithZeroConstructor = true,
		WithIdenticalViaEquality = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", DateTime);

template<>
struct TStructOpsTypeTraits<FTimespan> : public TStructOpsTypeTraitsBase2<FTimespan>
{
	enum 
	{
		WithCopy = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializer = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithZeroConstructor = true,
		WithIdenticalViaEquality = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", Timespan);

template<>
struct TStructOpsTypeTraits<FFrameNumber> : public TStructOpsTypeTraitsBase2<FFrameNumber>
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", FrameNumber);

template<>
struct TStructOpsTypeTraits<FSoftObjectPath> : public TStructOpsTypeTraitsBase2<FSoftObjectPath>
{
	enum
	{
		WithZeroConstructor = true,
		WithStructuredSerializer = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", SoftObjectPath);

template<>
struct TStructOpsTypeTraits<FSoftClassPath> : public TStructOpsTypeTraitsBase2<FSoftClassPath>
{
	enum
	{
		WithZeroConstructor = true,
		WithSerializer = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", SoftClassPath);

template<>
struct TStructOpsTypeTraits<FPrimaryAssetType> : public TStructOpsTypeTraitsBase2<FPrimaryAssetType>
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", PrimaryAssetType);

template<>
struct TStructOpsTypeTraits<FPrimaryAssetId> : public TStructOpsTypeTraitsBase2<FPrimaryAssetId>
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", PrimaryAssetId);

template<>
struct TStructOpsTypeTraits<FFallbackStruct> : public TStructOpsTypeTraitsBase2<FFallbackStruct>
{
};
UE_IMPLEMENT_STRUCT("/Script/CoreUObject", FallbackStruct);

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

constexpr FAsciiSet AlphaNumericChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

FORCEINLINE constexpr bool IsValidTokenStart(TCHAR FirstChar, bool bDottedNames)
{
	return AlphaNumericChars.Test(FirstChar) || (bDottedNames && FirstChar == '/') || FirstChar > 255;
}

FORCEINLINE constexpr FStringView ParsePropertyToken(const TCHAR* Str, bool DottedNames)
{
	constexpr FAsciiSet RegularTokenChars = AlphaNumericChars  + '_' + '-' + '+';
	constexpr FAsciiSet RegularNonTokenChars = ~RegularTokenChars;
	constexpr FAsciiSet DottedNonTokenChars = ~(RegularTokenChars + '.' + '/' + (char)SUBOBJECT_DELIMITER_CHAR);
	FAsciiSet CurrentNonTokenChars = DottedNames ? DottedNonTokenChars : RegularNonTokenChars;

	const TCHAR* TokenEnd = FAsciiSet::FindFirstOrEnd(Str, CurrentNonTokenChars);
	return FStringView(Str, UE_PTRDIFF_TO_INT32(TokenEnd - Str));
}

//
// Parse a token.
//
const TCHAR* FPropertyHelpers::ReadToken( const TCHAR* Buffer, FString& String, bool bDottedNames)
{
	if( *Buffer == TCHAR('"') )
	{
		int32 NumCharsRead = 0;
		if (!FParse::QuotedString(Buffer, String, &NumCharsRead))
		{
			UE_LOG(LogProperty, Warning, TEXT("ReadToken: Bad quoted string: %s"), Buffer );
			return nullptr;
		}
		Buffer += NumCharsRead;
	}
	else if (IsValidTokenStart(*Buffer, bDottedNames))
	{
		FStringView Token = ParsePropertyToken(Buffer, bDottedNames);
		String += Token;
		Buffer += Token.Len();
	}
	else
	{
		// Get just one.
		String += *Buffer;
	}
	return Buffer;
}

const TCHAR* FPropertyHelpers::ReadToken( const TCHAR* Buffer, FStringBuilderBase& Out, bool bDottedNames)
{
	if( *Buffer == TCHAR('"') )
	{
		int32 NumCharsRead = 0;
		if (!FParse::QuotedString(Buffer, Out, &NumCharsRead))
		{
			UE_LOG(LogProperty, Warning, TEXT("ReadToken: Bad quoted string: %s"), Buffer );
			return nullptr;
		}
		Buffer += NumCharsRead;

		// TODO special handling of null-terminator here?
	}
	else if (IsValidTokenStart(*Buffer, bDottedNames))
	{
		FStringView Token = ParsePropertyToken(Buffer, bDottedNames);
		Out << Token;
		Buffer += Token.Len();
	}
	else
	{
		// Get just one.
		if (*Buffer)
		{
			Out << *Buffer;
		}
	}
	return Buffer;
}

FString FGCStackSizeHelper::GetPropertyPath() const
{
	FString Result;
	const FProperty* PreviousProperty = nullptr;
	const TCHAR DelimiterChar = TEXT('.');

	for (int32 PropertyIndex = 0; PropertyIndex < PropertyStack.Num(); ++PropertyIndex)
	{
		const FProperty* Property = PropertyStack[PropertyIndex];
		check(Property);
		if (PropertyIndex > 0)
		{
			if (Property->GetOwner<FProperty>() == PreviousProperty && Property->GetFName() == PreviousProperty->GetFName())
			{
				// Skipping inner properties (inside of containers) if their name matches their owner name - TArrayName.TArrayName doesn't have much value
				// but we do want to keep TMapName.TMapName_Key
				continue;
			}
			Result += DelimiterChar;
		}
		Result += Property->GetName();
		PreviousProperty = Property;
	}
	return Result;
}

bool FGCStackSizeHelper::ConvertPathToProperties(UClass* ObjectClass, const FName& InPropertyPath, TArray<FProperty*>& OutProperties)
{
	const TCHAR DelimiterChar = TEXT('.');
	FString PropertyNameOrPath = InPropertyPath.ToString();
	int32 DelimiterIndex = -1;
	bool bFullPathConstructed = true;

	if (!PropertyNameOrPath.FindChar(DelimiterChar, DelimiterIndex))
	{
		// 99% of the time we're be dealing with just a single property
		FProperty* FoundProperty = ObjectClass->FindPropertyByName(*PropertyNameOrPath);
		if (FoundProperty)
		{
			OutProperties.Add(FoundProperty);
		}
		else
		{
			bFullPathConstructed = false;
		}
	}
	else
	{
		// Try and find the first property as we can't start processing the rest of the path without it
		FString PropertyName = PropertyNameOrPath.Left(DelimiterIndex);
		FProperty* FoundProperty = ObjectClass->FindPropertyByName(*PropertyName);
		if (FoundProperty)
		{
			OutProperties.Add(FoundProperty);

			int32 StartIndex = DelimiterIndex + 1;
			const TCHAR DelimiterStr[] = { DelimiterChar, TEXT('\0') };
			do
			{
				// Determine the next property name
				DelimiterIndex = PropertyNameOrPath.Find(DelimiterStr, ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIndex);
				PropertyName = PropertyNameOrPath.Mid(StartIndex, DelimiterIndex >= 0 ? (DelimiterIndex - StartIndex) : (PropertyNameOrPath.Len() - StartIndex));

				if (FStructProperty* StructProp = CastField<FStructProperty>(FoundProperty))
				{
					// If the previous property was a struct property, the next one belongs to the struct the previous property represented
					FoundProperty = StructProp->Struct->FindPropertyByName(*PropertyName);
				}
				else
				{
					// In all other case (though in reality it should only be a TMap) find the inner property
					FoundProperty = CastField<FProperty>(FoundProperty->GetInnerFieldByName(*PropertyName));
				}

				if (FoundProperty)
				{
					OutProperties.Add(FoundProperty);
				}
				else
				{
					bFullPathConstructed = false;
				}
			} while (DelimiterIndex >= 0 && bFullPathConstructed);
		}
		else
		{
			bFullPathConstructed = false;
		}
	}
	return bFullPathConstructed;
}

/*-----------------------------------------------------------------------------
	FProperty implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_FIELD(FProperty)

//
// Constructors.
//
FProperty::FProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FField(InOwner, InName, InObjectFlags)
	, ArrayDim(1)
	, ElementSize(0)
	, PropertyFlags(CPF_None)
	, RepIndex(0)
	, BlueprintReplicationCondition(COND_None)
	, Offset_Internal(0)
	, PropertyLinkNext(nullptr)
	, NextRef(nullptr)
	, DestructorLinkNext(nullptr)
	, PostConstructLinkNext(nullptr)
{
}

FProperty::FProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
	: FField(InOwner, InName, InObjectFlags)
	, ArrayDim(1)
	, ElementSize(0)
	, PropertyFlags(InFlags)
	, RepIndex(0)
	, BlueprintReplicationCondition(COND_None)
	, Offset_Internal(InOffset)
	, PropertyLinkNext(nullptr)
	, NextRef(nullptr)
	, DestructorLinkNext(nullptr)
	, PostConstructLinkNext(nullptr)
{
	Init();
}

FProperty::FProperty(FFieldVariant InOwner, const UECodeGen_Private::FPropertyParamsBaseWithOffset& Prop, EPropertyFlags AdditionalPropertyFlags /*= CPF_None*/)
	: FField(InOwner, UTF8_TO_TCHAR(Prop.NameUTF8), Prop.ObjectFlags)
	, ArrayDim(1)
	, ElementSize(0)
	, PropertyFlags(Prop.PropertyFlags | AdditionalPropertyFlags)
	, RepIndex(0)
	, BlueprintReplicationCondition(COND_None)
	, Offset_Internal(0)
	, PropertyLinkNext(nullptr)
	, NextRef(nullptr)
	, DestructorLinkNext(nullptr)
	, PostConstructLinkNext(nullptr)
{
	this->Offset_Internal = Prop.Offset;

	Init();
}

FProperty::FProperty(FFieldVariant InOwner, const UECodeGen_Private::FPropertyParamsBaseWithoutOffset& Prop, EPropertyFlags AdditionalPropertyFlags /*= CPF_None*/)
	: FField(InOwner, UTF8_TO_TCHAR(Prop.NameUTF8), Prop.ObjectFlags)
	, ArrayDim(1)
	, ElementSize(0)
	, PropertyFlags(Prop.PropertyFlags | AdditionalPropertyFlags)
	, RepIndex(0)
	, BlueprintReplicationCondition(COND_None)
	, Offset_Internal(0)
	, PropertyLinkNext(nullptr)
	, NextRef(nullptr)
	, DestructorLinkNext(nullptr)
	, PostConstructLinkNext(nullptr)
{
	Init();
}

#if WITH_EDITORONLY_DATA
FProperty::FProperty(UField* InField)
	: Super(InField)
	, PropertyLinkNext(nullptr)
	, NextRef(nullptr)
	, DestructorLinkNext(nullptr)
	, PostConstructLinkNext(nullptr)
{
	UProperty* SourceProperty = CastChecked<UProperty>(InField);
	ArrayDim = SourceProperty->ArrayDim;
	ElementSize = SourceProperty->ElementSize;
	PropertyFlags = SourceProperty->PropertyFlags;
	RepIndex = SourceProperty->RepIndex;
	Offset_Internal = SourceProperty->Offset_Internal;
	BlueprintReplicationCondition = SourceProperty->BlueprintReplicationCondition;
}
#endif // WITH_EDITORONLY_DATA

void FProperty::Init()
{
#if !WITH_EDITORONLY_DATA
	//@todo.COOKER/PACKAGER: Until we have a cooker/packager step, this can fire when WITH_EDITORONLY_DATA is not defined!
	//	checkSlow(!HasAnyPropertyFlags(CPF_EditorOnly));
#endif // WITH_EDITORONLY_DATA
	checkSlow(GetOwnerUField()->HasAllFlags(RF_Transient));
	checkSlow(HasAllFlags(RF_Transient));

	if (GetOwner<UObject>())
	{
		UField* OwnerField = GetOwnerChecked<UField>();
		OwnerField->AddCppProperty(this);
	}
	else
	{
		FField* OwnerField = GetOwnerChecked<FField>();
		OwnerField->AddCppProperty(this);
	}
}

//
// Serializer.
//
void FProperty::Serialize( FArchive& Ar )
{
	// Make sure that we aren't saving a property to a package that shouldn't be serialised.
#if WITH_EDITORONLY_DATA
	check(!Ar.IsFilterEditorOnly() || !IsEditorOnlyProperty());
#endif // WITH_EDITORONLY_DATA

	Super::Serialize(Ar);

	Ar << ArrayDim;
	Ar << ElementSize;

	EPropertyFlags SaveFlags = PropertyFlags & ~CPF_ComputedFlags;
	// Archive the basic info.
	Ar << (uint64&)SaveFlags;
	if (Ar.IsLoading())
	{
		PropertyFlags = (SaveFlags & ~CPF_ComputedFlags) | (PropertyFlags & CPF_ComputedFlags);
	}

	if (FPlatformProperties::HasEditorOnlyData() == false)
	{
		// Make sure that we aren't saving a property to a package that shouldn't be serialised.
		check( !IsEditorOnlyProperty() );
	}

	Ar << RepIndex;
	Ar << RepNotifyFunc;

	if (Ar.IsLoading())
	{
		Offset_Internal = 0;
		DestructorLinkNext = nullptr;
	}

	Ar << BlueprintReplicationCondition;
}

void FProperty::PostDuplicate(const FField& InField)
{
	const FProperty& Source = static_cast<const FProperty&>(InField);
	ArrayDim = Source.ArrayDim;
	ElementSize = Source.ElementSize;
	PropertyFlags = Source.PropertyFlags;
	RepIndex = Source.RepIndex;
	Offset_Internal = Source.Offset_Internal;
	RepNotifyFunc = Source.RepNotifyFunc;
	BlueprintReplicationCondition = Source.BlueprintReplicationCondition;

	Super::PostDuplicate(InField);
}

void FProperty::CopySingleValueToScriptVM( void* Dest, void const* Src ) const
{
	CopySingleValue(Dest, Src);
}

void FProperty::CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const
{
	CopyCompleteValue(Dest, Src);
}

void FProperty::CopySingleValueFromScriptVM( void* Dest, void const* Src ) const
{
	CopySingleValue(Dest, Src);
}

void FProperty::CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const
{
	CopyCompleteValue(Dest, Src);
}

void FProperty::ClearValueInternal( void* Data ) const
{
	checkf(0, TEXT("%s failed to handle ClearValueInternal, but it was not CPF_NoDestructor | CPF_ZeroConstructor"), *GetFullName());
}
void FProperty::DestroyValueInternal( void* Dest ) const
{
	checkf(0, TEXT("%s failed to handle DestroyValueInternal, but it was not CPF_NoDestructor"), *GetFullName());
}
void FProperty::InitializeValueInternal( void* Dest ) const
{
	checkf(0, TEXT("%s failed to handle InitializeValueInternal, but it was not CPF_ZeroConstructor"), *GetFullName());
}

/**
 * Verify that modifying this property's value via ImportText is allowed.
 * 
 * @param	PortFlags	the flags specified in the call to ImportText
 *
 * @return	true if ImportText should be allowed
 */
bool FProperty::ValidateImportFlags( uint32 PortFlags, FOutputDevice* ErrorHandler ) const
{
	// PPF_RestrictImportTypes is set when importing defaultproperties; it indicates that
	// we should not allow config/localized properties to be imported here
	if ((PortFlags & PPF_RestrictImportTypes) && (PropertyFlags & CPF_Config))
	{
		FString ErrorMsg = FString::Printf(TEXT("Import failed for '%s': property is config (Check to see if the property is listed in the DefaultProperties.  It should only be listed in the specific .ini file)"), *GetName());

		if (ErrorHandler)
		{
			ErrorHandler->Logf(TEXT("%s"), *ErrorMsg);
		}
		else
		{
			UE_LOG(LogProperty, Warning, TEXT("%s"), *ErrorMsg);
		}

		return false;
	}

	return true;
}

FString FProperty::GetNameCPP() const
{
	return HasAnyPropertyFlags(CPF_Deprecated) ? GetName() + TEXT("_DEPRECATED") : GetName();
}

FString FProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = TEXT("F");
	ExtendedTypeText += GetClass()->GetName();
	return TEXT("PROPERTY");
}

bool FProperty::PassCPPArgsByRef() const
{
	return false;
}


void FProperty::ExportCppDeclaration(FOutputDevice& Out, EExportedDeclaration::Type DeclarationType, const TCHAR* ArrayDimOverride, uint32 AdditionalExportCPPFlags
	, bool bSkipParameterName, const FString* ActualCppType, const FString* ActualExtendedType, const FString* ActualParameterName) const
{
	const bool bIsParameter = (DeclarationType == EExportedDeclaration::Parameter) || (DeclarationType == EExportedDeclaration::MacroParameter);
	const bool bIsInterfaceProp = CastField<const FInterfaceProperty>(this) != nullptr;

	// export the property type text (e.g. FString; int32; TArray, etc.)
	FString ExtendedTypeText;
	const uint32 ExportCPPFlags = AdditionalExportCPPFlags | (bIsParameter ? CPPF_ArgumentOrReturnValue : 0);
	FString TypeText;
	if (ActualCppType)
	{
		TypeText = *ActualCppType;
	}
	else
	{
		TypeText = GetCPPType(&ExtendedTypeText, ExportCPPFlags);
	}

	if (ActualExtendedType)
	{
		ExtendedTypeText = *ActualExtendedType;
	}

	const bool bCanHaveRef = 0 == (AdditionalExportCPPFlags & CPPF_NoRef);
	const bool bCanHaveConst = 0 == (AdditionalExportCPPFlags & CPPF_NoConst);
	if (!CastField<const FBoolProperty>(this) && bCanHaveConst) // can't have const bitfields because then we cannot determine their offset and mask from the compiler
	{
		const FObjectProperty* ObjectProp = CastField<FObjectProperty>(this);

		// export 'const' for parameters
		const bool bIsConstParam   = bIsParameter && (HasAnyPropertyFlags(CPF_ConstParm) || (bIsInterfaceProp && !HasAllPropertyFlags(CPF_OutParm)));
		const bool bIsOnConstClass = ObjectProp && ObjectProp->PropertyClass && ObjectProp->PropertyClass->HasAnyClassFlags(CLASS_Const);
		const bool bShouldHaveRef = bCanHaveRef && HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm);

		const bool bConstAtTheBeginning = bIsOnConstClass || (bIsConstParam && !bShouldHaveRef);
		if (bConstAtTheBeginning)
		{
			TypeText = FString::Printf(TEXT("const %s"), *TypeText);
		}

		const UClass* const MyPotentialConstClass = (DeclarationType == EExportedDeclaration::Member) ? GetOwner<UClass>() : nullptr;
		const bool bFromConstClass = MyPotentialConstClass && MyPotentialConstClass->HasAnyClassFlags(CLASS_Const);
		const bool bConstAtTheEnd = bFromConstClass || (bIsConstParam && bShouldHaveRef);
		if (bConstAtTheEnd)
		{
			ExtendedTypeText += TEXT(" const");
		}
	}

	FString NameCpp;
	if (!bSkipParameterName)
	{
		ensure((0 == (AdditionalExportCPPFlags & CPPF_BlueprintCppBackend)) || ActualParameterName);
		NameCpp = ActualParameterName ? *ActualParameterName : GetNameCPP();
	}
	if (DeclarationType == EExportedDeclaration::MacroParameter)
	{
		NameCpp = FString(TEXT(", ")) + NameCpp;
	}

	TCHAR ArrayStr[MAX_SPRINTF] = {};
	const bool bExportStaticArray = 0 == (CPPF_NoStaticArray & AdditionalExportCPPFlags);
	if ((ArrayDim != 1) && bExportStaticArray)
	{
		if (ArrayDimOverride)
		{
			FCString::Sprintf( ArrayStr, TEXT("[%s]"), ArrayDimOverride );
		}
		else
		{
			FCString::Sprintf( ArrayStr, TEXT("[%i]"), ArrayDim );
		}
	}

	if(auto BoolProperty = CastField<const FBoolProperty>(this) )
	{
		// if this is a member variable, export it as a bitfield
		if( ArrayDim==1 && DeclarationType == EExportedDeclaration::Member )
		{
			bool bCanUseBitfield = !BoolProperty->IsNativeBool();
			// export as a uint32 member....bad to hardcode, but this is a special case that won't be used anywhere else
			Out.Logf(TEXT("%s%s %s%s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr, bCanUseBitfield ? TEXT(":1") : TEXT(""));
		}

		//@todo we currently can't have out bools.. so this isn't really necessary, but eventually out bools may be supported, so leave here for now
		else if( bIsParameter && HasAnyPropertyFlags(CPF_OutParm) )
		{
			// export as a reference
			Out.Logf(TEXT("%s%s%s %s%s"), *TypeText, *ExtendedTypeText
				, bCanHaveRef ? TEXT("&") : TEXT("")
				, *NameCpp, ArrayStr);
		}

		else
		{
			Out.Logf(TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr);
		}
	}
	else 
	{
		if ( bIsParameter )
		{
			if ( ArrayDim > 1 )
			{
				// export as a pointer
				//Out.Logf( TEXT("%s%s* %s"), *TypeText, *ExtendedTypeText, *GetNameCPP() );
				// don't export as a pointer
				Out.Logf(TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr);
			}
			else
			{
				if ( PassCPPArgsByRef() )
				{
					// export as a reference (const ref if it isn't an out parameter)
					Out.Logf(TEXT("%s%s%s%s %s"),
						(bCanHaveConst && !HasAnyPropertyFlags(CPF_OutParm | CPF_ConstParm)) ? TEXT("const ") : TEXT(""),
						*TypeText, *ExtendedTypeText,
						bCanHaveRef ? TEXT("&") : TEXT(""),
						*NameCpp);
				}
				else
				{
					// export as a pointer if this is an optional out parm, reference if it's just an out parm, standard otherwise...
					TCHAR ModifierString[2] = { TCHAR('\0'), TCHAR('\0') };
					if (bCanHaveRef && (HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm) || bIsInterfaceProp))
					{
						ModifierString[0] = TEXT('&');
					}
					Out.Logf(TEXT("%s%s%s %s%s"), *TypeText, *ExtendedTypeText, ModifierString, *NameCpp, ArrayStr);
				}
			}
		}
		else
		{
			Out.Logf(TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *NameCpp, ArrayStr);
		}
	}
}

bool FProperty::ExportText_Direct
	(
	FString&	ValueStr,
	const void*	Data,
	const void*	Delta,
	UObject*	Parent,
	int32		PortFlags,
	UObject*	ExportRootScope
	) const
{
	if( Data==Delta || !Identical(Data,Delta,PortFlags) )
	{
		ExportText_Internal
			(
			ValueStr,
			(uint8*)Data,
			EPropertyPointerType::Direct,
			(uint8*)Delta,
			Parent,
			PortFlags,
			ExportRootScope
			);
		return true;
	}

	return false;
}

bool FProperty::ShouldSerializeValue( FArchive& Ar ) const
{
	if (Ar.ShouldSkipProperty(this))
	{
		return false;
	}

	if (!(PropertyFlags & CPF_SaveGame) && Ar.IsSaveGame())
	{
		return false;
	}

	const uint64 SkipFlags = CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient | CPF_NonTransactional | CPF_Deprecated | CPF_DevelopmentAssets | CPF_SkipSerialization;
	if (!(PropertyFlags & SkipFlags))
	{
		return true;
	}

	bool Skip =
			((PropertyFlags & CPF_Transient) && Ar.IsPersistent() && !Ar.IsSerializingDefaults())
		||	((PropertyFlags & CPF_DuplicateTransient) && (Ar.GetPortFlags() & PPF_Duplicate))
		||	((PropertyFlags & CPF_NonPIEDuplicateTransient) && !(Ar.GetPortFlags() & PPF_DuplicateForPIE) && (Ar.GetPortFlags() & PPF_Duplicate))
		||	((PropertyFlags & CPF_NonTransactional) && Ar.IsTransacting())
		||	((PropertyFlags & CPF_Deprecated) && !Ar.HasAllPortFlags(PPF_UseDeprecatedProperties) && (Ar.IsSaving() || Ar.IsTransacting() || Ar.WantBinaryPropertySerialization()))
		||  ((PropertyFlags & CPF_SkipSerialization) && (Ar.WantBinaryPropertySerialization() || !Ar.HasAllPortFlags(PPF_ForceTaggedSerialization)))
		||  (IsEditorOnlyProperty() && Ar.IsFilterEditorOnly());

	return !Skip;
}


//
// Net serialization.
//
bool FProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	SerializeItem( FStructuredArchiveFromArchive(Ar).GetSlot(), Data, NULL );
	return 1;
}

bool FProperty::SupportsNetSharedSerialization() const
{
	return true;
}

#if WITH_EDITORONLY_DATA
void FProperty::AppendSchemaHash(FBlake3& Builder, bool bSkipEditorOnly) const
{
	AppendHash(Builder, NamePrivate);
	Builder.Update(&ArrayDim, sizeof(ArrayDim));
	AppendHash(Builder, GetID());
}
#endif


//
// Return whether the property should be exported.
//
bool FProperty::ShouldPort( uint32 PortFlags/*=0*/ ) const
{
	// if no size, don't export
	if (GetSize() <= 0)
	{
		return false;
	}

	if (HasAnyPropertyFlags(CPF_Deprecated) && !(PortFlags & (PPF_ParsingDefaultProperties | PPF_UseDeprecatedProperties)))
	{
		return false;
	}

	// if we're parsing default properties or the user indicated that transient properties should be included
	if (HasAnyPropertyFlags(CPF_Transient) && !(PortFlags & (PPF_ParsingDefaultProperties | PPF_IncludeTransient)))
	{
		return false;
	}

	// if we're copying, treat DuplicateTransient as transient
	if ((PortFlags & PPF_Copy) && HasAnyPropertyFlags(CPF_DuplicateTransient | CPF_TextExportTransient) && !(PortFlags & (PPF_ParsingDefaultProperties | PPF_IncludeTransient)))
	{
		return false;
	}

	// if we're not copying for PIE and NonPIETransient is set, don't export
	if (!(PortFlags & PPF_DuplicateForPIE) && HasAnyPropertyFlags(CPF_NonPIEDuplicateTransient))
	{
		return false;
	}

	// if we're only supposed to export components and this isn't a component property, don't export
	if ((PortFlags & PPF_SubobjectsOnly) && !ContainsInstancedObjectProperty())
	{
		return false;
	}

	// hide non-Edit properties when we're exporting for the property window
	if ((PortFlags & PPF_PropertyWindow) && !(PropertyFlags & CPF_Edit))
	{
		return false;
	}

	return true;
}

//
// Return type id for encoding properties in .u files.
//
FName FProperty::GetID() const
{
	return GetClass()->GetFName();
}

void FProperty::InstanceSubobjects( void* Data, void const* DefaultData, UObject* InOwner, struct FObjectInstancingGraph* InstanceGraph )
{
}

int32 FProperty::GetMinAlignment() const
{
	return 1;
}


//
// Link property loaded from file.
//
void FProperty::LinkInternal(FArchive& Ar)
{
	check(0); // Link shouldn't call super...and we should never link an abstract property, like this base class
}

EConvertFromTypeResult FProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct)
{
	return EConvertFromTypeResult::UseSerializeItem;
}


int32 FProperty::SetupOffset()
{
	UObject* OwnerUObject = GetOwner<UObject>();
	if (OwnerUObject && (OwnerUObject->GetClass()->ClassCastFlags & CASTCLASS_UStruct))
	{
		UStruct* OwnerStruct = (UStruct*)OwnerUObject;
		Offset_Internal = Align(OwnerStruct->GetPropertiesSize(), GetMinAlignment());
	}
	else
	{
		Offset_Internal = Align(0, GetMinAlignment());
	}
	return Offset_Internal + GetSize();
}

void FProperty::SetOffset_Internal(int32 NewOffset)
{
	Offset_Internal = NewOffset;
}

bool FProperty::SameType(const FProperty* Other) const
{
	return Other && (GetClass() == Other->GetClass());
}

void* FProperty::AllocateAndInitializeValue() const
{
	void* Memory = (uint8*)FMemory::MallocZeroed(GetSize(), GetMinAlignment());
	if (!HasAnyPropertyFlags(CPF_ZeroConstructor)) // this stuff is already zero
	{
		InitializeValue(Memory);
	}
	return Memory;
}

void FProperty::DestroyAndFreeValue(void* InMemory) const
{
	if (InMemory)
	{
		DestroyValue(InMemory);
		FMemory::Free(InMemory);
	}
}

void* FProperty::GetValueAddressAtIndex_Direct(const FProperty* Inner, void* InValueAddress, int32 Index) const
{
	checkf(Inner == nullptr, TEXT("%s should not have an inner property or it's missing specialized GetValueAddressAtIndex_Direct override"), *GetFullName());
	checkf(Index < ArrayDim && Index >= 0, TEXT("Array index (%d) out of range"), Index);
	return (uint8*)InValueAddress + ElementSize * Index;
}

void FProperty::SetSingleValue_InContainer(void* InContainer, const void* InValue, int32 ArrayIndex) const
{
	checkf(ArrayIndex <= ArrayDim, TEXT("ArrayIndex (%d) must be less than the property %s array size (%d)"), ArrayIndex, *GetFullName(), ArrayDim);
	if (!HasSetter())
	{
		// Fast path - direct memory access
		CopySingleValue(ContainerVoidPtrToValuePtrInternal((void*)InContainer, ArrayIndex), InValue);
	}
	else
	{
		if (ArrayDim == 1)
		{
			// Slower but no mallocs. We can copy the value directly to the resulting param
			CallSetter(InContainer, InValue);
		}
		else
		{
			// Malloc a temp value that is the size of the array. We will then copy the entire array to the temp value
			uint8* ValueArray = (uint8*)AllocateAndInitializeValue();
			GetValue_InContainer(InContainer, ValueArray);
			// Replace the value at the specified index in the temp array with the InValue
			CopySingleValue(ValueArray + ArrayIndex * ElementSize, InValue);
			// Now call a setter to replace the entire array and then destroy the temp value
			CallSetter(InContainer, ValueArray);
			DestroyAndFreeValue(ValueArray);
		}
	}
}

void FProperty::GetSingleValue_InContainer(const void* InContainer, void* OutValue, int32 ArrayIndex) const
{
	checkf(ArrayIndex <= ArrayDim, TEXT("ArrayIndex (%d) must be less than the property %s array size (%d)"), ArrayIndex, *GetFullName(), ArrayDim);
	if (!HasGetter())
	{
		// Fast path - direct memory access
		CopySingleValue(OutValue, ContainerVoidPtrToValuePtrInternal((void*)InContainer, ArrayIndex));
	}
	else
	{
		if (ArrayDim == 1)
		{
			// Slower but no mallocs. We can copy the value directly to the resulting param
			CallGetter(InContainer, OutValue);
		}
		else
		{
			// Malloc a temp value that is the size of the array. Getter will then copy the entire array to the temp value
			uint8* ValueArray = (uint8*)AllocateAndInitializeValue();
			GetValue_InContainer(InContainer, ValueArray);
			// Copy the item we care about and free the temp array
			CopySingleValue(OutValue, ValueArray + ArrayIndex * ElementSize);
			DestroyAndFreeValue(ValueArray);
		}
	}
}

void FProperty::PerformOperationWithSetter(void* InContainer, void* DirectPropertyAddress, TFunctionRef<void(void*)> DirectValueAccessFunc) const
{
	if (InContainer && HasSetterOrGetter()) // If there's a getter we need to allocate a temp value even if there's no setter
	{
		// When modifying container or struct properties that have a setter or getter function we first allocate a temp value
		// that we can operate on directly (add new elements or modify existing ones)
		void* LocalValuePtr = AllocateAndInitializeValue();
		// Copy the value to the allocated local (using a getter if present)
		GetValue_InContainer(InContainer, LocalValuePtr);

		// Perform operation on the temp value
		DirectValueAccessFunc(LocalValuePtr);

		// Assign the temp value back to the property using a setter function
		SetValue_InContainer(InContainer, LocalValuePtr);
		// Destroy and free the temp value
		DestroyAndFreeValue(LocalValuePtr);
	}
	else
	{
		// When there's no setter or getter present it's ok to perform the operation directly on the container / struct memory
		if (!DirectPropertyAddress)
		{
			checkf(InContainer, TEXT("Container pointr must be valid if DirectPropertyAddress is not valid"));
			DirectPropertyAddress = PointerToValuePtr(InContainer, EPropertyPointerType::Container);
		}
		DirectValueAccessFunc(DirectPropertyAddress);
	}
}

void FProperty::PerformOperationWithGetter(void* InContainer, const void* DirectPropertyAddress, TFunctionRef<void(const void*)> DirectValueAccessFunc) const
{
	if (InContainer && HasGetter())
	{
		// When modifying container or struct properties that have a getter function we first allocate a temp value
		// that we can operate on directly (add new elements or modify existing ones)
		void* LocalValuePtr = AllocateAndInitializeValue();
		// Copy the value to the allocated local using a getter
		GetValue_InContainer(InContainer, LocalValuePtr);

		// Perform read-only operation on the temp value
		DirectValueAccessFunc(LocalValuePtr);

		// Destroy and free the temp value
		DestroyAndFreeValue(LocalValuePtr);
	}
	else
	{
		if (!DirectPropertyAddress)
		{
			checkf(InContainer, TEXT("Container pointr must be valid if DirectPropertyAddress is not valid"));
			DirectPropertyAddress = PointerToValuePtr(InContainer, EPropertyPointerType::Container);
		}
		DirectValueAccessFunc(DirectPropertyAddress);
	}
}

/**
 * Attempts to read an array index (xxx) sequence.  Handles const/enum replacements, etc.
 * @param	ObjectStruct	the scope of the object/struct containing the property we're currently importing
 * @param	Str				[out] pointer to the the buffer containing the property value to import
 * @param	Warn			the output device to send errors/warnings to
 * @return	the array index for this defaultproperties line.  INDEX_NONE if this line doesn't contains an array specifier, or 0 if there was an error parsing the specifier.
 */
static const int32 ReadArrayIndex(UStruct* ObjectStruct, const TCHAR*& Str, FOutputDevice* Warn)
{
	const TCHAR* Start = Str;
	int32 Index = INDEX_NONE;
	SkipWhitespace(Str);

	if (*Str == '(' || *Str == '[')
	{
		Str++;
		FString IndexText(TEXT(""));
		while ( *Str && *Str != ')' && *Str != ']' )
		{
			if ( *Str == TCHAR('=') )
			{
				// we've encountered an equals sign before the closing bracket
				Warn->Logf(ELogVerbosity::Warning, TEXT("Missing ')' in default properties subscript: %s"), Start);
				return 0;
			}

			IndexText += *Str++;
		}

		if ( *Str++ )
		{
			if (IndexText.Len() > 0 )
			{
				if (FChar::IsAlpha(IndexText[0]))
				{
					FName IndexTokenName = FName(*IndexText, FNAME_Find);
					if (IndexTokenName != NAME_None)
					{
						// Search for the enum in question.
						Index = UEnum::LookupEnumName(FName(), IndexTokenName, EFindFirstObjectOptions::NativeFirst /* Only native enums can be used as array indices */);
						if (Index == INDEX_NONE)
						{
							Index = 0;
							Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
						}
					}
					else
					{
						Index = 0;

						// unknown or invalid identifier specified for array subscript
						Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
					}
				}
				else if (FChar::IsDigit(IndexText[0]))
				{
					Index = FCString::Atoi(*IndexText);
				}
				else
				{
					// unknown or invalid identifier specified for array subscript
					Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
				}
			}
			else
			{
				Index = 0;

				// nothing was specified between the opening and closing parenthesis
				Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid subscript in default properties: %s"), Start);
			}
		}
		else
		{
			Index = 0;
			Warn->Logf(ELogVerbosity::Warning, TEXT("Missing ')' in default properties subscript: %s"), Start );
		}
	}
	return Index;
}

/** 
 * Do not attempt to import this property if there is no value for it - i.e. (Prop1=,Prop2=)
 * This normally only happens for empty strings or empty dynamic arrays, and the alternative
 * is for strings and dynamic arrays to always export blank delimiters, such as Array=() or String="", 
 * but this tends to cause problems with inherited property values being overwritten, especially in the localization 
 * import/export code

 * The safest way is to interpret blank delimiters as an indication that the current value should be overwritten with an empty
 * value, while the lack of any value or delimiter as an indication to not import this property, thereby preventing any current
 * values from being overwritten if this is not the intent.

 * Thus, arrays and strings will only export empty delimiters when overriding an inherited property's value with an 
 * empty value.
 */
static bool IsPropertyValueSpecified( const TCHAR* Buffer )
{
	return Buffer && *Buffer && *Buffer != TCHAR(',') && *Buffer != TCHAR(')');
}

const TCHAR* FProperty::ImportSingleProperty( const TCHAR* Str, void* DestData, UStruct* ObjectStruct, UObject* SubobjectOuter, int32 PortFlags,
											FOutputDevice* Warn, TArray<FDefinedProperty>& DefinedProperties )
{
	check(ObjectStruct);

	constexpr FAsciiSet Whitespaces(" \t");
	constexpr FAsciiSet Delimiters("=([.");

	// strip leading whitespace
	const TCHAR* Start = FAsciiSet::Skip(Str, Whitespaces);
	// find first delimiter
	Str = FAsciiSet::FindFirstOrEnd(Start, Delimiters);
	// check if delimiter was found...
	if (*Str)
	{
		// strip trailing whitespace
		int32 Len = UE_PTRDIFF_TO_INT32(Str - Start);
		while (Len > 0 && Whitespaces.Contains(Start[Len - 1]))
		{
			--Len;
		}

		const FName PropertyName(Len, Start);
		FProperty* Property = FindFProperty<FProperty>(ObjectStruct, PropertyName);

		if (Property == nullptr)
		{
			// Check for redirects
			FName NewPropertyName = FindRedirectedPropertyName(ObjectStruct, PropertyName);

			if (NewPropertyName != NAME_None)
			{
				Property = FindFProperty<FProperty>(ObjectStruct, NewPropertyName);
			}

			if (!Property)
			{
				Property = ObjectStruct->CustomFindProperty(PropertyName);
			}
		}		

		if (Property == NULL)
		{
			UE_SUPPRESS(LogExec, Verbose, Warn->Logf(TEXT("Unknown property in %s: %s "), *ObjectStruct->GetName(), Start));
			return Str;
		}

		if (!Property->ShouldPort(PortFlags))
		{
			UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Cannot perform text import on property '%s' here: %s"), *Property->GetName(), Start));
			return Str;
		}

		// Parse an array operation, if present.
		enum EArrayOp
		{
			ADO_None,
			ADO_Add,
			ADO_Remove,
			ADO_RemoveIndex,
			ADO_Empty,
		};

		EArrayOp ArrayOp = ADO_None;
		if(*Str == '.')
		{
			Str++;
			if(FParse::Command(&Str,TEXT("Empty")))
			{
				ArrayOp = ADO_Empty;
			}
			else if(FParse::Command(&Str,TEXT("Add")))
			{
				ArrayOp = ADO_Add;
			}
			else if(FParse::Command(&Str,TEXT("Remove")))
			{
				ArrayOp = ADO_Remove;
			}
			else if (FParse::Command(&Str,TEXT("RemoveIndex")))
			{
				ArrayOp = ADO_RemoveIndex;
			}
		}

		FArrayProperty* const ArrayProperty = ExactCastField<FArrayProperty>(Property);
		FMulticastDelegateProperty* const MulticastDelegateProperty = CastField<FMulticastDelegateProperty>(Property);
		if( MulticastDelegateProperty != NULL && ArrayOp != ADO_None )
		{
			// Allow Add(), Remove() and Empty() on multi-cast delegates
			if( ArrayOp == ADO_Add || ArrayOp == ADO_Remove || ArrayOp == ADO_Empty )
			{
				SkipWhitespace(Str);
				if(*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties multi-cast delegate operation: %s"), Start));
					return Str;
				}
				SkipWhitespace(Str);

				if( ArrayOp == ADO_Empty )
				{
					// Clear out the delegate
					MulticastDelegateProperty->ClearDelegate(SubobjectOuter, Property->ContainerPtrToValuePtr<void>(DestData));
				}
				else
				{
					FStringOutputDevice ImportError;

					const TCHAR* Result = NULL;
					if( ArrayOp == ADO_Add )
					{
						// Add a function to a multi-cast delegate
						Result = MulticastDelegateProperty->ImportText_Add(Str, Property->ContainerPtrToValuePtr<void>(DestData), PortFlags, SubobjectOuter, &ImportError);
					}
					else if( ArrayOp == ADO_Remove )
					{
						// Remove a function from a multi-cast delegate
						Result = MulticastDelegateProperty->ImportText_Remove(Str, Property->ContainerPtrToValuePtr<void>(DestData), PortFlags, SubobjectOuter, &ImportError);
					}
				
					// Spit any error we had while importing property
					if (ImportError.Len() > 0)
					{
						TArray<FString> ImportErrors;
						ImportError.ParseIntoArray(ImportErrors, LINE_TERMINATOR, true);

						for ( int32 ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s"), *ImportErrors[ErrorIndex]);
						}
					}
					else if (Result == NULL || Result == Str)
					{
						Warn->Logf(ELogVerbosity::Warning, TEXT("Unable to parse parameter value '%s' in defaultproperties multi-cast delegate operation: %s"), Str, Start);
					}
					// in the failure case, don't return NULL so the caller can potentially skip less and get values further in the string
					if (Result != NULL)
					{
						Str = Result;
					}

				}
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Unsupported operation on multi-cast delegate variable: %s"), Start));
				return Str;
			}
			SkipWhitespace(Str);
			if (*Str != ')')
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing ')' in default properties multi-cast delegate operation: %s"), Start));
				return Str;
			}
			Str++;
		}
		else if (ArrayOp != ADO_None)
		{
			if (ArrayProperty == NULL)
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Array operation performed on non-array variable: %s"), Start));
				return Str;
			}

			FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, DestData);
			if (ArrayOp == ADO_Empty)
			{
				ArrayHelper.EmptyValues();
				SkipWhitespace(Str);
				if (*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties array operation: %s"), Start));
					return Str;
				}
			}
			else if (ArrayOp == ADO_Add || ArrayOp == ADO_Remove)
			{
				SkipWhitespace(Str);
				if(*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties array operation: %s"), Start));
					return Str;
				}
				SkipWhitespace(Str);

				if(ArrayOp == ADO_Add)
				{
					int32	Index = ArrayHelper.AddValue();

					const TCHAR* Result = ArrayProperty->Inner->ImportText_Direct(Str, ArrayHelper.GetRawPtr(Index), SubobjectOuter, PortFlags, Warn);
					if ( Result == NULL || Result == Str )
					{
						Warn->Logf(ELogVerbosity::Warning, TEXT("Unable to parse parameter value '%s' in defaultproperties array operation: %s"), Str, Start);
						return Str;
					}
					else
					{
						Str = Result;
					}
				}
				else
				{
					int32 Size = ArrayProperty->Inner->ElementSize;

					uint8* Temp = (uint8*)FMemory_Alloca(Size);
					ArrayProperty->Inner->InitializeValue(Temp);
							
					// export the value specified to a temporary buffer
					const TCHAR* Result = ArrayProperty->Inner->ImportText_Direct(Str, Temp, SubobjectOuter, PortFlags, Warn);
					if ( Result == NULL || Result == Str )
					{
						Warn->Logf(ELogVerbosity::Error, TEXT("Unable to parse parameter value '%s' in defaultproperties array operation: %s"), Str, Start);
						ArrayProperty->Inner->DestroyValue(Temp);
						return Str;
					}
					else
					{
						// find the array member corresponding to this value
						bool bFound = false;
						for(uint32 Index = 0;Index < (uint32)ArrayHelper.Num();Index++)
						{
							const void* ElementDestData = ArrayHelper.GetRawPtr(Index);
							if(ArrayProperty->Inner->Identical(Temp,ElementDestData))
							{
								ArrayHelper.RemoveValues(Index--);
								bFound = true;
							}
						}
						if (!bFound)
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s.Remove(): Value not found in array"), *ArrayProperty->GetName());
						}
						ArrayProperty->Inner->DestroyValue(Temp);
						Str = Result;
					}
				}
			}
			else if (ArrayOp == ADO_RemoveIndex) //-V547
			{
				SkipWhitespace(Str);
				if(*Str++ != '(')
				{
					UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing '(' in default properties array operation:: %s"), Start ));
					return Str;
				}
				SkipWhitespace(Str);

				FString strIdx;
				while (*Str != ')')
				{		
					if (*Str == 0)
					{
						UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing ')' in default properties array operation: %s"), Start));
						return Str;
					}
					strIdx += *Str;
					Str++;
				}
				int32 removeIdx = FCString::Atoi(*strIdx);
				if (ArrayHelper.IsValidIndex(removeIdx))
				{
					ArrayHelper.RemoveValues(removeIdx);
				}
				else
				{
					Warn->Logf(ELogVerbosity::Warning, TEXT("%s.RemoveIndex(%d): Index not found in array"), *ArrayProperty->GetName(), removeIdx);
				}
			}
			SkipWhitespace(Str);
			if (*Str != ')')
			{
				UE_SUPPRESS(LogExec, Warning, Warn->Logf(TEXT("Missing ')' in default properties array operation: %s"), Start));
				return Str;
			}
			Str++;
		}
		else
		{
			// try to read an array index
			int32 Index = ReadArrayIndex(ObjectStruct, Str, Warn);

			// check for out of bounds on static arrays
			if (ArrayProperty == NULL && Index >= Property->ArrayDim)
			{
				Warn->Logf(ELogVerbosity::Warning, TEXT("Out of bound array default property (%i/%i): %s"), Index, Property->ArrayDim, Start);
				return Str;
			}

			// check to see if this property has already imported data
			FDefinedProperty D;
			D.Property = Property;
			D.Index = Index;
			if (DefinedProperties.Find(D) != INDEX_NONE)
			{
				Warn->Logf(ELogVerbosity::Warning, TEXT("redundant data: %s"), Start);
				return Str;
			}
			DefinedProperties.Add(D);

			// strip whitespace before =
			SkipWhitespace(Str);
			if (*Str++ != '=')
			{
				Warn->Logf(ELogVerbosity::Warning, TEXT("Missing '=' in default properties assignment: %s"), Start );
				return Str;
			}
			// strip whitespace after =
			SkipWhitespace(Str);

			if (!IsPropertyValueSpecified(Str) && ArrayProperty == nullptr)
			{
				// if we're not importing default properties for classes (i.e. we're pasting something in the editor or something)
				// and there is no property value for this element, skip it, as that means that the value of this element matches
				// the intrinsic null value of the property type and we want to skip importing it
				return Str;
			}

			// disallow importing of an object's name from here
			// not done above with ShouldPort() check because this is intentionally exported so we don't want it to cause errors on import
			if (Property->GetFName() != NAME_Name || !Property->GetOwnerVariant().IsUObject() || Property->GetOwner<UObject>()->GetFName() != NAME_Object)
			{
				if (Index > -1 && ArrayProperty != NULL) //set single dynamic array element
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, DestData);

					ArrayHelper.ExpandForIndex(Index);

					FStringOutputDevice ImportError;
					const TCHAR* Result = ArrayProperty->Inner->ImportText_Direct(Str, ArrayHelper.GetRawPtr(Index), SubobjectOuter, PortFlags, &ImportError);
					// Spit any error we had while importing property
					if (ImportError.Len() > 0)
					{
						TArray<FString> ImportErrors;
						ImportError.ParseIntoArray(ImportErrors,LINE_TERMINATOR,true);

						for ( int32 ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s"), *ImportErrors[ErrorIndex]);
						}
					}
					else if (Result == Str)
					{
						Warn->Logf(ELogVerbosity::Warning, TEXT("Invalid property value in defaults: %s"), Start);
					}
					// in the failure case, don't return NULL so the caller can potentially skip less and get values further in the string
					if (Result != NULL)
					{
						Str = Result;
					}
				}
				else
				{
					if (Index == INDEX_NONE)
					{
						Index = 0;
					}

					FStringOutputDevice ImportError;

					const TCHAR* Result = Property->ImportText_Direct(Str, Property->ContainerPtrToValuePtr<void>(DestData, Index), SubobjectOuter, PortFlags, &ImportError);
					
					// Spit any error we had while importing property
					if (ImportError.Len() > 0)
					{
						TArray<FString> ImportErrors;
						ImportError.ParseIntoArray(ImportErrors, LINE_TERMINATOR, true);

						for ( int32 ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
						{
							Warn->Logf(ELogVerbosity::Warning, TEXT("%s"), *ImportErrors[ErrorIndex]);
						}
					}
					else if ((Result == NULL && ArrayProperty == nullptr) || Result == Str)
					{
						UE_SUPPRESS(LogExec, Verbose, Warn->Logf(TEXT("Unknown property in %s: %s "), *ObjectStruct->GetName(), Start));
					}
					// in the failure case, don't return NULL so the caller can potentially skip less and get values further in the string
					if (Result != NULL)
					{
						Str = Result;
					}
				}
			}
		}
	}
	return Str;
}

FName FProperty::FindRedirectedPropertyName(UStruct* ObjectStruct, FName OldName)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FProperty::FindRedirectedPropertyName"), STAT_LinkerLoad_FindRedirectedPropertyName, STATGROUP_LoadTimeVerbose);

	// ObjectStruct may be a nested struct, so extract path
	UPackage* StructPackage = ObjectStruct->GetOutermost();
	FName PackageName = StructPackage->GetFName();
	// Avoid GetPathName string allocation and FName initialization when there is only one outer
	FName OuterName = (StructPackage == ObjectStruct->GetOuter()) ? ObjectStruct->GetFName() : FName(*ObjectStruct->GetPathName(StructPackage));

	FCoreRedirectObjectName OldRedirectName(OldName, OuterName, PackageName);
	FCoreRedirectObjectName NewRedirectName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property, OldRedirectName);

	if (NewRedirectName != OldRedirectName)
	{
		return NewRedirectName.ObjectName;
	}

	return NAME_None;
}

/**
 * Returns the hash value for an element of this property.
 */
uint32 FProperty::GetValueTypeHash(const void* Src) const
{
	check(PropertyFlags & CPF_HasGetValueTypeHash); // make sure the type is hashable
	check(Src);
	return GetValueTypeHashInternal(Src);
}

void FProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	check(0); // if you are not memcpyable, then you need to deal with the virtual call
}

uint32 FProperty::GetValueTypeHashInternal(const void* Src) const
{
	check(false); // you need to deal with the virtual call
	return 0;
}

#if WITH_EDITORONLY_DATA
UPropertyWrapper* FProperty::GetUPropertyWrapper()
{
	UStruct* OwnerStruct = GetOwnerStruct();
	UPropertyWrapper* Wrapper = nullptr;
	if (OwnerStruct)
	{
		// Find an existing wrapper object
		for (UPropertyWrapper* ExistingWrapper : OwnerStruct->PropertyWrappers)
		{
			if (ExistingWrapper->GetProperty() == this)
			{
				Wrapper = ExistingWrapper;
				break;
			}
		}
		if (!Wrapper)
		{
			// Try to find the class of a new wrapper object mathich this property's class
			FString WrapperClassName = GetClass()->GetName();
			WrapperClassName += TEXT("Wrapper");
			UClass* WrapperClass = Cast<UClass>(StaticFindObjectFast(UClass::StaticClass(), UPackage::StaticClass()->GetOutermost(), *WrapperClassName));
			if (!WrapperClass)
			{
				// Default to generic wrapper class
				WrapperClass = UPropertyWrapper::StaticClass();
			}
			Wrapper = NewObject<UPropertyWrapper>(OwnerStruct, WrapperClass, *FString::Printf(TEXT("%sWrapper"), *GetName()));
			check(Wrapper);
			Wrapper->SetProperty(this);
			OwnerStruct->PropertyWrappers.Add(Wrapper);
		}
	}
	return Wrapper;
}
#endif //  WITH_EDITORONLY_DATA

void FFloatProperty::ExportText_Internal(FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	Super::ExportText_Internal(ValueStr, PropertyValueOrContainer, PropertyPointerType, DefaultValue, Parent, PortFlags, ExportRootScope);

	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += TEXT("f");
	}
}


FProperty* UStruct::FindPropertyByName(FName InName) const
{
	for (FProperty* Property = PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
	{
		if (Property->GetFName() == InName)
		{
			return Property;
		}
	}

	return NULL;
}
