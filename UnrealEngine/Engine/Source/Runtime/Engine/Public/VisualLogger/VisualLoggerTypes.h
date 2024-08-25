// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLoggerCustomVersion.h"

class AActor;
class UCanvas;
struct FLogEntryItem;

#define DEFINE_ENUM_TO_STRING(EnumType, EnumPackage) FString EnumToString(const EnumType Value) \
{ \
	static const UEnum* TypeEnum = FindObject<UEnum>(nullptr, TEXT(EnumPackage) TEXT(".") TEXT(#EnumType)); \
	return TypeEnum->GetNameStringByIndex(static_cast<int32>(Value)); \
}
#define DECLARE_ENUM_TO_STRING(EnumType) FString EnumToString(const EnumType Value)

enum class ECreateIfNeeded : int8
{
	Invalid = -1,
	DontCreate = 0,
	Create = 1,
};

// flags describing VisualLogger device's features
namespace EVisualLoggerDeviceFlags
{
	enum Type
	{
		NoFlags = 0,
		CanSaveToFile = 1,
		StoreLogsLocally = 2,
	};
}

//types of shape elements
enum class EVisualLoggerShapeElement : uint8
{
	Invalid = 0,
	SinglePoint, // individual points, rendered as plain spheres
	Sphere, 
	WireSphere,
	Segment, // pairs of points 
	Path,	// sequence of point
	Box,
	WireBox,
	Cone,
	WireCone,
	Cylinder,
	WireCylinder,
	Capsule,
	WireCapsule,
	Polygon,
	Mesh,
	NavAreaMesh, // convex based mesh with min and max Z values
	Arrow,
	Circle,
	// note that in order to remain backward compatibility in terms of log
	// serialization new enum values need to be added at the end
};

#if ENABLE_VISUAL_LOG
struct FVisualLogEventBase
{
	const FString Name;
	const FString FriendlyDesc;
	const ELogVerbosity::Type Verbosity;

	FVisualLogEventBase(const FString& InName, const FString& InFriendlyDesc, ELogVerbosity::Type InVerbosity)
		: Name(InName), FriendlyDesc(InFriendlyDesc), Verbosity(InVerbosity)
	{
	}
};

struct FVisualLogEvent
{
	FString Name;
	FString UserFriendlyDesc;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TMap<FName, int32>	 EventTags;
	int32 Counter;
	int64 UserData;
	FName TagName;

	FVisualLogEvent() : Counter(1) { /* Empty */ }
	ENGINE_API FVisualLogEvent(const FVisualLogEventBase& Event);
	ENGINE_API FVisualLogEvent& operator=(const FVisualLogEventBase& Event);

	friend inline bool operator==(const FVisualLogEvent& Left, const FVisualLogEvent& Right) 
	{ 
		return Left.Name == Right.Name; 
	}
};

struct FVisualLogLine
{
	FString Line;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	int32 UniqueId;
	int64 UserData;
	FName TagName;

	FVisualLogLine() { /* Empty */ }
	ENGINE_API FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine);
	ENGINE_API FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine, int64 InUserData);
};

struct FVisualLogStatusCategory
{
	TArray<FString> Data;
	FString Category;
	int32 UniqueId;
	TArray<FVisualLogStatusCategory> Children;

	ENGINE_API FVisualLogStatusCategory();
	explicit FVisualLogStatusCategory(const FString& InCategory/* = TEXT("")*/)
		: Category(InCategory)
	{
	}

	ENGINE_API void Add(const FString& Key, const FString& Value);
	ENGINE_API bool GetDesc(int32 Index, FString& Key, FString& Value) const;
	ENGINE_API void AddChild(const FVisualLogStatusCategory& Child);
};

struct FVisualLogShapeElement
{
	FString Description;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TArray<FVector> Points;
	FMatrix TransformationMatrix;
	int32 UniqueId;
	EVisualLoggerShapeElement Type;
	uint8 Color;
	union
	{
		uint16 Thicknes;
		uint16 Radius;
	};

