// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/ListUtilityFunctions.h"
#include "VectorTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ListUtilityFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_ListUtilityFunctions"


int UGeometryScriptLibrary_ListUtilityFunctions::GetIndexListLength(FGeometryScriptIndexList IndexList)
{
	return (IndexList.List.IsValid()) ? IndexList.List->Num() : 0;
}

int UGeometryScriptLibrary_ListUtilityFunctions::GetIndexListLastIndex(FGeometryScriptIndexList IndexList)
{
	return IndexList.List.IsValid() ? IndexList.List->Num()-1 : -1;
}

int UGeometryScriptLibrary_ListUtilityFunctions::GetIndexListItem(FGeometryScriptIndexList IndexList, int Index, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (IndexList.List.IsValid() && Index >= 0 && Index < IndexList.List->Num())
	{
		bIsValidIndex = true;
		return (*IndexList.List)[Index];
	}
	return -1;
}

void UGeometryScriptLibrary_ListUtilityFunctions::SetIndexListItem(FGeometryScriptIndexList& IndexList, int Index, int NewValue, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (IndexList.List.IsValid() && Index >= 0 && Index < IndexList.List->Num())
	{
		bIsValidIndex = true;
		(*IndexList.List)[Index] = NewValue;
	}
}


void UGeometryScriptLibrary_ListUtilityFunctions::ConvertIndexListToArray(FGeometryScriptIndexList IndexList, TArray<int>& IndexArray)
{
	IndexArray.Reset();
	if (IndexList.List.IsValid())
	{
		IndexArray.Append(*IndexList.List);
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertArrayToIndexList(const TArray<int>& IndexArray, FGeometryScriptIndexList& IndexList, EGeometryScriptIndexType IndexType)
{
	IndexList.Reset(IndexType);
	IndexList.List->Append(IndexArray);
}

void UGeometryScriptLibrary_ListUtilityFunctions::DuplicateIndexList(FGeometryScriptIndexList IndexList, FGeometryScriptIndexList& DuplicateList)
{
	DuplicateList.Reset(IndexList.IndexType);
	*DuplicateList.List = *IndexList.List;
}

void UGeometryScriptLibrary_ListUtilityFunctions::ClearIndexList(FGeometryScriptIndexList& IndexList, int ClearValue)
{
	int Num = GetIndexListLength(IndexList);
	IndexList.Reset(IndexList.IndexType);
	IndexList.List->Init(ClearValue, Num);
}




int UGeometryScriptLibrary_ListUtilityFunctions::GetTriangleListLength(FGeometryScriptTriangleList TriangleList)
{
	return (TriangleList.List.IsValid()) ? TriangleList.List->Num() : 0;
}

int UGeometryScriptLibrary_ListUtilityFunctions::GetTriangleListLastTriangle(FGeometryScriptTriangleList TriangleList)
{
	return (TriangleList.List.IsValid()) ? FMath::Max(TriangleList.List->Num()-1,0) : 0;
}

FIntVector UGeometryScriptLibrary_ListUtilityFunctions::GetTriangleListItem(FGeometryScriptTriangleList TriangleList, int Triangle, bool& bIsValidTriangle)
{
	bIsValidTriangle = false;
	if (TriangleList.List.IsValid() && Triangle >= 0 && Triangle < TriangleList.List->Num())
	{
		bIsValidTriangle = true;
		return (*TriangleList.List)[Triangle];
	}
	return FIntVector::NoneValue;
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertTriangleListToArray(FGeometryScriptTriangleList TriangleList, TArray<FIntVector>& TriangleArray)
{
	TriangleArray.Reset();
	if (TriangleList.List.IsValid())
	{
		TriangleArray.Append(*TriangleList.List);
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertArrayToTriangleList(const TArray<FIntVector>& TriangleArray, FGeometryScriptTriangleList& TriangleList)
{
	TriangleList.Reset();
	TriangleList.List->Append(TriangleArray);
}





int UGeometryScriptLibrary_ListUtilityFunctions::GetScalarListLength(FGeometryScriptScalarList ScalarList)
{
	return (ScalarList.List.IsValid()) ? ScalarList.List->Num() : 0;
}

int UGeometryScriptLibrary_ListUtilityFunctions::GetScalarListLastIndex(FGeometryScriptScalarList ScalarList)
{
	return ScalarList.List.IsValid() ? ScalarList.List->Num()-1 : -1;
}

double UGeometryScriptLibrary_ListUtilityFunctions::GetScalarListItem(FGeometryScriptScalarList ScalarList, int Index, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (ScalarList.List.IsValid() && Index >= 0 && Index < ScalarList.List->Num())
	{
		bIsValidIndex = true;
		return (*ScalarList.List)[Index];
	}
	return 0.0;
}

void UGeometryScriptLibrary_ListUtilityFunctions::SetScalarListItem(FGeometryScriptScalarList& ScalarList, int Index, double NewValue, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (ScalarList.List.IsValid() && Index >= 0 && Index < ScalarList.List->Num())
	{
		bIsValidIndex = true;
		(*ScalarList.List)[Index] = NewValue;
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertScalarListToArray(FGeometryScriptScalarList ScalarList, TArray<double>& ScalarArray)
{
	ScalarArray.Reset();
	if (ScalarList.List.IsValid())
	{
		ScalarArray.Append(*ScalarList.List);
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertArrayToScalarList(const TArray<double>& ScalarArray, FGeometryScriptScalarList& ScalarList)
{
	ScalarList.Reset();
	ScalarList.List->Append(ScalarArray);
}

void UGeometryScriptLibrary_ListUtilityFunctions::DuplicateScalarList(FGeometryScriptScalarList ScalarList, FGeometryScriptScalarList& DuplicateList)
{
	DuplicateList.Reset();
	*DuplicateList.List = *ScalarList.List;
}

void UGeometryScriptLibrary_ListUtilityFunctions::ClearScalarList(FGeometryScriptScalarList& ScalarList, double ClearValue)
{
	int Num = GetScalarListLength(ScalarList);
	ScalarList.Reset();
	ScalarList.List->Init(ClearValue, Num);
}





int UGeometryScriptLibrary_ListUtilityFunctions::GetVectorListLength(FGeometryScriptVectorList VectorList)
{
	return (VectorList.List.IsValid()) ? VectorList.List->Num() : 0;
}

int UGeometryScriptLibrary_ListUtilityFunctions::GetVectorListLastIndex(FGeometryScriptVectorList VectorList)
{
	return VectorList.List.IsValid() ? VectorList.List->Num()-1 : -1;
}


FVector UGeometryScriptLibrary_ListUtilityFunctions::GetVectorListItem(FGeometryScriptVectorList VectorList, int Index, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (VectorList.List.IsValid() && Index >= 0 && Index < VectorList.List->Num())
	{
		bIsValidIndex = true;
		return (*VectorList.List)[Index];
	}
	return FVector::Zero();
}

void UGeometryScriptLibrary_ListUtilityFunctions::SetVectorListItem(FGeometryScriptVectorList& VectorList, int Index, FVector NewValue, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (VectorList.List.IsValid() && Index >= 0 && Index < VectorList.List->Num())
	{
		bIsValidIndex = true;
		(*VectorList.List)[Index] = NewValue;
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertVectorListToArray(FGeometryScriptVectorList VectorList, TArray<FVector>& VectorArray)
{
	VectorArray.Reset();
	if (VectorList.List.IsValid())
	{
		VectorArray.Append(*VectorList.List);
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertArrayToVectorList(const TArray<FVector>& VectorArray, FGeometryScriptVectorList& VectorList)
{
	VectorList.Reset();
	VectorList.List->Append(VectorArray);
}

void UGeometryScriptLibrary_ListUtilityFunctions::DuplicateVectorList(FGeometryScriptVectorList VectorList, FGeometryScriptVectorList& DuplicateList)
{
	DuplicateList.Reset();
	*DuplicateList.List = *VectorList.List;
}

void UGeometryScriptLibrary_ListUtilityFunctions::ClearVectorList(FGeometryScriptVectorList& VectorList, FVector ClearValue)
{
	int Num = GetVectorListLength(VectorList);
	VectorList.Reset();
	VectorList.List->Init(ClearValue, Num);
}



int UGeometryScriptLibrary_ListUtilityFunctions::GetUVListLength(FGeometryScriptUVList UVList)
{
	return (UVList.List.IsValid()) ? UVList.List->Num() : 0;
}

int UGeometryScriptLibrary_ListUtilityFunctions::GetUVListLastIndex(FGeometryScriptUVList UVList)
{
	return UVList.List.IsValid() ? UVList.List->Num()-1 : -1;
}

FVector2D UGeometryScriptLibrary_ListUtilityFunctions::GetUVListItem(FGeometryScriptUVList UVList, int Index, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (UVList.List.IsValid() && Index >= 0 && Index < UVList.List->Num())
	{
		bIsValidIndex = true;
		return (*UVList.List)[Index];
	}
	return FVector2D::ZeroVector;
}

void UGeometryScriptLibrary_ListUtilityFunctions::SetUVListItem(FGeometryScriptUVList& UVList, int Index, FVector2D NewUV, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (UVList.List.IsValid() && Index >= 0 && Index < UVList.List->Num())
	{
		bIsValidIndex = true;
		(*UVList.List)[Index] = NewUV;
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertUVListToArray(FGeometryScriptUVList UVList, TArray<FVector2D>& UVArray)
{
	UVArray.Reset();
	if (UVList.List.IsValid())
	{
		UVArray.Append(*UVList.List);
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertArrayToUVList(const TArray<FVector2D>& UVArray, FGeometryScriptUVList& UVList)
{
	UVList.Reset();
	UVList.List->Append(UVArray);
}

void UGeometryScriptLibrary_ListUtilityFunctions::DuplicateUVList(FGeometryScriptUVList UVList, FGeometryScriptUVList& DuplicateList)
{
	DuplicateList.Reset();
	*DuplicateList.List = *UVList.List;
}

void UGeometryScriptLibrary_ListUtilityFunctions::ClearUVList(FGeometryScriptUVList& UVList, FVector2D ClearUV)
{
	int Num = GetUVListLength(UVList);
	UVList.Reset();
	UVList.List->Init(ClearUV, Num);
}




int UGeometryScriptLibrary_ListUtilityFunctions::GetColorListLength(FGeometryScriptColorList ColorList)
{
	return (ColorList.List.IsValid()) ? ColorList.List->Num() : 0;
}

int UGeometryScriptLibrary_ListUtilityFunctions::GetColorListLastIndex(FGeometryScriptColorList ColorList)
{
	return ColorList.List.IsValid() ? ColorList.List->Num()-1 : -1;
}

FLinearColor UGeometryScriptLibrary_ListUtilityFunctions::GetColorListItem(FGeometryScriptColorList ColorList, int Index, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (ColorList.List.IsValid() && Index >= 0 && Index < ColorList.List->Num())
	{
		bIsValidIndex = true;
		return (*ColorList.List)[Index];
	}
	return FLinearColor::White;
}

void UGeometryScriptLibrary_ListUtilityFunctions::SetColorListItem(FGeometryScriptColorList& ColorList, int Index, FLinearColor NewColor, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (ColorList.List.IsValid() && Index >= 0 && Index < ColorList.List->Num())
	{
		bIsValidIndex = true;
		(*ColorList.List)[Index] = NewColor;
	}
}


void UGeometryScriptLibrary_ListUtilityFunctions::ConvertColorListToArray(FGeometryScriptColorList ColorList, TArray<FLinearColor>& ColorArray)
{
	ColorArray.Reset();
	if (ColorList.List.IsValid())
	{
		ColorArray.Append(*ColorList.List);
	}
}

void UGeometryScriptLibrary_ListUtilityFunctions::ConvertArrayToColorList(const TArray<FLinearColor>& ColorArray, FGeometryScriptColorList& ColorList)
{
	ColorList.Reset();
	ColorList.List->Append(ColorArray);
}


void UGeometryScriptLibrary_ListUtilityFunctions::DuplicateColorList(FGeometryScriptColorList ColorList, FGeometryScriptColorList& DuplicateList)
{
	DuplicateList.Reset();
	*DuplicateList.List = *ColorList.List;
}

void UGeometryScriptLibrary_ListUtilityFunctions::ClearColorList(FGeometryScriptColorList& ColorList, FLinearColor ClearColor)
{
	int Num = GetColorListLength(ColorList);
	ColorList.Reset();
	ColorList.List->Init(ClearColor, Num);
}


void UGeometryScriptLibrary_ListUtilityFunctions::ExtractColorListChannel(FGeometryScriptColorList ColorList, FGeometryScriptScalarList& ScalarList, int32 ChannelIndex)
{
	if (ChannelIndex < 0 || ChannelIndex > 3)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExtractColorListChannel: ChannelIndex is not in valid range 0-3"));
	}
	ChannelIndex = FMath::Clamp(ChannelIndex, 0, 3);

	ScalarList.Reset();
	if (ColorList.List.IsValid())
	{
		TArray<FLinearColor>& Colors = *ColorList.List;
		TArray<double>& Scalars = *ScalarList.List;
		int32 N = Colors.Num();
		Scalars.SetNumUninitialized(N);
		for (int32 k = 0; k < N; ++k)
		{
			FVector4d Vector4 = (FVector4d)(Colors[k]);
			Scalars[k] = Vector4[ChannelIndex];
		}
	}
}


void UGeometryScriptLibrary_ListUtilityFunctions::ExtractColorListChannels(
	FGeometryScriptColorList ColorList, FGeometryScriptVectorList& VectorList, 
	int32 XChannelIndex, int32 YChannelIndex, int32 ZChannelIndex)
{
	if (XChannelIndex < 0 || XChannelIndex > 3)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExtractColorListChannels: XChannelIndex is not in valid range 0-3"));
	}
	if (YChannelIndex < 0 || YChannelIndex > 3)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExtractColorListChannels: YChannelIndex is not in valid range 0-3"));
	}
	if (ZChannelIndex < 0 || ZChannelIndex > 3)
	{
		UE_LOG(LogGeometry, Warning, TEXT("ExtractColorListChannels: ZChannelIndex is not in valid range 0-3"));
	}
	XChannelIndex = FMath::Clamp(XChannelIndex, 0, 3);
	YChannelIndex = FMath::Clamp(YChannelIndex, 0, 3);
	ZChannelIndex = FMath::Clamp(ZChannelIndex, 0, 3);

	VectorList.Reset();
	if (ColorList.List.IsValid())
	{
		TArray<FLinearColor>& Colors = *ColorList.List;
		TArray<FVector>& Vectors = *VectorList.List;
		int32 N = Colors.Num();
		Vectors.SetNumUninitialized(N);
		for (int32 k = 0; k < N; ++k)
		{
			FVector4d Vector4 = (FVector4d)(Colors[k]);
			Vectors[k] = FVector(Vector4[XChannelIndex], Vector4[YChannelIndex], Vector4[ZChannelIndex]);
		}
	}
}



#undef LOCTEXT_NAMESPACE
