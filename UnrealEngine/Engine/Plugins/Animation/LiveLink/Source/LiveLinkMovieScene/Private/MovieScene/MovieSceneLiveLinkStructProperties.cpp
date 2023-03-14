// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneLiveLinkStructProperties.h"

#include "MovieScene/MovieSceneLiveLinkEnumHandler.h"
#include "MovieScene/MovieSceneLiveLinkPropertyHandler.h"
#include "MovieScene/MovieSceneLiveLinkTransformHandler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneLiveLinkStructProperties)


namespace LiveLinkPropertiesUtils
{
	TSharedPtr<IMovieSceneLiveLinkPropertyHandler> CreateHandlerFromProperty(FProperty* InProperty, const FLiveLinkStructPropertyBindings& InBinding, FLiveLinkPropertyData* InPropertyData)
	{
		if (InProperty->IsA(FFloatProperty::StaticClass()))
		{
			return MakeShared<FMovieSceneLiveLinkPropertyHandler<float>>(InBinding, InPropertyData);
		}
		else if (InProperty->IsA(FIntProperty::StaticClass()))
		{
			return MakeShared<FMovieSceneLiveLinkPropertyHandler<int32>>(InBinding, InPropertyData);
		}
		else if (InProperty->IsA(FBoolProperty::StaticClass()))
		{
			return MakeShared<FMovieSceneLiveLinkPropertyHandler<bool>>(InBinding, InPropertyData);
		}
		else if (InProperty->IsA(FStrProperty::StaticClass()))
		{
			return MakeShared<FMovieSceneLiveLinkPropertyHandler<FString>>(InBinding, InPropertyData);
		}
		else if (InProperty->IsA(FByteProperty::StaticClass()))
		{
			return MakeShared<FMovieSceneLiveLinkPropertyHandler<uint8>>(InBinding, InPropertyData);
		}
		else if (InProperty->IsA(FEnumProperty::StaticClass()))
		{
			return MakeShared<FMovieSceneLiveLinkEnumHandler>(InBinding, InPropertyData);
		}
		else if (InProperty->IsA(FStructProperty::StaticClass()))
		{
			if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
			{
				if (StructProperty->Struct->GetFName() == NAME_Transform)
				{
					return MakeShared<FMovieSceneLiveLinkTransformHandler>(InBinding, InPropertyData);
				}
				else if (StructProperty->Struct->GetFName() == NAME_Vector)
				{
					return MakeShared<FMovieSceneLiveLinkPropertyHandler<FVector>>(InBinding, InPropertyData);
				}
				else if (StructProperty->Struct->GetFName() == NAME_Color)
				{
					return MakeShared<FMovieSceneLiveLinkPropertyHandler<FColor>>(InBinding, InPropertyData);
				}
			}
		}
		
		return TSharedPtr<IMovieSceneLiveLinkPropertyHandler>();
	}

	TSharedPtr<IMovieSceneLiveLinkPropertyHandler> CreatePropertyHandler(const UScriptStruct& InStruct, FLiveLinkPropertyData* InPropertyData)
	{
		FLiveLinkStructPropertyBindings PropertyBinding(InPropertyData->PropertyName, InPropertyData->PropertyName.ToString());
		FProperty* PropertyPtr = PropertyBinding.GetProperty(InStruct);
		if (PropertyPtr == nullptr)
		{
			return TSharedPtr<IMovieSceneLiveLinkPropertyHandler>();
		}

		if (PropertyPtr->IsA(FArrayProperty::StaticClass()))
		{
			return CreateHandlerFromProperty(CastFieldChecked<FArrayProperty>(PropertyPtr)->Inner, PropertyBinding, InPropertyData);
		}
		else
		{
			return CreateHandlerFromProperty(PropertyPtr, PropertyBinding, InPropertyData);
		}
	}

}

