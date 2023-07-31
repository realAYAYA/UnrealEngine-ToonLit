// Copyright Epic Games, Inc. All Rights Reserved.


#include "ContourList.h"
#include "Part.h"
#include "Data.h"

#include "Math/UnrealMathUtility.h"


FContourList::FContourList()
{
}

FContourList::FContourList(const FContourList& Other)
{
	for (const FContour& OtherContour : Other)
		{
		FContour& Contour = Add();
		Contour.CopyFrom(OtherContour);
	}
}

void FContourList::Initialize(const TSharedRef<FData>& Data)
{
	for (FContour& Contour : *this)
	{
		Contour.SetNeighbours();

		for (FPartPtr Edge : Contour)
		{
			Edge->ComputeTangentX();
		}

		for (const FPartPtr& Point : Contour)
		{
			Point->ComputeSmooth();
		}

		for (int32 Index = 0; Index < Contour.Num(); Index++)
		{
			const FPartPtr Point = Contour[Index];

			if (!Point->bSmooth && Point->TangentsDotProduct() > 0.f)
			{
				const FPartPtr Curr = Point;
				const FPartPtr Prev = Point->Prev;

				const float TangentsCrossProduct = FVector2D::CrossProduct(-Prev->TangentX, Curr->TangentX);
				const float MinTangentsCrossProduct = 0.9f;

				if (FMath::Abs(TangentsCrossProduct) < MinTangentsCrossProduct)
				{
					const float OffsetDefault = 0.01f;
					const float Offset = FMath::Min3(Prev->Length() / 2.f, Curr->Length() / 2.f, OffsetDefault);

					const FPartPtr Added = MakeShared<FPart>();
					Contour.Insert(Added, Index);

					Prev->Next = Added;
					Added->Prev = Prev;
					Added->Next = Curr;
					Curr->Prev = Added;

					const FVector2D CornerPosition = Curr->Position;

					Curr->Position = CornerPosition + Curr->TangentX * Offset;
					Added->Position = CornerPosition - Prev->TangentX * Offset;

					Data->AddVertices(1);
					const int32 VertexID = Data->AddVertex(Added->Position, { 1.f, 0.f }, { -1.f, 0.f, 0.f });

					Added->PathPrev.Add(VertexID);
					Added->PathNext.Add(VertexID);

					Added->ComputeTangentX();

					Added->ComputeSmooth();
					Curr->ComputeSmooth();
				}
			}
		}

		for (const FPartPtr& Point : Contour)
		{
			Point->ComputeNormal();
			Point->ResetInitialPosition();
		}
	}
}

FContour& FContourList::Add()
{
	AddTail(FContour());
	return GetTail()->GetValue();
}

void FContourList::Remove(const FContour& Contour)
{
	// Search with comparing pointers
	for (TDoubleLinkedList<FContour>::TDoubleLinkedListNode* Node = GetHead(); Node; Node = Node->GetNextNode())
	{
		if (&Node->GetValue() == &Contour)
		{
			RemoveNode(Node);
			break;
		}
	}
}

void FContourList::Reset()
{
	for (FContour& Contour : *this)
	{
		for (const FPartPtr& Part : Contour)
		{
			Part->ResetDoneExpand();
		}
	}
}
