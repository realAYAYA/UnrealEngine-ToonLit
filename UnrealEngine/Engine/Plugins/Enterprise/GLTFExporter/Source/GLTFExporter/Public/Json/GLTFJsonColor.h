// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/GLTFColor.h"
#include "Json/GLTFJsonArray.h"
#include "Json/GLTFJsonWriter.h"

template <typename BaseType>
struct TGLTFJsonColor : BaseType, IGLTFJsonArray
{
	TGLTFJsonColor(const BaseType& Other)
		: BaseType(Other)
	{
	}

	TGLTFJsonColor& operator=(const BaseType& Other)
	{
		*static_cast<BaseType*>(this) = Other;
		return *this;
	}

	bool operator==(const BaseType& Other) const
	{
		for (SIZE_T i = 0; i < GetNum(BaseType::Components); ++i)
		{
			if (BaseType::Components[i] != Other.Components[i])
			{
				return false;
			}
		}

		return true;
	}

	bool operator!=(const BaseType& Other) const
	{
		return !(*this == Other);
	}

	virtual void WriteArray(IGLTFJsonWriter& Writer) const override
	{
		for (SIZE_T i = 0; i < GetNum(BaseType::Components); ++i)
		{
			Writer.Write(BaseType::Components[i]);
		}
	}

	bool IsNearlyEqual(const BaseType& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		for (SIZE_T i = 0; i < GetNum(BaseType::Components); ++i)
		{
			if (!FMath::IsNearlyEqual(BaseType::Components[i], Other.Components[i], Tolerance))
			{
				return false;
			}
		}

		return true;
	}
};

struct GLTFEXPORTER_API FGLTFJsonColor3 : TGLTFJsonColor<FGLTFColor3>
{
	static const FGLTFJsonColor3 Black;
	static const FGLTFJsonColor3 White;

	using TGLTFJsonColor::TGLTFJsonColor;
	using TGLTFJsonColor::operator=;
};

struct GLTFEXPORTER_API FGLTFJsonColor4 : TGLTFJsonColor<FGLTFColor4>
{
	static const FGLTFJsonColor4 Black;
	static const FGLTFJsonColor4 White;

	using TGLTFJsonColor::TGLTFJsonColor;
	using TGLTFJsonColor::operator=;
};