	ENGINE_API FVisualLogShapeElement(EVisualLoggerShapeElement InType = EVisualLoggerShapeElement::Invalid);
	ENGINE_API FVisualLogShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness, const FName& InCategory);
	ENGINE_API void SetColor(const FColor& InColor);
	ENGINE_API EVisualLoggerShapeElement GetType() const;
	ENGINE_API void SetType(EVisualLoggerShapeElement InType);
	ENGINE_API FColor GetFColor() const;
};

struct FVisualLogHistogramSample
{
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	FName GraphName;
	FName DataName;
	FVector2D SampleValue;
	int32 UniqueId;
};

struct FVisualLogDataBlock
{
	FName TagName;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TArray<uint8>	Data;
	int32 UniqueId;
};
#endif  //ENABLE_VISUAL_LOG

struct FVisualLogEntry
{
#if ENABLE_VISUAL_LOG
	/** For absolute position of events along a timeline (can involve multiple worlds/game instances such as clients and server) */
	double TimeStamp;
	/** The time of the event according to its UWorld (can vary widely between game instances such as clients and server) */
	double WorldTimeStamp;
	FVector Location;
	uint8 bPassedClassAllowList : 1;
	uint8 bPassedObjectAllowList : 1;	
	uint8 bIsAllowedToLog : 1;
	uint8 bIsLocationValid : 1;
	uint8 bIsInitialized : 1;

	TArray<FVisualLogEvent> Events;
	TArray<FVisualLogLine> LogLines;
	TArray<FVisualLogStatusCategory> Status;
	TArray<FVisualLogShapeElement> ElementsToDraw;
	TArray<FVisualLogHistogramSample> HistogramSamples;
	TArray<FVisualLogDataBlock>	DataBlocks;

	FVisualLogEntry() { Reset(); }

	UE_DEPRECATED(5.4, "To be removed.  Build up the FVisualLogEntry manually or write your own helper function which doesn't rely on Children being IVisualLoggerDebugSnapshotInterface")
	ENGINE_API FVisualLogEntry(const AActor* InActor, TArray<TWeakObjectPtr<UObject> >* Children);
	
	UE_DEPRECATED(5.4, "To be removed.  Build up the FVisualLogEntry manually or write your own helper function which doesn't rely on Children being IVisualLoggerDebugSnapshotInterface")
	ENGINE_API FVisualLogEntry(double InTimeStamp, FVector InLocation, const UObject* Object, TArray<TWeakObjectPtr<UObject> >* Children);

	bool ShouldLog(const ECreateIfNeeded ShouldCreate) const
	{
		// We serialize and reinitialize entries only when allowed to log and parameter
		// indicates that new entry can be created.
		return bIsAllowedToLog && ShouldCreate == ECreateIfNeeded::Create;
	}

	bool ShouldFlush(double InTimeStamp) const
	{
		//Same LogOwner can be used for logs at different time in the frame so need to flush entry right away
		return bIsInitialized && InTimeStamp > TimeStamp;
	}

	ENGINE_API void InitializeEntry( const double InTimeStamp );
	ENGINE_API void Reset();
	ENGINE_API void SetPassedObjectAllowList(const bool bPassed);
	ENGINE_API void UpdateAllowedToLog();

