// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"

#if ENABLE_VISUAL_LOG
#include "Engine/World.h"
#include "UObject/Interface.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "VisualLogger/VisualLoggerCustomVersion.h"
#include "VisualLogger/VisualLogger.h"
#endif

namespace
{
	const FName NAME_UnnamedCategory = TEXT("UnnamedCategry");
}

UVisualLoggerDebugSnapshotInterface::UVisualLoggerDebugSnapshotInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if ENABLE_VISUAL_LOG

#define DEPRECATED_VISUAL_LOGGER_MAGIC_NUMBER 0xFAFAAFAF
#define VISUAL_LOGGER_MAGIC_NUMBER_OLD_CUSTOM_VERSION 0xAFAFFAFA
#define VISUAL_LOGGER_MAGIC_NUMBER_CUSTOM_VERSION_CONTAINER 0xBFBFBBFB
#define VISUAL_LOGGER_MAGIC_NUMBER_LATEST VISUAL_LOGGER_MAGIC_NUMBER_CUSTOM_VERSION_CONTAINER

//----------------------------------------------------------------------//
// FVisualLogShapeElement 
//----------------------------------------------------------------------//
FVisualLogShapeElement::FVisualLogShapeElement(EVisualLoggerShapeElement InType)
	: Category(NAME_UnnamedCategory)
	, Verbosity(ELogVerbosity::All)
	, TransformationMatrix(FMatrix::Identity)
	, Type(InType)
	, Color(0xff)
	, Thicknes(0)
{

}

FVisualLogStatusCategory::FVisualLogStatusCategory()
	: Category(NAME_UnnamedCategory.ToString())
{
}

bool FVisualLogStatusCategory::GetDesc(int32 Index, FString& Key, FString& Value) const
{
	if (Data.IsValidIndex(Index))
	{
		int32 SplitIdx = INDEX_NONE;
		if (Data[Index].FindChar(TEXT('|'), SplitIdx))
		{
			Key = Data[Index].Left(SplitIdx);
			Value = Data[Index].Mid(SplitIdx + 1);
			return true;
		}
	}

	return false;
}

FVisualLogEntry::FVisualLogEntry(const AActor* InActor, TArray<TWeakObjectPtr<UObject> >* Children)
{
	if (!IsValid(InActor))
	{
		Reset();
		return;
	}

	WorldTimeStamp = InActor->GetWorld()->TimeSeconds;
	Location = InActor->GetActorLocation();
	bIsLocationValid = true;

#if ENABLE_VISUAL_LOG
	TimeStamp = FVisualLogger::Get().GetTimeStampForObject(InActor);
#else
	TimeStamp = WorldTimeStamp;
#endif

	const IVisualLoggerDebugSnapshotInterface* DebugSnapshotInterface = Cast<const IVisualLoggerDebugSnapshotInterface>(InActor);
	if (DebugSnapshotInterface)
	{
		DebugSnapshotInterface->GrabDebugSnapshot(this);
	}
	if (Children != nullptr)
	{
		TWeakObjectPtr<UObject>* WeakActorPtr = Children->GetData();
		for (int32 Index = 0; Index < Children->Num(); ++Index, ++WeakActorPtr)
		{
			if (WeakActorPtr->IsValid())
			{
				const IVisualLoggerDebugSnapshotInterface* ChildActor = Cast<const IVisualLoggerDebugSnapshotInterface>(WeakActorPtr->Get());
				if (ChildActor)
				{
					ChildActor->GrabDebugSnapshot(this);
				}
			}
		}
	}
}

FVisualLogEntry::FVisualLogEntry(double InTimeStamp, FVector InLocation, const UObject* Object, TArray<TWeakObjectPtr<UObject> >* Children)
{
	TimeStamp = InTimeStamp;
	Location = InLocation;
	bIsLocationValid = true;

	const IVisualLoggerDebugSnapshotInterface* DebugSnapshotInterface = Cast<const IVisualLoggerDebugSnapshotInterface>(Object);
	if (DebugSnapshotInterface)
	{
		DebugSnapshotInterface->GrabDebugSnapshot(this);
	}
	if (Children != nullptr)
	{
		TWeakObjectPtr<UObject>* WeakActorPtr = Children->GetData();
		for (int32 Index = 0; Index < Children->Num(); ++Index, ++WeakActorPtr)
		{
			if (WeakActorPtr->IsValid())
			{
				const IVisualLoggerDebugSnapshotInterface* ChildActor = Cast<const IVisualLoggerDebugSnapshotInterface>(WeakActorPtr->Get());
				if (ChildActor)
				{
					ChildActor->GrabDebugSnapshot(this);
				}
			}
		}
	}
}

