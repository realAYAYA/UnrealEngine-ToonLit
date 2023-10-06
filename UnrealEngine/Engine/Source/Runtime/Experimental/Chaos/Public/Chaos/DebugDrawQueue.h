// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ChaosDebugDrawDeclares.h"
#include "Chaos/AABB.h"
#include "Containers/List.h"
#include "Math/Vector.h"
#include "Math/Color.h"
#include "Math/Quat.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/IConsoleManager.h"

class AActor;

#if CHAOS_DEBUG_DRAW
namespace Chaos
{
/** Thread-safe single-linked list (lock-free). (Taken from Light Mass, should probably just move into core) */
template<typename ElementType>
class TListThreadSafe
{
public:

	/** Initialization constructor. */
	TListThreadSafe() :
		FirstElement(nullptr)
	{}

	/**
	* Adds an element to the list.
	* @param Element	Newly allocated and initialized list element to add.
	*/
	void AddElement(TList<ElementType>* Element)
	{
		// Link the element at the beginning of the list.
		TList<ElementType>* LocalFirstElement;
		do
		{
			LocalFirstElement = FirstElement;
			Element->Next = LocalFirstElement;
		} while (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&FirstElement, Element, LocalFirstElement) != LocalFirstElement);
	}

	/**
	* Clears the list and returns the elements.
	* @return	List of all current elements. The original list is cleared. Make sure to delete all elements when you're done with them!
	*/
	TList<ElementType>* ExtractAll()
	{
		// Atomically read the complete list and clear the shared head pointer.
		TList<ElementType>* LocalFirstElement;
		do
		{
			LocalFirstElement = FirstElement;
		} while (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&FirstElement, NULL, LocalFirstElement) != LocalFirstElement);
		return LocalFirstElement;
	}

	/**
	*	Clears the list.
	*/
	void Clear()
	{
		while (FirstElement)
		{
			// Atomically read the complete list and clear the shared head pointer.
			TList<ElementType>* Element = ExtractAll();

			// Delete all elements in the local list.
			while (Element)
			{
				TList<ElementType>* NextElement = Element->Next;
				delete Element;
				Element = NextElement;
			};
		};
	}

private:

	TList<ElementType>* FirstElement;
};

struct FLatentDrawCommand
{
	FVector LineStart;
	FVector LineEnd;
	FColor Color;
	int32 Segments;
	bool bPersistentLines;
	float ArrowSize;
	float LifeTime;
	uint8 DepthPriority;
	float Thickness;
	FReal Radius;
	FReal HalfHeight;
	FVector Center;
	FVector Extent;
	FQuat Rotation;
	FVector TextLocation;
	FString Text;
	class AActor* TestBaseActor;
	bool bDrawShadow;
	float FontScale;
	float Duration;
	FMatrix TransformMatrix;
	bool bDrawAxis;
	FVector YAxis;
	FVector ZAxis;

	enum class EDrawType
	{
		Point,
		Line,
		DirectionalArrow,
		Sphere,
		Box,
		String,
		Circle,
		Capsule,
	} Type;

	FLatentDrawCommand()
		: TestBaseActor(nullptr)
	{
	}

	static FLatentDrawCommand DrawPoint(const FVector& Position, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		FLatentDrawCommand Command;
		Command.LineStart = Position;
		Command.Color = Color;
		Command.bPersistentLines = bPersistentLines;
		Command.LifeTime = LifeTime;
		Command.DepthPriority = DepthPriority;
		Command.Thickness = Thickness;
		Command.Type = EDrawType::Point;
		return Command;
	}


	static FLatentDrawCommand DrawLine(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		FLatentDrawCommand Command;
		Command.LineStart = LineStart;
		Command.LineEnd = LineEnd;
		Command.Color = Color;
		Command.bPersistentLines = bPersistentLines;
		Command.LifeTime = LifeTime;
		Command.DepthPriority = DepthPriority;
		Command.Thickness = Thickness;
		Command.Type = EDrawType::Line;
		return Command;
	}

	static FLatentDrawCommand DrawDirectionalArrow(const FVector& LineStart, FVector const& LineEnd, float ArrowSize, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		FLatentDrawCommand Command;
		Command.LineStart = LineStart;
		Command.LineEnd = LineEnd;
		Command.ArrowSize = ArrowSize;
		Command.Color = Color;
		Command.bPersistentLines = bPersistentLines;
		Command.LifeTime = LifeTime;
		Command.DepthPriority = DepthPriority;
		Command.Thickness = Thickness;
		Command.Type = EDrawType::DirectionalArrow;
		return Command;
	}

	static FLatentDrawCommand DrawDebugSphere(const FVector& Center, FVector::FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		FLatentDrawCommand Command;
		Command.LineStart = Center;
		Command.Radius = Radius;
		Command.Color = Color;
		Command.Segments = Segments;
		Command.bPersistentLines = bPersistentLines;
		Command.LifeTime = LifeTime;
		Command.DepthPriority = DepthPriority;
		Command.Thickness = Thickness;
		Command.Type = EDrawType::Sphere;
		return Command;
	}