	ENGINE_API void AddText(const FString& TextLine, const FName& CategoryName, ELogVerbosity::Type Verbosity);
	// path
	UE_DEPRECATED(5.4, "Use AddPath")
	ENGINE_API void AddElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	ENGINE_API void AddPath(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// location
	UE_DEPRECATED(5.4, "Use AddLocation")
	ENGINE_API void AddElement(const FVector& Point, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	ENGINE_API void AddLocation(const FVector& Point, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);	// location
	// sphere
	ENGINE_API void AddSphere(const FVector& Center, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), bool bInUseWires = false);
	// segment
	UE_DEPRECATED(5.4, "Use AddSegment")
	ENGINE_API void AddElement(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	ENGINE_API void AddSegment(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// box
	UE_DEPRECATED(5.4, "Use AddBox")
	ENGINE_API void AddElement(const FBox& Box, const FMatrix& Matrix, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0, bool bInUseWires = false);
	ENGINE_API void AddBox(const FBox& Box, const FMatrix& Matrix, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0, bool bInUseWires = false);
	// cone
	UE_DEPRECATED(5.4, "Use AddCone")
	ENGINE_API void AddElement(const FVector& Origin, const FVector& Direction, float Length, float AngleWidth, float AngleHeight, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0, bool bInUseWires = false);
	ENGINE_API void AddCone(const FVector& Origin, const FVector& Direction, float Length, float AngleWidth, float AngleHeight, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0, bool bInUseWires = false);
	// cylinder
	UE_DEPRECATED(5.4, "Use AddCylinder")
	ENGINE_API void AddElement(const FVector& Start, const FVector& End, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0, bool bInUseWires = false);
	ENGINE_API void AddCylinder(const FVector& Start, const FVector& End, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0, bool bInUseWires = false);
	// capsule
	UE_DEPRECATED(5.4, "Use AddCapsule")
	ENGINE_API void AddElement(const FVector& Base, float HalfHeight, float Radius, const FQuat & Rotation, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), bool bInUseWires = false);
	ENGINE_API void AddCapsule(const FVector& Base, float HalfHeight, float Radius, const FQuat & Rotation, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), bool bInUseWires = false);
	// custom element
	ENGINE_API void AddElement(const FVisualLogShapeElement& Element);
	// NavArea or vertically pulled convex shape
	UE_DEPRECATED(5.4, "Use AddPulledConvex")
	ENGINE_API void AddElement(const TArray<FVector>& ConvexPoints, FVector::FReal MinZ, FVector::FReal MaxZ, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	ENGINE_API void AddPulledConvex(const TArray<FVector>& ConvexPoints, FVector::FReal MinZ, FVector::FReal MaxZ, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// 3d Mesh
	UE_DEPRECATED(5.4, "Use AddMesh")
	ENGINE_API void AddElement(const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	ENGINE_API void AddMesh(const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// 2d convex
	ENGINE_API void AddConvexElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// histogram sample
	ENGINE_API void AddHistogramData(const FVector2D& DataSample, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FName& GraphName, const FName& DataName);
	// arrow
	ENGINE_API void AddArrow(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// boxes
	ENGINE_API void AddBoxes(const TArray<FBox>& Boxes, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White);
	// circle
	ENGINE_API void AddCircle(const FVector& Center, const FVector& UpAxis, const float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color, const FString& Description = TEXT(""), uint16 Thickness = 0);

	// Custom data block
	ENGINE_API FVisualLogDataBlock& AddDataBlock(const FString& TagName, const TArray<uint8>& BlobDataArray, const FName& CategoryName, ELogVerbosity::Type Verbosity);
	// Event
	ENGINE_API int32 AddEvent(const FVisualLogEventBase& Event);
	// find index of status category
	ENGINE_API int32 FindStatusIndex(const FString& CategoryName);

	// Moves all content to provided entry and reseting our content.
	ENGINE_API void MoveTo(FVisualLogEntry& Other);

#endif // ENABLE_VISUAL_LOG
};

#if  ENABLE_VISUAL_LOG

/**
 * Interface for Visual Logger Device
 */
class FVisualLogDevice
{
public:
	struct FVisualLogEntryItem
	{
		FVisualLogEntryItem() {}
		FVisualLogEntryItem(FName InOwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry) : OwnerName(InOwnerName), OwnerClassName(InOwnerClassName), Entry(LogEntry) { }

		FName OwnerName;
		FName OwnerClassName;
		FVisualLogEntry Entry;
	};

	virtual ~FVisualLogDevice() { }
	virtual void Serialize(const UObject* LogOwner, FName OwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry) = 0;
	virtual void Cleanup(bool bReleaseMemory = false) { /* Empty */ }
	virtual void StartRecordingToFile(double TimeStamp) { /* Empty */ }
	virtual void StopRecordingToFile(double TimeStamp) { /* Empty */ }
	UE_DEPRECATED(5.2, "Use the version which takes a double.")
	virtual void StartRecordingToFile(float TimeStamp) final { StartRecordingToFile(static_cast<double>(TimeStamp)); }
	UE_DEPRECATED(5.2, "Use the version which takes a double.")
	virtual void StopRecordingToFile(float TimeStamp) final { StopRecordingToFile(static_cast<double>(TimeStamp)); }
	virtual void DiscardRecordingToFile() { /* Empty */ }
	virtual void SetFileName(const FString& InFileName) { /* Empty */ }
	virtual void GetRecordedLogs(TArray<FVisualLogDevice::FVisualLogEntryItem>& OutLogs)  const { /* Empty */ }
	virtual bool HasFlags(int32 InFlags) const { return false; }
	FGuid GetSessionGUID() const { return SessionGUID; }
	uint32 GetShortSessionID() const { return SessionGUID[0]; }
protected:
	FGuid SessionGUID;
};

struct FVisualLoggerCategoryVerbosityPair
{
	FVisualLoggerCategoryVerbosityPair(FName Category, ELogVerbosity::Type InVerbosity) : CategoryName(Category), Verbosity(InVerbosity) {}

	FName CategoryName;
	ELogVerbosity::Type Verbosity;

	friend inline bool operator==(const FVisualLoggerCategoryVerbosityPair& A, const FVisualLoggerCategoryVerbosityPair& B)
	{
		return A.CategoryName == B.CategoryName
			&& A.Verbosity == B.Verbosity;
	}
};

struct FVisualLoggerHelpers
{
	static ENGINE_API FString GenerateTemporaryFilename(const FString& FileExt);
	static ENGINE_API FString GenerateFilename(const FString& TempFileName, const FString& Prefix, double StartRecordingTime, double EndTimeStamp);
	static ENGINE_API FArchive& Serialize(FArchive& Ar, FName& Name);
	static ENGINE_API FArchive& Serialize(FArchive& Ar, TArray<FVisualLogDevice::FVisualLogEntryItem>& RecordedLogs);
	static ENGINE_API void GetCategories(const FVisualLogEntry& RecordedLogs, TArray<FVisualLoggerCategoryVerbosityPair>& OutCategories);
	static ENGINE_API void GetHistogramCategories(const FVisualLogEntry& RecordedLogs, TMap<FString, TArray<FString> >& OutCategories);
};

struct IVisualLoggerEditorInterface
{
	virtual const FName& GetRowClassName(FName RowName) const = 0;
	virtual int32 GetSelectedItemIndex(FName RowName) const = 0;
	virtual const TArray<FVisualLogDevice::FVisualLogEntryItem>& GetRowItems(FName RowName) = 0;
	virtual const FVisualLogDevice::FVisualLogEntryItem& GetSelectedItem(FName RowName) const = 0;

	virtual const TArray<FName>& GetSelectedRows() const = 0;
	virtual bool IsRowVisible(FName RowName) const = 0;
	virtual bool IsItemVisible(FName RowName, int32 ItemIndex) const = 0;
	virtual UWorld* GetWorld() const = 0;
	virtual AActor* GetHelperActor(UWorld* InWorld = nullptr) const = 0;

	virtual bool MatchCategoryFilters(const FString& String, ELogVerbosity::Type Verbosity = ELogVerbosity::All) = 0;
};

class FVisualLogExtensionInterface
{
public:
	virtual ~FVisualLogExtensionInterface() { }

	virtual void ResetData(IVisualLoggerEditorInterface* EdInterface) = 0;
	virtual void DrawData(IVisualLoggerEditorInterface* EdInterface, UCanvas* Canvas) = 0;

	virtual void OnItemsSelectionChanged(IVisualLoggerEditorInterface* EdInterface) {};
	virtual void OnLogLineSelectionChanged(IVisualLoggerEditorInterface* EdInterface, TSharedPtr<struct FLogEntryItem> SelectedItem, int64 UserData) {};
	UE_DEPRECATED(5.2, "Use the version which takes a double.")
	virtual void OnScrubPositionChanged(IVisualLoggerEditorInterface* EdInterface, float NewScrubPosition, bool bScrubbing) final {}
	virtual void OnScrubPositionChanged(IVisualLoggerEditorInterface* EdInterface, double NewScrubPosition, bool bScrubbing) {}
};

ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogDevice::FVisualLogEntryItem& FrameCacheItem);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogDataBlock& Data);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogHistogramSample& Sample);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogShapeElement& Element);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogEvent& Event);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogLine& LogLine);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogStatusCategory& Status);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogEntry& LogEntry);