void FVisualLogEntry::InitializeEntry(const double InTimeStamp)
{
	Reset();
	TimeStamp = InTimeStamp;
	WorldTimeStamp = InTimeStamp;
	bIsInitialized = true;
}

void FVisualLogEntry::Reset()
{
	TimeStamp = -1.0;
	WorldTimeStamp = -1.0;
	Location = FVector::ZeroVector;
	bIsLocationValid = false;
	Events.Reset();
	LogLines.Reset();
	Status.Reset();
	ElementsToDraw.Reset();
	HistogramSamples.Reset();
	DataBlocks.Reset();
	bIsInitialized = false;
}

void FVisualLogEntry::SetPassedObjectAllowList(const bool bPassed)
{
	bPassedObjectAllowList = bPassed;
	UpdateAllowedToLog();
}

void FVisualLogEntry::UpdateAllowedToLog()
{
	bIsAllowedToLog = bPassedClassAllowList || bPassedObjectAllowList;
}

int32 FVisualLogEntry::AddEvent(const FVisualLogEventBase& Event)
{
	return Events.Add(Event);
}

void FVisualLogEntry::MoveTo(FVisualLogEntry& Other)
{
	ensureMsgf(bIsInitialized && Other.bIsInitialized, TEXT("Both entries need to be initialized to move to the other"));
	ensureMsgf(TimeStamp == Other.TimeStamp, TEXT("Can only move similar entries"));
	ensureMsgf(bPassedClassAllowList == Other.bPassedClassAllowList, TEXT("Can only move similar entries"));
	ensureMsgf(bPassedObjectAllowList == Other.bPassedObjectAllowList, TEXT("Can only move similar entries"));
	ensureMsgf(bIsAllowedToLog == Other.bIsAllowedToLog, TEXT("Can only move similar entries"));
	Other.Events.Append(Events);
	Other.LogLines.Append(LogLines);
	Other.Status.Append(Status);
	Other.ElementsToDraw.Append(ElementsToDraw);
	Other.HistogramSamples.Append(HistogramSamples);
	Other.DataBlocks.Append(DataBlocks);
	Reset();
}

void FVisualLogEntry::AddText(const FString& TextLine, const FName& CategoryName, ELogVerbosity::Type Verbosity)
{
	LogLines.Add(FVisualLogLine(CategoryName, Verbosity, TextLine));
}

void FVisualLogEntry::AddElement(const FVisualLogShapeElement& Element)
{
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	AddPath(Points, CategoryName, Verbosity, Color, Description, Thickness);
}

