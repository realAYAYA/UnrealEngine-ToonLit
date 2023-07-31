// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/GLTFMatrix.h"
#include "Json/GLTFJsonArray.h"
#include "Json/GLTFJsonWriter.h"

template <typename BaseType>
struct TGLTFJsonMatrix : BaseType, IGLTFJsonArray
{
	TGLTFJsonMatrix(const BaseType& Other)
		: BaseType(Other)
	{
	}

	TGLTFJsonMatrix& operator=(const BaseType& Other)
	{
		*static_cast<BaseType*>(this) = Other;
		return *this;
	}

	bool operator==(const BaseType& Other) const
	{
		for (SIZE_T i = 0; i < GetNum(BaseType::Elements); ++i)
		{
			if (BaseType::Elements[i] != Other.Elements[i])
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
		for (SIZE_T i = 0; i < GetNum(BaseType::Elements); ++i)
		{
			Writer.Write(BaseType::Elements[i]);
		}
	}

	bool IsNearlyEqual(const BaseType& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		for (SIZE_T i = 0; i < GetNum(BaseType::Elements); ++i)
		{
			if (!FMath::IsNearlyEqual(BaseType::Elements[i], Other.Elements[i], Tolerance))
			{
				return false;
			}
		}

		return true;
	}
};

struct GLTFEXPORTER_API FGLTFJsonMatrix2 : TGLTFJsonMatrix<FGLTFMatrix2>
{
	static const FGLTFJsonMatrix2 Identity;

	using TGLTFJsonMatrix::TGLTFJsonMatrix;
	using TGLTFJsonMatrix::operator=;
};

struct GLTFEXPORTER_API FGLTFJsonMatrix3 : TGLTFJsonMatrix<FGLTFMatrix3>
{
	static const FGLTFJsonMatrix3 Identity;

	using TGLTFJsonMatrix::TGLTFJsonMatrix;
	using TGLTFJsonMatrix::operator=;
};

struct GLTFEXPORTER_API FGLTFJsonMatrix4 : TGLTFJsonMatrix<FGLTFMatrix4>
{
	static const FGLTFJsonMatrix4 Identity;

	using TGLTFJsonMatrix::TGLTFJsonMatrix;
	using TGLTFJsonMatrix::operator=;
};