inline
FVisualLogEvent::FVisualLogEvent(const FVisualLogEventBase& Event)
: Counter(1)
{
	Name = Event.Name;
	UserFriendlyDesc = Event.FriendlyDesc;
	Verbosity = Event.Verbosity;
}

inline
FVisualLogEvent& FVisualLogEvent::operator= (const FVisualLogEventBase& Event)
{
	Name = Event.Name;
	UserFriendlyDesc = Event.FriendlyDesc;
	Verbosity = Event.Verbosity;
	return *this;
}

inline
FVisualLogLine::FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine)
: Line(InLine)
, Category(InCategory)
, Verbosity(InVerbosity)
, UserData(0)
{

}

inline
FVisualLogLine::FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine, int64 InUserData)
: Line(InLine)
, Category(InCategory)
, Verbosity(InVerbosity)
, UserData(InUserData)
{

}

inline
void FVisualLogStatusCategory::Add(const FString& Key, const FString& Value)
{
	Data.Add(FString(Key).AppendChar(TEXT('|')) + Value);
}

inline
void FVisualLogStatusCategory::AddChild(const FVisualLogStatusCategory& Child)
{
	Children.Add(Child);
}



inline
FVisualLogShapeElement::FVisualLogShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness, const FName& InCategory)
: Category(InCategory)
, Verbosity(ELogVerbosity::All)
, TransformationMatrix(FMatrix::Identity)
, Type(EVisualLoggerShapeElement::Invalid)
, Thicknes(InThickness)
{
	if (InDescription.IsEmpty() == false)
	{
		Description = InDescription;
	}
	SetColor(InColor);
}