void FVisualLogEntry::AddPath(const TArray<FVector>&Points, const FName & CategoryName, ELogVerbosity::Type Verbosity, const FColor & Color, const FString & Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points = Points;
	Element.Type = EVisualLoggerShapeElement::Path;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const FVector& Point, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	AddLocation(Point, CategoryName, Verbosity, Color, Description, Thickness);
}
void FVisualLogEntry::AddLocation(const FVector& Point, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Add(Point);
	Element.Type = EVisualLoggerShapeElement::SinglePoint;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddSphere(const FVector& Center, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, bool bInUseWires)
{
	FVisualLogShapeElement Element(Description, Color, Radius, CategoryName);
	Element.Points.Add(Center);
	Element.Type = bInUseWires ? EVisualLoggerShapeElement::WireSphere : EVisualLoggerShapeElement::Sphere;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	AddSegment(Start, End, CategoryName, Verbosity, Color, Description, Thickness);
}
void FVisualLogEntry::AddSegment(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(2);
	Element.Points.Add(Start);
	Element.Points.Add(End);
	Element.Type = EVisualLoggerShapeElement::Segment;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddArrow(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description)
{
	FVisualLogShapeElement Element(EVisualLoggerShapeElement::Arrow);
	Element.Category = CategoryName;
	Element.SetColor(Color);
	Element.Description = Description;
	Element.Points.Reserve(2);
	Element.Points.Add(Start);
	Element.Points.Add(End);
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddCircle(const FVector& Center, const FVector& UpAxis, const float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, const uint16 Thickness)
{
	FVisualLogShapeElement Element(EVisualLoggerShapeElement::Circle);
	Element.Category = CategoryName;
	Element.SetColor(Color);
	Element.Thicknes = Thickness;
	Element.Description = Description;
	Element.Points.Reserve(3);
	Element.Points.Add(Center);
	Element.Points.Add(UpAxis);
	Element.Points.Add(FVector(Radius, 0., 0.));
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const FBox& Box, const FMatrix& Matrix, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness, bool bInUseWires)
{
	AddBox(Box, Matrix, CategoryName, Verbosity, Color, Description, Thickness, bInUseWires);
}
void FVisualLogEntry::AddBox(const FBox& Box, const FMatrix& Matrix, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness, bool bInUseWires)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(2);
	Element.Points.Add(Box.Min);
	Element.Points.Add(Box.Max);
	Element.Type = bInUseWires ? EVisualLoggerShapeElement::WireBox : EVisualLoggerShapeElement::Box;
	Element.Verbosity = Verbosity;
	Element.TransformationMatrix = Matrix;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddBoxes(const TArray<FBox>& Boxes, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color)
{
	FVisualLogShapeElement& Element = ElementsToDraw[ElementsToDraw.Add(FVisualLogShapeElement(EVisualLoggerShapeElement::Box))];
	Element.Category = CategoryName;
	Element.Verbosity = Verbosity;
	Element.Points.Reserve(2 * Boxes.Num());
	for (const FBox& Box : Boxes)
	{
		Element.Points.Add(Box.Min);
		Element.Points.Add(Box.Max);
	}
	Element.Verbosity = Verbosity;
}

// Deprecated : 
void FVisualLogEntry::AddElement(const FVector& Origin, const FVector& Direction, float Length, float AngleWidth, float AngleHeight, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness, bool bInUseWires)
{
	AddCone(Origin, Direction, Length, AngleWidth, AngleHeight, CategoryName, Verbosity, Color, Description, Thickness, bInUseWires);
}
void FVisualLogEntry::AddCone(const FVector& Origin, const FVector& Direction, float Length, float AngleWidth, float AngleHeight, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness, bool bInUseWires)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(3);
	Element.Points.Add(Origin);
	Element.Points.Add(Direction);
	Element.Points.Add(FVector(Length, AngleWidth, AngleHeight));
	Element.Type = bInUseWires ? EVisualLoggerShapeElement::WireCone : EVisualLoggerShapeElement::Cone;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const FVector& Start, const FVector& End, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness, bool bInUseWires)
{
	AddCylinder(Start, End, Radius, CategoryName, Verbosity, Color, Description, Thickness, bInUseWires);
}
void FVisualLogEntry::AddCylinder(const FVector& Start, const FVector& End, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, uint16 Thickness, bool bInUseWires)
{
	FVisualLogShapeElement Element(Description, Color, Thickness, CategoryName);
	Element.Points.Reserve(3);
	Element.Points.Add(Start);
	Element.Points.Add(End);
	Element.Points.Add(FVector(Radius, Thickness, 0));
	Element.Type = bInUseWires ? EVisualLoggerShapeElement::WireCylinder : EVisualLoggerShapeElement::Cylinder;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const FVector& Base, float HalfHeight, float Radius, const FQuat& Rotation, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, bool bInUseWires)
{
	AddCapsule(Base, HalfHeight, Radius, Rotation, CategoryName, Verbosity, Color, Description, bInUseWires);
}
void FVisualLogEntry::AddCapsule(const FVector& Base, float HalfHeight, float Radius, const FQuat & Rotation, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description, bool bInUseWires)
{
	FVisualLogShapeElement Element(Description, Color, 0, CategoryName);
	Element.Points.Reserve(3);
	Element.Points.Add(Base);
	Element.Points.Add(FVector(HalfHeight, Radius, Rotation.X));
	Element.Points.Add(FVector(Rotation.Y, Rotation.Z, Rotation.W));
	Element.Type = bInUseWires ? EVisualLoggerShapeElement::WireCapsule : EVisualLoggerShapeElement::Capsule;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const TArray<FVector>& ConvexPoints, FVector::FReal MinZ, FVector::FReal MaxZ, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description)
{
	AddPulledConvex(ConvexPoints, MinZ, MaxZ, CategoryName, Verbosity, Color, Description);
}
void FVisualLogEntry::AddPulledConvex(const TArray<FVector>& ConvexPoints, FVector::FReal MinZ, FVector::FReal MaxZ, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description)
{
	FVisualLogShapeElement Element(Description, Color, 0, CategoryName);
	Element.Points.Reserve(1 + ConvexPoints.Num());
	Element.Points.Add(FVector(MinZ, MaxZ, 0.));
	Element.Points.Append(ConvexPoints);
	Element.Type = EVisualLoggerShapeElement::NavAreaMesh;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

// Deprecated : 
void FVisualLogEntry::AddElement(const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description)
{
	AddMesh(Vertices, Indices, CategoryName, Verbosity, Color, Description);
}
void FVisualLogEntry::AddMesh(const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description)
{
	FVisualLogShapeElement Element(Description, Color, 0, CategoryName);
	uint32 FacesNum = Indices.Num() / 3;
	Element.Points.Reserve(1 + Vertices.Num() + FacesNum);
	Element.Points.Add(FVector(Vertices.Num(), FacesNum, 0)); //add header data
	Element.Points.Append(Vertices);
	TArray<FVector> Faces;
	Faces.Reserve(FacesNum);
	for (int32 i = 0; i < Indices.Num(); i += 3)
	{
		Faces.Add(FVector(Indices[i + 0], Indices[i + 1], Indices[i + 2]));
	}
	Element.Points.Append(Faces);

	Element.Type = EVisualLoggerShapeElement::Mesh;
	Element.Verbosity = Verbosity;
	ElementsToDraw.Add(Element);
}

void FVisualLogEntry::AddConvexElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description)
{
	FVisualLogShapeElement Element(Description, Color, 0, CategoryName);
	Element.Points = Points;
	Element.Verbosity = Verbosity;
	Element.Type = EVisualLoggerShapeElement::Polygon;
	ElementsToDraw.Add(Element);
}


void FVisualLogEntry::AddHistogramData(const FVector2D& DataSample, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FName& GraphName, const FName& DataName)
{
	FVisualLogHistogramSample Sample;
	Sample.Category = CategoryName;
	Sample.GraphName = GraphName;
	Sample.DataName = DataName;
	Sample.SampleValue = DataSample;
	Sample.Verbosity = Verbosity;

	HistogramSamples.Add(Sample);
}

FVisualLogDataBlock& FVisualLogEntry::AddDataBlock(const FString& TagName, const TArray<uint8>& BlobDataArray, const FName& CategoryName, ELogVerbosity::Type Verbosity)
{
	FVisualLogDataBlock DataBlock;
	DataBlock.Category = CategoryName;
	DataBlock.TagName = *TagName;
	DataBlock.Data = BlobDataArray;
	DataBlock.Verbosity = Verbosity;

	const int32 Index = DataBlocks.Add(DataBlock);
	return DataBlocks[Index];
}

FArchive& operator<<(FArchive& Ar, FVisualLogDataBlock& Data)
{
	FVisualLoggerHelpers::Serialize(Ar, Data.TagName);
	FVisualLoggerHelpers::Serialize(Ar, Data.Category);
	Ar << Data.Verbosity;
	Ar << Data.Data;
	Ar << Data.UniqueId;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogHistogramSample& Sample)
{
	FVisualLoggerHelpers::Serialize(Ar, Sample.Category);
	FVisualLoggerHelpers::Serialize(Ar, Sample.GraphName);
	FVisualLoggerHelpers::Serialize(Ar, Sample.DataName);
	Ar << Sample.Verbosity;

	if (Ar.CustomVer(EVisualLoggerVersion::GUID) >= EVisualLoggerVersion::LargeWorldCoordinatesAndLocationValidityFlag)
	{
		Ar << Sample.SampleValue;
	}
	else
	{
		FVector2f SampleValueFlt;
		Ar << SampleValueFlt;
		Sample.SampleValue = FVector2D(SampleValueFlt);
	}

	Ar << Sample.UniqueId;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, EVisualLoggerShapeElement& Shape)
{
	uint8 ShapeAsInt = (uint8)Shape;
	Ar << ShapeAsInt;

	if (Ar.IsLoading())
	{
		Shape = (EVisualLoggerShapeElement)ShapeAsInt;
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogShapeElement& Element)
{
	FVisualLoggerHelpers::Serialize(Ar, Element.Category);
	Ar << Element.Description;
	Ar << Element.Verbosity;
	const int32 VLogsVer = Ar.CustomVer(EVisualLoggerVersion::GUID);

	const bool bUseLargeWorldCoordinates = (VLogsVer >= EVisualLoggerVersion::LargeWorldCoordinatesAndLocationValidityFlag);
	
	if (VLogsVer >= EVisualLoggerVersion::TransformationForShapes)
	{
		if (bUseLargeWorldCoordinates)
		{
			Ar << Element.TransformationMatrix;
		}
		else
		{
			FMatrix44f TransformationMatrixFlt;
        	Ar << TransformationMatrixFlt;
        	Element.TransformationMatrix = FMatrix(TransformationMatrixFlt);
		}
	}

	if (bUseLargeWorldCoordinates)
	{
		Ar << Element.Points;
	}
	else
	{
		TArray<FVector3f> FltPoints;
		Ar << FltPoints;
		Element.Points.Reserve(FltPoints.Num());
		for (FVector3f Point : FltPoints)
		{
			Element.Points.Emplace(Point);
		}
	}

	Ar << Element.UniqueId;
	Ar << Element.Type;
	Ar << Element.Color;
	Ar << Element.Thicknes;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogEvent& Event)
{
	Ar << Event.Name;
	Ar << Event.UserFriendlyDesc;
	Ar << Event.Verbosity;

	int32 NumberOfTags = Event.EventTags.Num();
	Ar << NumberOfTags;
	if (Ar.IsLoading())
	{
		for (int32 Index = 0; Index < NumberOfTags; ++Index)
		{
			FName Key = NAME_None;
			int32 Value = 0;
			FVisualLoggerHelpers::Serialize(Ar, Key);
			Ar << Value;
			Event.EventTags.Add(Key, Value);
		}
	}
	else
	{
		for (auto& CurrentTag : Event.EventTags)
		{
			FVisualLoggerHelpers::Serialize(Ar, CurrentTag.Key);
			Ar << CurrentTag.Value;
		}
	}

	Ar << Event.Counter;
	Ar << Event.UserData;
	FVisualLoggerHelpers::Serialize(Ar, Event.TagName);

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogLine& LogLine)
{
	FVisualLoggerHelpers::Serialize(Ar, LogLine.Category);
	FVisualLoggerHelpers::Serialize(Ar, LogLine.TagName);
	Ar << LogLine.Verbosity;
	Ar << LogLine.UniqueId;
	Ar << LogLine.UserData;
	Ar << LogLine.Line;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogStatusCategory& Status)
{
	Ar << Status.Category;
	Ar << Status.Data;

	const int32 VLogsVer = Ar.CustomVer(EVisualLoggerVersion::GUID);
	if (VLogsVer >= EVisualLoggerVersion::StatusCategoryWithChildren)
	{
		int32 NumChildren = Status.Children.Num();
		Ar << NumChildren;
		if (Ar.IsLoading())
		{
			for (int32 Index = 0; Index < NumChildren; ++Index)
			{
				FVisualLogStatusCategory CurrentChild;
				Ar << CurrentChild;
				Status.Children.Add(CurrentChild);
			}
		}
		else
		{
			for (auto& CurrentChild : Status.Children)
			{
				Ar << CurrentChild;
			}
		}
	}
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogEntry& LogEntry)
{
	const int32 VLogsOldVer = Ar.CustomVer(EVisualLoggerVersion::GUID);
	const int32 VLogsStreamObjectVer = Ar.CustomVer(FUE5MainStreamObjectVersion::GUID);

	if (VLogsStreamObjectVer >= FUE5MainStreamObjectVersion::VisualLoggerTimeStampAsDouble)
	{
		Ar << LogEntry.TimeStamp;
	}
	else
	{
		float TimeStampFlt = static_cast<float>(LogEntry.TimeStamp);
		Ar << TimeStampFlt;
		LogEntry.TimeStamp = TimeStampFlt;
	}

	if (VLogsStreamObjectVer < FUE5MainStreamObjectVersion::VisualLoggerAddedSeparateWorldTime)
	{
		LogEntry.WorldTimeStamp = LogEntry.TimeStamp;
	}
	else
	{
		Ar << LogEntry.WorldTimeStamp;
	}

	if (VLogsOldVer >= EVisualLoggerVersion::LargeWorldCoordinatesAndLocationValidityFlag)
	{
		Ar << LogEntry.Location;

		uint8 bTempIsLocationValid = (LogEntry.bIsLocationValid != 0);
		Ar.SerializeBits(&bTempIsLocationValid, 1);
		LogEntry.bIsLocationValid = bTempIsLocationValid != 0;
	}
	else
	{
		FVector3f LocationFlt(LogEntry.Location);
		Ar << LocationFlt;
		LogEntry.Location = FVector(LocationFlt);
	}

	Ar << LogEntry.LogLines;
	Ar << LogEntry.Status;
	Ar << LogEntry.Events;
	Ar << LogEntry.ElementsToDraw;
	Ar << LogEntry.DataBlocks;
	
	if (VLogsOldVer > EVisualLoggerVersion::Initial)
	{
		Ar << LogEntry.HistogramSamples;
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FVisualLogDevice::FVisualLogEntryItem& FrameCacheItem)
{
	FVisualLoggerHelpers::Serialize(Ar, FrameCacheItem.OwnerName);
	const int32 VLogsVer = Ar.CustomVer(EVisualLoggerVersion::GUID);
	if (VLogsVer >= EVisualLoggerVersion::AddedOwnerClassName)
	{
		FVisualLoggerHelpers::Serialize(Ar, FrameCacheItem.OwnerClassName);
	}
	Ar << FrameCacheItem.Entry;
	return Ar;
}

FString FVisualLoggerHelpers::GenerateTemporaryFilename(const FString& FileExt)
{
	return FString::Printf(TEXT("VTEMP_%s.%s"), *FDateTime::Now().ToString(), *FileExt);
}

FString FVisualLoggerHelpers::GenerateFilename(const FString& TempFileName, const FString& Prefix, double StartRecordingTime, double EndTimeStamp)
{
	const FString FullFilename = FString::Printf(TEXT("%s_%s"), *Prefix, *TempFileName);
	const FString TimeFrameString = FString::Printf(TEXT("%d-%d_"), FMath::TruncToInt(StartRecordingTime), FMath::TruncToInt(EndTimeStamp));
	return FullFilename.Replace(TEXT("VTEMP_"), *TimeFrameString, ESearchCase::CaseSensitive);
}

FArchive& FVisualLoggerHelpers::Serialize(FArchive& Ar, FName& Name)
{
	// Serialize the FName as a string
	if (Ar.IsLoading())
	{
		FString StringName;
		Ar << StringName;
		Name = FName(*StringName);
	}
	else
	{
		FString StringName = Name.ToString();
		Ar << StringName;
	}
	return Ar;
}

FArchive& FVisualLoggerHelpers::Serialize(FArchive& Ar, TArray<FVisualLogDevice::FVisualLogEntryItem>& RecordedLogs)
{
	Ar.UsingCustomVersion(EVisualLoggerVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		TArray<FVisualLogDevice::FVisualLogEntryItem> CurrentFrame;
		while (Ar.AtEnd() == false)
		{
			int32 FrameTag = VISUAL_LOGGER_MAGIC_NUMBER_LATEST;
			Ar << FrameTag;
			if (FrameTag != DEPRECATED_VISUAL_LOGGER_MAGIC_NUMBER && FrameTag != VISUAL_LOGGER_MAGIC_NUMBER_OLD_CUSTOM_VERSION && FrameTag != VISUAL_LOGGER_MAGIC_NUMBER_CUSTOM_VERSION_CONTAINER)
			{
				break;
			}

			if (FrameTag == VISUAL_LOGGER_MAGIC_NUMBER_CUSTOM_VERSION_CONTAINER)
			{
				FCustomVersionContainer CustomVersions;
				CustomVersions.Serialize(Ar);
				Ar.SetCustomVersions(CustomVersions);
			}
			else
			{
				Ar.SetCustomVersion(FUE5MainStreamObjectVersion::GUID, FUE5MainStreamObjectVersion::BeforeCustomVersionWasAdded, TEXT("VisualLogger"));

				if (FrameTag == VISUAL_LOGGER_MAGIC_NUMBER_OLD_CUSTOM_VERSION)
				{
					int32 ArchiveOldVer = -1;
					Ar << ArchiveOldVer;
					check(ArchiveOldVer >= 0);

					Ar.SetCustomVersion(EVisualLoggerVersion::GUID, ArchiveOldVer, TEXT("VisualLogger"));
				}
				else // DEPRECATED_VISUAL_LOGGER_MAGIC_NUMBER
				{
					Ar.SetCustomVersion(EVisualLoggerVersion::GUID, EVisualLoggerVersion::Initial, TEXT("VisualLogger"));
				}
			}

			Ar << CurrentFrame;
			RecordedLogs.Append(CurrentFrame);
			CurrentFrame.Reset();
		}
	}
	else
	{
		int32 FrameTag = VISUAL_LOGGER_MAGIC_NUMBER_LATEST;
		Ar << FrameTag;

		FCustomVersionContainer CustomVersions = Ar.GetCustomVersions();
		CustomVersions.Serialize(Ar);

		Ar << RecordedLogs;
	}

	return Ar;
}

void FVisualLoggerHelpers::GetCategories(const FVisualLogEntry& EntryItem, TArray<FVisualLoggerCategoryVerbosityPair>& OutCategories)
{
	for (const auto& Element : EntryItem.Events)
	{
		OutCategories.AddUnique(FVisualLoggerCategoryVerbosityPair(*Element.Name, Element.Verbosity));
	}

	for (const auto& Element : EntryItem.LogLines)
	{
		OutCategories.AddUnique(FVisualLoggerCategoryVerbosityPair(Element.Category, Element.Verbosity));
	}

	for (const auto& Element : EntryItem.ElementsToDraw)
	{
		OutCategories.AddUnique(FVisualLoggerCategoryVerbosityPair(Element.Category, Element.Verbosity));
	}

	for (const auto& Element : EntryItem.HistogramSamples)
	{
		OutCategories.AddUnique(FVisualLoggerCategoryVerbosityPair(Element.Category, Element.Verbosity));
	}

	for (const auto& Element : EntryItem.DataBlocks)
	{
		OutCategories.AddUnique(FVisualLoggerCategoryVerbosityPair(Element.Category, Element.Verbosity));
	}
}

void FVisualLoggerHelpers::GetHistogramCategories(const FVisualLogEntry& EntryItem, TMap<FString, TArray<FString> >& OutCategories)
{
	for (const auto& CurrentSample : EntryItem.HistogramSamples)
	{
		auto& DataNames = OutCategories.FindOrAdd(CurrentSample.GraphName.ToString());
		if (DataNames.Find(CurrentSample.DataName.ToString()) == INDEX_NONE)
		{
			DataNames.AddUnique(CurrentSample.DataName.ToString());
		}
	}
}

#endif //ENABLE_VISUAL_LOG