	static FLatentDrawCommand DrawDebugBox(const FVector& Center, const FVector& Extent, const FQuat& Rotation, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		FLatentDrawCommand Command;
		Command.Center = Center;
		Command.Extent = Extent;
		Command.Rotation = Rotation;
		Command.Color = Color;
		Command.bPersistentLines = bPersistentLines;
		Command.LifeTime = LifeTime;
		Command.DepthPriority = DepthPriority;
		Command.Thickness = Thickness;
		Command.Type = EDrawType::Box;
		return Command;
	}

	static FLatentDrawCommand DrawDebugString(const FVector& TextLocation, const FString& Text, class AActor* TestBaseActor, const FColor& Color, float Duration, bool bDrawShadow, float FontScale)
	{
		FLatentDrawCommand Command;
		Command.TextLocation = TextLocation;
		Command.Text = Text;
		Command.TestBaseActor = TestBaseActor;
		Command.Color = Color;
		Command.Duration = Duration;
		Command.LifeTime = Duration;
		Command.bDrawShadow = bDrawShadow;
		Command.FontScale = FontScale;
		Command.Type = EDrawType::String;
		return Command;
	}

	static FLatentDrawCommand DrawDebugCircle(const FVector& Center, FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness, const FVector& YAxis, const FVector& ZAxis, bool bDrawAxis)
	{
		FLatentDrawCommand Command;
		Command.Center = Center;
		Command.Radius = Radius;
		Command.Segments = Segments;
		Command.Color = Color;
		Command.bPersistentLines = bPersistentLines;
		Command.LifeTime = LifeTime;
		Command.DepthPriority = DepthPriority;
		Command.Thickness = Thickness;
		Command.YAxis = YAxis;
		Command.ZAxis = ZAxis;
		Command.bDrawAxis = bDrawAxis;
		Command.Type = EDrawType::Circle;
		return Command;
	}

	static FLatentDrawCommand DrawDebugCapsule(const FVector& Center, FReal HalfHeight, FReal Radius, const FQuat& Rotation, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		FLatentDrawCommand Command;
		Command.Center = Center;
		Command.HalfHeight = HalfHeight;
		Command.Radius = Radius;
		Command.Rotation = Rotation;
		Command.Color = Color;
		Command.bPersistentLines = bPersistentLines;
		Command.LifeTime = LifeTime;
		Command.DepthPriority = DepthPriority;
		Command.Thickness = Thickness;
		Command.Type = EDrawType::Capsule;
		return Command;
	}

};

/** A thread safe way to generate latent debug drawing. (This is picked up later by the geometry collection component which is a total hack for now, but needed to get into an engine world ) */
class FDebugDrawQueue
{
public:
	enum EBuffer
	{
		Internal = 0,
		External = 1
	};

	void ExtractAllElements(TArray<FLatentDrawCommand>& OutDrawCommands, bool bPaused = false)
	{
		FScopeLock Lock(&CommandQueueCS);

		// Move the buffer into the output, and reserve space in the new buffer to prevent excess allocations and copies
		int Capacity = CommandQueue.Max();
		OutDrawCommands = MoveTemp(CommandQueue);
		CommandQueue.Reserve(Capacity);
		RequestedCommandCost = 0;
	}