inline
void FVisualLogShapeElement::SetColor(const FColor& InColor)
{
	Color = (uint8)(((InColor.DWColor() >> 30) << 6)	| (((InColor.DWColor() & 0x00ff0000) >> 22) << 4)	| (((InColor.DWColor() & 0x0000ff00) >> 14) << 2)	| ((InColor.DWColor() & 0x000000ff) >> 6));
}

inline
EVisualLoggerShapeElement FVisualLogShapeElement::GetType() const
{
	return Type;
}

inline
void FVisualLogShapeElement::SetType(EVisualLoggerShapeElement InType)
{
	Type = InType;
}

inline
FColor FVisualLogShapeElement::GetFColor() const
{
	FColor RetColor(((Color & 0xc0) << 24) | ((Color & 0x30) << 18) | ((Color & 0x0c) << 12) | ((Color & 0x03) << 6));
	RetColor.A = (RetColor.A * 255) / 192; // convert alpha to 0-255 range
	return RetColor;
}


inline
int32 FVisualLogEntry::FindStatusIndex(const FString& CategoryName)
{
	for (int32 TestCategoryIndex = 0; TestCategoryIndex < Status.Num(); TestCategoryIndex++)
	{
		if (Status[TestCategoryIndex].Category == CategoryName)
		{
			return TestCategoryIndex;
		}
	}

	return INDEX_NONE;
}

#endif // ENABLE_VISUAL_LOG
