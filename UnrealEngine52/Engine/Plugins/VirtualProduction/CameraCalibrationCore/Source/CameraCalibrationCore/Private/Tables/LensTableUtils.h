// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Set of templated functions operating on common APIed lens table data structure
 */
namespace LensDataTableUtils
{
	/** Removes a focus point from a container */
	template<typename FocusPointType>
	void RemoveFocusPoint(TArray<FocusPointType>& Container, float InFocus)
	{
		const int32 FoundIndex = Container.IndexOfByPredicate([InFocus](const FocusPointType& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
    	if(FoundIndex != INDEX_NONE)
    	{
    		Container.RemoveAt(FoundIndex);
    	}
	}

	/** Gets all point info for specific data table */
	template<typename FPointInfoType, typename FDataTableType>
	TArray<FPointInfoType> GetAllPointsInfo(const FDataTableType& InTable)
	{
		TArray<FPointInfoType> PointsInfoType;

		for (const typename FDataTableType::FocusPointType& Point : InTable.FocusPoints)
		{
			// Get Focus Value
			const float FocusValue = Point.Focus;

			// Loop through all zoom points
			const int32 ZoomPointsNum = Point.GetNumPoints();
			for (int32 ZoomPointIndex = 0; ZoomPointIndex < ZoomPointsNum; ++ZoomPointIndex)
			{
				// Get Zoom Value
				const float ZoomValue = Point.GetZoom(ZoomPointIndex);

				// Zoom point should be valid
				typename FPointInfoType::TypeInfo PointInfo;
				ensure(InTable.GetPoint(FocusValue, ZoomValue, PointInfo));

				// Add Point into Array
				PointsInfoType.Add({FocusValue, ZoomValue, MoveTemp(PointInfo)});
			}
		}

		return PointsInfoType;
	}

	/** Removes a zoom point for a given focus value in a container */
	template<typename FocusPointType>
	void RemoveZoomPoint(TArray<FocusPointType>& Container, float InFocus, float InZoom)
	{
		bool bIsEmpty = false;
		const int32 FoundIndex = Container.IndexOfByPredicate([InFocus](const FocusPointType& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
		if(FoundIndex != INDEX_NONE)
		{
			Container[FoundIndex].RemovePoint(InZoom);
			bIsEmpty = Container[FoundIndex].IsEmpty();
		}

		if(bIsEmpty)
		{
			Container.RemoveAt(FoundIndex);
		}
	}

	/** Adds a point at a specified focus and zoom input values */
	template<typename FocusPointType, typename DataType>
	bool AddPoint(TArray<FocusPointType>& InContainer, float InFocus, float InZoom, const DataType& InData, float InputTolerance, bool bIsCalibrationPoint)
	{
		int32 PointIndex = 0;
		for (; PointIndex < InContainer.Num(); ++PointIndex)
		{
			FocusPointType& FocusPoint = InContainer[PointIndex];
			if (FMath::IsNearlyEqual(FocusPoint.Focus, InFocus, InputTolerance))
			{
				return FocusPoint.AddPoint(InZoom, InData, InputTolerance, bIsCalibrationPoint);
			}
			else if (InFocus < FocusPoint.Focus)
			{
				break;
			}
		}

		FocusPointType NewFocusPoint;
		NewFocusPoint.Focus = InFocus;
		const bool bSuccess = NewFocusPoint.AddPoint(InZoom, InData, InputTolerance, bIsCalibrationPoint);
		if(bSuccess)
		{
			InContainer.Insert(MoveTemp(NewFocusPoint), PointIndex);
		}

		return bSuccess;
	}
	
	template<typename TableType, typename DataType>
	bool SetPoint(TableType& InTable, float InFocus, float InZoom, const DataType& InData, float InputTolerance = KINDA_SMALL_NUMBER)
	{
		for (int32 PointIndex = 0; PointIndex < InTable.FocusPoints.Num() && InTable.FocusPoints[PointIndex].Focus <= InFocus; ++PointIndex)
		{
			typename TableType::FocusPointType& FocusPoint = InTable.FocusPoints[PointIndex];
			if (FMath::IsNearlyEqual(FocusPoint.Focus, InFocus, InputTolerance))
			{
				return FocusPoint.SetPoint(InZoom, InData);
			}
		}

		return false;
	}

	/** Clears content of a table */
	template<typename Type>
	void EmptyTable(Type& InTable)
	{
		InTable.FocusPoints.Empty(0);
	}

		struct FPointNeighbors
	{
		int32 PreviousIndex = INDEX_NONE;
		int32 NextIndex = INDEX_NONE;
	};

	/** Finds indices of neighbor focus points for a given focus value */
	template<typename Type>
	FPointNeighbors FindFocusPoints(float InFocus, TConstArrayView<Type> Container)
	{
		FPointNeighbors Neighbors;
		if (Container.Num() <= 0)
		{
			return Neighbors;
		}

		for (int32 Index = 0; Index < Container.Num(); ++Index)
		{
			const Type& Point = Container[Index];
			if (Point.Focus > InFocus)
			{
				Neighbors.NextIndex = Index;
				Neighbors.PreviousIndex = FMath::Max(Index - 1, 0);
				break;
			}
			else if (FMath::IsNearlyEqual(Point.Focus, InFocus))
			{
				//We found a point exactly matching the desired one
				Neighbors.NextIndex = Index;
				Neighbors.PreviousIndex = Index;
				break;
			}
		}

		//We haven't found a point, default to last one
		if (Neighbors.PreviousIndex == INDEX_NONE && Neighbors.NextIndex == INDEX_NONE)
		{
			Neighbors.NextIndex = Container.Num() - 1;
			Neighbors.PreviousIndex = Container.Num() - 1;
		}

		return Neighbors;
	}

	/** Finds indices of neighbor zoom points for a given zoom value */
	template<typename Type>
	FPointNeighbors FindZoomPoints(float InZoom, const TArray<Type>& Container)
	{
		FPointNeighbors Neighbors;
		if (Container.Num() <= 0)
		{
			return Neighbors;
		}

		for (int32 Index = 0; Index < Container.Num(); ++Index)
		{
			const Type& Point = Container[Index];
			if (Point.Zoom > InZoom)
			{
				Neighbors.NextIndex = Index;
				Neighbors.PreviousIndex = FMath::Max(Index - 1, 0);
				break;
			}
			else if (FMath::IsNearlyEqual(Point.Zoom, InZoom))
			{
				//We found a point exactly matching the desired one
				Neighbors.NextIndex = Index;
				Neighbors.PreviousIndex = Index;
				break;
			}
		}

		//We haven't found a point, default to last one
		if (Neighbors.PreviousIndex == INDEX_NONE && Neighbors.NextIndex == INDEX_NONE)
		{
			Neighbors.NextIndex = Container.Num() - 1;
			Neighbors.PreviousIndex = Container.Num() - 1;
		}

		return Neighbors;
	}

	/** Finds a points that matches input Focus and Zoom and returns its value.
	 * Returns false if point isn't found
	 */
	template<typename FocusPointType, typename DataType>
	bool GetPointValue(float InFocus, float InZoom, TConstArrayView<FocusPointType> Container, DataType& OutData)
	{
		const FPointNeighbors FocusNeighbors = FindFocusPoints(InFocus, Container);

		if (FocusNeighbors.PreviousIndex != FocusNeighbors.NextIndex)
		{
			return false;
		}

		if (FocusNeighbors.PreviousIndex == INDEX_NONE)
		{
			return false;
		}

		const FPointNeighbors ZoomNeighbors = FindZoomPoints(InZoom, Container[FocusNeighbors.PreviousIndex].ZoomPoints);

		if (ZoomNeighbors.PreviousIndex != ZoomNeighbors.NextIndex)
		{
			return false;
		}

		if (ZoomNeighbors.PreviousIndex == INDEX_NONE)
		{
			return false;
		}

		return Container[FocusNeighbors.PreviousIndex].GetValue(ZoomNeighbors.PreviousIndex, OutData);
	}

	/** Get total number of Zoom points for all Focus points of this data table */
	template<typename FocusPointType>
	int32 GetTotalPointNum(const TArray<FocusPointType>& Container)
	{
		int32 PointNum = 0;
	
		for (const FocusPointType& Point : Container)
		{
			PointNum += Point.GetNumPoints();
		}

		return PointNum;
	}
}