	void DrawDebugPoint(const FVector& Position, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			if (AcceptCommand(1, Position))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawPoint(Position, Color, bPersistentLines, LifeTime, DepthPriority, Thickness));
			}
		}
	}

	void DrawDebugLine(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			if (AcceptCommand(1, LineStart))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawLine(LineStart, LineEnd, Color, bPersistentLines, LifeTime, DepthPriority, Thickness));
			}
		}
	}

	void DrawDebugDirectionalArrow(const FVector& LineStart, const FVector& LineEnd, float ArrowSize, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			if (AcceptCommand(3, LineStart))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawDirectionalArrow(LineStart, LineEnd, ArrowSize, Color, bPersistentLines, LifeTime, DepthPriority, Thickness));
			}
		}
	}

	void DrawDebugCoordinateSystem(const FVector& Position, const FRotator& AxisRot, FReal Scale, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);

			FRotationMatrix R(AxisRot);
			FVector const X = R.GetScaledAxis(EAxis::X);
			FVector const Y = R.GetScaledAxis(EAxis::Y);
			FVector const Z = R.GetScaledAxis(EAxis::Z);

			if (AcceptCommand(3, Position))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawLine(Position, Position + X * Scale, FColor::Red, bPersistentLines, LifeTime, DepthPriority, Thickness));
				CommandQueue.Emplace(FLatentDrawCommand::DrawLine(Position, Position + Y * Scale, FColor::Green, bPersistentLines, LifeTime, DepthPriority, Thickness));
				CommandQueue.Emplace(FLatentDrawCommand::DrawLine(Position, Position + Z * Scale, FColor::Blue, bPersistentLines, LifeTime, DepthPriority, Thickness));
			}
		}
	}


	void DrawDebugSphere(FVector const& Center, FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			const int Cost = Segments * Segments;
			if (AcceptCommand(Cost, Center))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawDebugSphere(Center, Radius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness));
			}
		}
	}

	void DrawDebugBox(FVector const& Center, FVector const& Extent, const FQuat& Rotation, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			if (AcceptCommand(12, Center))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawDebugBox(Center, Extent, Rotation, Color, bPersistentLines, LifeTime, DepthPriority, Thickness));
			}
		}
	}

	void DrawDebugString(FVector const& TextLocation, const FString& Text, class AActor* TestBaseActor, FColor const& Color, float Duration, bool bDrawShadow, float FontScale)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			int Cost = Text.Len();
			if (AcceptCommand(Cost, TextLocation))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawDebugString(TextLocation, Text, TestBaseActor, Color, Duration, bDrawShadow, FontScale));
			}
		}
	}

	void DrawDebugCircle(const FVector& Center, FReal Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness, const FVector& YAxis, const FVector& ZAxis, bool bDrawAxis)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			if (AcceptCommand(Segments, Center))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawDebugCircle(Center, Radius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness, YAxis, ZAxis, bDrawAxis));
			}
		}
	}

	void DrawDebugCapsule(const FVector& Center, FReal HalfHeight, FReal Radius, const FQuat& Rotation, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
	{
		if (IsDebugDrawingEnabled())
		{
			FScopeLock Lock(&CommandQueueCS);
			if (AcceptCommand(16, Center))
			{
				CommandQueue.Emplace(FLatentDrawCommand::DrawDebugCapsule(Center, HalfHeight, Radius, Rotation, Color, bPersistentLines, LifeTime, DepthPriority, Thickness));
			}
		}
	}

	void SetEnabled(bool bInEnabled)
	{
		FScopeLock Lock(&CommandQueueCS);
		bEnableDebugDrawing = bInEnabled;
	}

	void SetMaxCost(int32 InMaxCost)
	{
		FScopeLock Lock(&CommandQueueCS);
		MaxCommandCost = InMaxCost;
	}

	void SetRegionOfInterest(const FVector& Pos, float InRadius)
	{
		FScopeLock Lock(&CommandQueueCS);
		CenterOfInterest = Pos;
		RadiusOfInterest = InRadius;
	}

	const FVector& GetCenterOfInterest() const
	{
		return CenterOfInterest;
	}

	FReal GetRadiusOfInterest() const
	{
		return RadiusOfInterest;
	}

	bool IsInRegionOfInterest(FVector Pos) const
	{
		return (RadiusOfInterest <= 0.0f) || ((Pos - CenterOfInterest).SizeSquared() < RadiusOfInterest * RadiusOfInterest);
	}

	bool IsInRegionOfInterest(FVector Pos, FReal Radius) const
	{
		return (RadiusOfInterest <= 0.0f) || ((Pos - CenterOfInterest).SizeSquared() < (RadiusOfInterest + Radius) * (RadiusOfInterest + Radius));
	}

	bool IsInRegionOfInterest(FAABB3 Bounds) const
	{
		return Bounds.ThickenSymmetrically(FVec3(RadiusOfInterest)).Contains(CenterOfInterest);
	}

	CHAOS_API void SetConsumerActive(void* Consumer, bool bConsumerActive);

	static CHAOS_API FDebugDrawQueue& GetInstance();

	static bool IsDebugDrawingEnabled()
	{
		// Don't queue up debug draws unless something is consuming the queue, otherwise it can build up forever
		return GetInstance().bEnableDebugDrawing && (GetInstance().NumConsumers > 0);
	}

private:
	FDebugDrawQueue()
		: RequestedCommandCost(0)
		, MaxCommandCost(10000)
		, CenterOfInterest(FVector::ZeroVector)
		, RadiusOfInterest(0)
		, bEnableDebugDrawing(false)
	{}
	~FDebugDrawQueue() {}

	bool AcceptCommand(int Cost, const FVec3& Position)
	{
		if (IsInRegionOfInterest(Position))
		{
			RequestedCommandCost += Cost;
			return IsInBudget();
		}
		return false;
	}

	bool IsInBudget() const
	{
		return (MaxCommandCost <= 0) || (RequestedCommandCost <= MaxCommandCost);
	}

	TArray<FLatentDrawCommand> CommandQueue;
	int32 RequestedCommandCost;
	int32 MaxCommandCost;
	FCriticalSection CommandQueueCS;

	TArray<void*> Consumers;
	int32 NumConsumers;
	FCriticalSection ConsumersCS;

	FVector CenterOfInterest;
	FReal RadiusOfInterest;
	bool bEnableDebugDrawing;
};
}
#endif
