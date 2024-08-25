// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerTypes.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "EngineStats.h"
#endif
#include "Templates/IsValidVariadicFunctionArg.h"
#include "Templates/IsArrayOrRefOfTypeByPredicate.h"
#include "Traits/IsCharEncodingCompatibleWith.h"
#include "UObject/ObjectKey.h"
#include "Containers/Ticker.h"

#if ENABLE_VISUAL_LOG

#define REDIRECT_TO_VLOG(Dest) FVisualLogger::Redirect(this, Dest)
#define REDIRECT_OBJECT_TO_VLOG(Src, Dest) FVisualLogger::Redirect(Src, Dest)

#define CONNECT_WITH_VLOG(Dest)
#define CONNECT_OBJECT_WITH_VLOG(Src, Dest)

// Text, regular log
#define UE_VLOG(LogOwner, CategoryName, Verbosity, Format, ...) if( FVisualLogger::IsRecording() ) FVisualLogger::CategorizedLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Format, ##__VA_ARGS__)
#define UE_CVLOG(Condition, LogOwner, CategoryName, Verbosity, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG(LogOwner, CategoryName, Verbosity, Format, ##__VA_ARGS__);} 
// Text, log with output to regular unreal logs too.  NOTE: UE_VLOG_UELOG will not UELOG if the Visual Logger is disabled (i.e. ENABLE_VISUAL_LOG is 0).  For the more common case where you want to always log, use UE_VLOG_ALWAYS_UELOG
#define UE_VLOG_UELOG(LogOwner, CategoryName, Verbosity, Format, ...) { if(FVisualLogger::IsRecording()) FVisualLogger::CategorizedLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Format, ##__VA_ARGS__); UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); }
#define UE_CVLOG_UELOG(Condition, LogOwner, CategoryName, Verbosity, Format, ...)  if(Condition) {UE_VLOG_UELOG(LogOwner, CategoryName, Verbosity, Format, ##__VA_ARGS__);} 
// Text, log with output to regular unreal logs too.  Regular log will always happen even if the Visual Logger is disabled by via compiler switch.  See also UE_CVLOG_ALWAYS_UELOG below
#define UE_VLOG_ALWAYS_UELOG(LogOwner, CategoryName, Verbosity, Format, ...) { UE_VLOG_UELOG(LogOwner, CategoryName, Verbosity, Format, ##__VA_ARGS__); }
// Segment shape
#define UE_VLOG_SEGMENT(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::SegmentLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, SegmentStart, SegmentEnd, Color, 0, Format, ##__VA_ARGS__)
#define UE_CVLOG_SEGMENT(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_SEGMENT(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ##__VA_ARGS__);}
// Segment shape
#define UE_VLOG_SEGMENT_THICK(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::SegmentLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ##__VA_ARGS__)
#define UE_CVLOG_SEGMENT_THICK(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_SEGMENT_THICK(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ##__VA_ARGS__);} 
// Localization as sphere shape
#define UE_VLOG_LOCATION(LogOwner, CategoryName, Verbosity, Location, Thickness, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::LocationLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Location, Thickness, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_LOCATION(Condition, LogOwner, CategoryName, Verbosity, Location, Thickness, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_LOCATION(LogOwner, CategoryName, Verbosity, Location, Thickness, Color, Format, ##__VA_ARGS__);} 
// Sphere shape
#define UE_VLOG_SPHERE(LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::SphereLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Location, Radius, Color, /*bWireframe = */false, Format, ##__VA_ARGS__)
#define UE_CVLOG_SPHERE(Condition, LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_SPHERE(LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ##__VA_ARGS__);} 
// Wire sphere shape
#define UE_VLOG_WIRESPHERE(LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::SphereLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Location, Radius, Color, /*bWireframe = */true, Format, ##__VA_ARGS__)
#define UE_CVLOG_WIRESPHERE(Condition, LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_WIRESPHERE(LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ##__VA_ARGS__);} 
// Box shape
#define UE_VLOG_BOX(LogOwner, CategoryName, Verbosity, Box, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::BoxLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Box, FMatrix::Identity, Color, /*bWireframe = */false, Format, ##__VA_ARGS__)
#define UE_CVLOG_BOX(Condition, LogOwner, CategoryName, Verbosity, Box, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_BOX(LogOwner, CategoryName, Verbosity, Box, Color, Format, ##__VA_ARGS__);} 
// Wire box shape
#define UE_VLOG_WIREBOX(LogOwner, CategoryName, Verbosity, Box, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::BoxLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Box, FMatrix::Identity, Color, /*bWireframe = */true, Format, ##__VA_ARGS__)
#define UE_CVLOG_WIREBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_BOX(LogOwner, CategoryName, Verbosity, Box, Color, Format, ##__VA_ARGS__);} 
// Oriented box shape
#define UE_VLOG_OBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::BoxLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Box, Matrix, Color, /*bWireframe = */false, Format, ##__VA_ARGS__)
#define UE_CVLOG_OBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_OBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ##__VA_ARGS__);} 
// Wire oriented box shape
#define UE_VLOG_WIREOBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::BoxLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Box, Matrix, Color, /*bWireframe = */true, Format, ##__VA_ARGS__)
#define UE_CVLOG_WIREOBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_OBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ##__VA_ARGS__);} 
// Cone shape
#define UE_VLOG_CONE(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::ConeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Origin, Direction, Length, Angle, Color, /*bWireframe = */false, Format, ##__VA_ARGS__)
#define UE_CVLOG_CONE(Condition, LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CONE(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, Format, ##__VA_ARGS__);} 
// Wire cone shape
#define UE_VLOG_WIRECONE(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::ConeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Origin, Direction, Length, Angle, Color, /*bWireframe = */true, Format, ##__VA_ARGS__)
#define UE_CVLOG_WIRECONE(Condition, LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CONE(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, Format, ##__VA_ARGS__);} 
// Cylinder shape
#define UE_VLOG_CYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::CylinderLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Start, End, Radius, Color, /*bWireframe = */false, Format, ##__VA_ARGS__)
#define UE_CVLOG_CYLINDER(Condition, LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ##__VA_ARGS__);} 
// Wire cylinder shape
#define UE_VLOG_WIRECYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::CylinderLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Start, End, Radius, Color, /*bWireframe = */true, Format, ##__VA_ARGS__)
#define UE_CVLOG_WIRECYLINDER(Condition, LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ##__VA_ARGS__);} 
// Capsule shape
#define UE_VLOG_CAPSULE(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::CapsuleLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Base, HalfHeight, Radius, Rotation, Color, /*bWireframe = */false, Format, ##__VA_ARGS__)
#define UE_CVLOG_CAPSULE(Condition, LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CAPSULE(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, Format, ##__VA_ARGS__);} 
// Wire capsule shape
#define UE_VLOG_WIRECAPSULE(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::CapsuleLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Base, HalfHeight, Radius, Rotation, Color, /*bWireframe = */true, Format, ##__VA_ARGS__)
#define UE_CVLOG_WIRECAPSULE(Condition, LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CAPSULE(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, Format, ##__VA_ARGS__);} 
// Histogram data for 2d graphs 
#define UE_VLOG_HISTOGRAM(LogOwner, CategoryName, Verbosity, GraphName, DataName, Data) if(FVisualLogger::IsRecording()) FVisualLogger::HistogramDataLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, GraphName, DataName, Data, FColor::White, TEXT(""))
#define UE_CVLOG_HISTOGRAM(Condition, LogOwner, CategoryName, Verbosity, GraphName, DataName, Data) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_HISTOGRAM(LogOwner, CategoryName, Verbosity, GraphName, DataName, Data);} 
// NavArea or vertically pulled convex shape
#define UE_VLOG_PULLEDCONVEX(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::PulledConvexLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_PULLEDCONVEX(Condition, LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_PULLEDCONVEX(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ##__VA_ARGS__);}
// regular 3d mesh shape to log
#define UE_VLOG_MESH(LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, Format, ...) if (FVisualLogger::IsRecording()) FVisualLogger::MeshLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Vertices, Indices, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_MESH(Condition, LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_MESH(LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, Format, ##__VA_ARGS__);}
// 2d convex poly shape
#define UE_VLOG_CONVEXPOLY(LogOwner, CategoryName, Verbosity, Points, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::ConvexLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Points, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_CONVEXPOLY(Condition, LogOwner, CategoryName, Verbosity, Points, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CONVEXPOLY(LogOwner, CategoryName, Verbosity, Points, Color, Format, ##__VA_ARGS__);}
// Segment with an arrowhead
#define UE_VLOG_ARROW(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::ArrowLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, SegmentStart, SegmentEnd, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_ARROW(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_ARROW(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ##__VA_ARGS__);} 
// Circle shape
#define UE_VLOG_CIRCLE(LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::CircleLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Center, UpAxis, Radius, Color, 0, Format, ##__VA_ARGS__)
#define UE_CVLOG_CIRCLE(Condition, LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CIRCLE(LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, 0, Format, ##__VA_ARGS__);} 
// Circle shape
#define UE_VLOG_CIRCLE_THICK(LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Thickness, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::CircleLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Center, UpAxis, Radius, Color, Thickness, Format, ##__VA_ARGS__)
#define UE_CVLOG_CIRCLE_THICK(Condition, LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Thickness, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CIRCLE(LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Thickness, Format, ##__VA_ARGS__);} 

#define DECLARE_VLOG_EVENT(EventName) extern FVisualLogEventBase EventName;
#define DEFINE_VLOG_EVENT(EventName, Verbosity, UserFriendlyDesc) FVisualLogEventBase EventName(TEXT(#EventName), TEXT(UserFriendlyDesc), ELogVerbosity::Verbosity); 

#define UE_VLOG_EVENTS(LogOwner, TagNameToLog, ...) if(FVisualLogger::IsRecording()) FVisualLogger::EventLog(LogOwner, TagNameToLog, ##__VA_ARGS__)
#define UE_CVLOG_EVENTS(Condition, LogOwner, TagNameToLog, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_EVENTS(LogOwner, TagNameToLog, ##__VA_ARGS__);}
#define UE_VLOG_EVENT_WITH_DATA(LogOwner, LogEvent, ...) if(FVisualLogger::IsRecording()) FVisualLogger::EventLog(LogOwner, LogEvent, ##__VA_ARGS__)
#define UE_CVLOG_EVENT_WITH_DATA(Condition, LogOwner, LogEvent, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_EVENT_WITH_DATA(LogOwner, LogEvent, ##__VA_ARGS__);}

#define UE_IFVLOG(__code_block__) if( FVisualLogger::IsRecording() ) { __code_block__; }

#else // if !ENABLE_VISUAL_LOG
#define REDIRECT_TO_VLOG(Dest)
#define REDIRECT_OBJECT_TO_VLOG(Src, Dest)
#define CONNECT_WITH_VLOG(Dest)
#define CONNECT_OBJECT_WITH_VLOG(Src, Dest)

#define UE_VLOG(LogOwner, CategoryName, Verbosity, Format, ...)
#define UE_CVLOG(Condition, LogOwner, CategoryName, Verbosity, Format, ...)
#define UE_VLOG_UELOG(LogOwner, CategoryName, Verbosity, Format, ...)
// Always UELOG version will just become a UE_LOG when Visual Logging is disabled.
#define UE_VLOG_ALWAYS_UELOG(LogOwner, CategoryName, Verbosity, Format, ...) { UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); }
#define UE_CVLOG_UELOG(Condition, LogOwner, CategoryName, Verbosity, Format, ...)
#define UE_VLOG_SEGMENT(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, DescriptionFormat, ...)
#define UE_CVLOG_SEGMENT(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, DescriptionFormat, ...)
#define UE_VLOG_SEGMENT_THICK(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, DescriptionFormat, ...)
#define UE_CVLOG_SEGMENT_THICK(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, DescriptionFormat, ...)
#define UE_VLOG_LOCATION(LogOwner, CategoryName, Verbosity, Location, Thickness, Color, DescriptionFormat, ...)
#define UE_CVLOG_LOCATION(Condition, LogOwner, CategoryName, Verbosity, Location, Thickness, Color, DescriptionFormat, ...)
#define UE_VLOG_SPHERE(LogOwner, CategoryName, Verbosity, Location, Radius, Color, DescriptionFormat, ...)
#define UE_CVLOG_SPHERE(Condition, LogOwner, CategoryName, Verbosity, Location, Radius, Color, DescriptionFormat, ...)
#define UE_VLOG_WIRESPHERE(LogOwner, CategoryName, Verbosity, Location, Radius, Color, DescriptionFormat, ...)
#define UE_CVLOG_WIRESPHERE(Condition, LogOwner, CategoryName, Verbosity, Location, Radius, Color, DescriptionFormat, ...)
#define UE_VLOG_BOX(LogOwner, CategoryName, Verbosity, Box, Color, DescriptionFormat, ...) 
#define UE_CVLOG_BOX(Condition, LogOwner, CategoryName, Verbosity, Box, Color, DescriptionFormat, ...) 
#define UE_VLOG_WIREBOX(LogOwner, CategoryName, Verbosity, Box, Color, DescriptionFormat, ...) 
#define UE_CVLOG_WIREBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Color, DescriptionFormat, ...) 
#define UE_VLOG_OBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...)  
#define UE_CVLOG_OBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) 
#define UE_VLOG_WIREOBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...)  
#define UE_CVLOG_WIREOBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) 
#define UE_VLOG_CONE(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, DescriptionFormat, ...)
#define UE_CVLOG_CONE(Condition, LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, DescriptionFormat, ...)
#define UE_VLOG_WIRECONE(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, DescriptionFormat, ...)
#define UE_CVLOG_WIRECONE(Condition, LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, DescriptionFormat, ...)
#define UE_VLOG_CYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, DescriptionFormat, ...)
#define UE_CVLOG_CYLINDER(Condition, LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, DescriptionFormat, ...)
#define UE_VLOG_WIRECYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, DescriptionFormat, ...)
#define UE_CVLOG_WIRECYLINDER(Condition, LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, DescriptionFormat, ...)
#define UE_VLOG_CAPSULE(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, DescriptionFormat, ...)
#define UE_CVLOG_CAPSULE(Condition, LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, DescriptionFormat, ...)
#define UE_VLOG_WIRECAPSULE(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, DescriptionFormat, ...)
#define UE_CVLOG_WIRECAPSULE(Condition, LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, DescriptionFormat, ...)
#define UE_VLOG_HISTOGRAM(LogOwner, CategoryName, Verbosity, GraphName, DataName, Data)
#define UE_CVLOG_HISTOGRAM(Condition, LogOwner, CategoryName, Verbosity, GraphName, DataName, Data)
#define UE_VLOG_PULLEDCONVEX(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...)
#define UE_CVLOG_PULLEDCONVEX(Condition, LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...)
#define UE_VLOG_MESH(LogOwner, CategoryName, Verbosity, Vertices, Indexes, Color, Format, ...) 
#define UE_CVLOG_MESH(Condition, LogOwner, CategoryName, Verbosity, Vertices, Indexes, Color, Format, ...) 
#define UE_VLOG_CONVEXPOLY(LogOwner, CategoryName, Verbosity, Points, Color, Format, ...) 
#define UE_CVLOG_CONVEXPOLY(Condition, LogOwner, CategoryName, Verbosity, Points, Color, Format, ...)
#define UE_VLOG_ARROW(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) 
#define UE_CVLOG_ARROW(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) 
#define UE_VLOG_CIRCLE(LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Format, ...)
#define UE_CVLOG_CIRCLE(Condition, LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Format, ...)
#define UE_VLOG_CIRCLE_THICK(LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Thickness, Format, ...)
#define UE_CVLOG_CIRCLE_THICK(Condition, LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Thickness, Format, ...)

#define DECLARE_VLOG_EVENT(EventName)
#define DEFINE_VLOG_EVENT(EventName, Verbosity, UserFriendlyDesc)
#define UE_VLOG_EVENTS(LogOwner, TagNameToLog, ...) 
#define UE_CVLOG_EVENTS(Condition, LogOwner, TagNameToLog, ...) 
#define UE_VLOG_EVENT_WITH_DATA(LogOwner, LogEvent, ...)
#define UE_CVLOG_EVENT_WITH_DATA(Condition, LogOwner, LogEvent, ...)

#define UE_IFVLOG(__code_block__)

#endif //ENABLE_VISUAL_LOG

// Definition is the same with/without ENABLE_VISUAL_LOG, because it's dependent on UE_VLOG_ALWAYS_UELOG which handles
// the difference.
// Text, log with output to regular logs if the condition is met... regular log will still happen if condition is true
// even when the compiler switch (ENABLE_VISUAL_LOG) is off.
#define UE_CVLOG_ALWAYS_UELOG(Condition, LogOwner, CategoryName, Verbosity, Format, ...) if (Condition) { UE_VLOG_ALWAYS_UELOG(LogOwner, CategoryName, Verbosity, Format, ##__VA_ARGS__); }

// helper macros
#define TEXT_EMPTY TEXT("")
#define TEXT_NULL TEXT("NULL")
#define TEXT_TRUE TEXT("TRUE")
#define TEXT_FALSE TEXT("FALSE")
#define TEXT_CONDITION(Condition) ((Condition) ? TEXT_TRUE : TEXT_FALSE)

class FVisualLogger;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogVisual, Display, All);

#if ENABLE_VISUAL_LOG

class FVisualLogDevice;
class FVisualLogExtensionInterface;
class UObject;
class UWorld;
struct FLogCategoryBase;

DECLARE_DELEGATE_RetVal(FString, FVisualLogFilenameGetterDelegate);

class FVisualLogger : public FOutputDevice
{
	static ENGINE_API void CategorizedLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...);
	static ENGINE_API void SegmentLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const uint16 Thickness, const TCHAR* Fmt, ...);
	static ENGINE_API void LocationLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Center, uint16 Thickness, const FColor& Color, const TCHAR* Fmt, ...);
	static ENGINE_API void SphereLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Center, float Radius, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...);
	static ENGINE_API void BoxLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FBox& Box, const FMatrix& Matrix, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...);
	static ENGINE_API void ConeLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Origin, const FVector& Direction, const float Length, const float Angle, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...);
	static ENGINE_API void CylinderLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const float Radius, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...);
	static ENGINE_API void CapsuleLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Base, float HalfHeight, float Radius, const FQuat& Rotation, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...);
	static ENGINE_API void PulledConvexLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& ConvexPoints, float MinZ, float MaxZ, const FColor& Color, const TCHAR* Fmt, ...);
	static ENGINE_API void MeshLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FColor& Color, const TCHAR* Fmt, ...);
	static ENGINE_API void ConvexLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Points, const FColor& Color, const TCHAR* Fmt, ...);
	static ENGINE_API void HistogramDataLogfImpl (const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, FName GraphName, FName DataName, const FVector2D& Data, const FColor& Color, const TCHAR* Fmt, ...);
	static ENGINE_API void ArrowLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const TCHAR* Fmt, ...);
	static ENGINE_API void CircleLogfImpl(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Center, const FVector& UpAxis, const float Radius, const FColor& Color, const uint16 Thickness, const TCHAR* Fmt, ...);

public:
	// Regular text log
	template <typename FmtType, typename... Types>
	static void CategorizedLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CategorizedLogf");

		CategorizedLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void CategorizedLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CategorizedLogf");

		CategorizedLogfImpl(LogOwner, CategoryName, Verbosity, (const TCHAR*)Fmt, Args...);
	}

	// Segment log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use SegmentLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const uint16 Thickness, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		SegmentLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Start, End, Color, Thickness, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use SegmentLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const uint16 Thickness, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		SegmentLogfImpl(LogOwner, CategoryName, Verbosity, Start, End, Color, Thickness, (const TCHAR*)Fmt, Args...);
	}

	template <typename FmtType, typename... Types>
	static void SegmentLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const uint16 Thickness, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::SegmentLogf");

		SegmentLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Start, End, Color, Thickness, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void SegmentLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const uint16 Thickness, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::SegmentLogf");

		SegmentLogfImpl(LogOwner, CategoryName, Verbosity, Start, End, Color, Thickness, (const TCHAR*)Fmt, Args...);
	}

	// Arrow
	template <typename FmtType, typename... Types>
	static void ArrowLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::ArrowLogf");
		ArrowLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Start, End, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void ArrowLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::ArrowLogf");

		ArrowLogfImpl(LogOwner, CategoryName, Verbosity, Start, End, Color, (const TCHAR*)Fmt, Args...);
	}


	// Circle log
	template <typename FmtType, typename... Types>
	static void CircleLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Center, const FVector& UpAxis, const float Radius, const FColor& Color, const uint16 Thickness, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CircleLogf");

		CircleLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Center, UpAxis, Radius, Color, Thickness, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void CircleLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Center, const FVector& UpAxis, const float Radius, const FColor& Color, const uint16 Thickness, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CircleLogf");

		CircleLogfImpl(LogOwner, CategoryName, Verbosity, Center, UpAxis, Radius, Color, Thickness, (const TCHAR*)Fmt, Args...);
	}

	// Location log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use LocationLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Location, float Radius, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		LocationLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Location, static_cast<uint16>(Radius), Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use LocationLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Location, float Radius, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		LocationLogfImpl(LogOwner, CategoryName, Verbosity, Location, static_cast<uint16>(Radius), Color, (const TCHAR*)Fmt, Args...);
	}

	template <typename FmtType, typename... Types>
	static void LocationLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Location, const uint16 Thickness, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::LocationLogf");

		LocationLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Location, Thickness, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void LocationLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Location, const uint16 Thickness, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::LocationLogf");

		LocationLogfImpl(LogOwner, CategoryName, Verbosity, Location, Thickness, Color, (const TCHAR*)Fmt, Args...);
	}

	// Sphere log
	template <typename FmtType, typename... Types>
	static void SphereLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Location, float Radius, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to FVisualLogger::SphereLogf");

		SphereLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Location, Radius, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void SphereLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Location, float Radius, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to FVisualLogger::SphereLogf");

		SphereLogfImpl(LogOwner, CategoryName, Verbosity, Location, Radius, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}

	// Box log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use BoxLogf")
	static void GeometryBoxLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FBox& Box, const FMatrix& Matrix, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryBoxLogf");

		BoxLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Box, Matrix, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use BoxLogf")
	static void GeometryBoxLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FBox& Box, const FMatrix& Matrix, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryBoxLogf");

		BoxLogfImpl(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void BoxLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FBox& Box, const FMatrix& Matrix, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::BoxLogf");

		BoxLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Box, Matrix, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void BoxLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FBox& Box, const FMatrix& Matrix, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::BoxLogf");

		BoxLogfImpl(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}

	// Cone log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use ConeLogfImpl")
	static void GeometryShapeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Origin, const FVector& Direction, const float Length, const float Angle, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		ConeLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Origin, Direction, Length, Angle, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use ConeLogfImpl")
	static void GeometryShapeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Origin, const FVector& Direction, const float Length, const float Angle, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		ConeLogfImpl(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void ConeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Origin, const FVector& Direction, const float Length, const float Angle, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::ConeLogf");

		ConeLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Origin, Direction, Length, Angle, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void ConeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Origin, const FVector& Direction, const float Length, const float Angle, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::ConeLogf");

		ConeLogfImpl(LogOwner, CategoryName, Verbosity, Origin, Direction, Length, Angle, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}

	// Cylinder log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use CylinderLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const float Radius, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		CylinderLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Start, End, Radius, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use CylinderLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const float Radius, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		CylinderLogfImpl(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void CylinderLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const float Radius, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CylinderLogf");

		CylinderLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Start, End, Radius, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void CylinderLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const float Radius, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CylinderLogf");

		CylinderLogfImpl(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}

	// Capsule log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use CapsuleLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Base, float HalfHeight, float Radius, const FQuat & Rotation, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		CapsuleLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Base, HalfHeight, Radius, Rotation, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use CapsuleLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Base, float HalfHeight, float Radius, const FQuat & Rotation, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		CapsuleLogfImpl(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, /*bWireframe = */false, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void CapsuleLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FVector& Base, float HalfHeight, float Radius, const FQuat & Rotation, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CapsuleLogf");

		CapsuleLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Base, HalfHeight, Radius, Rotation, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void CapsuleLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Base, float HalfHeight, float Radius, const FQuat & Rotation, const FColor& Color, bool bWireframe, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::CapsuleLogf");

		CapsuleLogfImpl(LogOwner, CategoryName, Verbosity, Base, HalfHeight, Radius, Rotation, Color, bWireframe, (const TCHAR*)Fmt, Args...);
	}

	// NavArea/Extruded convex log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use PulledConvexLogf")
	static void NavAreaShapeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const TArray<FVector>& ConvexPoints, float MinZ, float MaxZ, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::NavAreaShapeLogf");

		PulledConvexLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, ConvexPoints, MinZ, MaxZ, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use PulledConvexLogf")
	static void NavAreaShapeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& ConvexPoints, float MinZ, float MaxZ, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::NavAreaShapeLogf");

		PulledConvexLogfImpl(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void PulledConvexLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const TArray<FVector>& ConvexPoints, float MinZ, float MaxZ, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::PulledConvexLogf");

		PulledConvexLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, ConvexPoints, MinZ, MaxZ, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void PulledConvexLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& ConvexPoints, float MinZ, float MaxZ, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::PulledConvexLogf");

		PulledConvexLogfImpl(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, (const TCHAR*)Fmt, Args...);
	}

	// 3d Mesh log
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use MeshLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		MeshLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Vertices, Indices, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use MeshLogf")
	static void GeometryShapeLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryShapeLogf");

		MeshLogfImpl(LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void MeshLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::MeshLogf");

		MeshLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Vertices, Indices, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void MeshLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::MeshLogf");

		MeshLogfImpl(LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, (const TCHAR*)Fmt, Args...);
	}

	// 2d Convex shape
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use ConvexLogf")
	static void GeometryConvexLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const TArray<FVector>& Points, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryConvexLogf");

		ConvexLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Points, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	UE_DEPRECATED(5.4, "Use ConvexLogf")
	static void GeometryConvexLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Points, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::GeometryConvexLogf");

		ConvexLogfImpl(LogOwner, CategoryName, Verbosity, Points, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void ConvexLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const TArray<FVector>& Points, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::ConvexLogf");

		ConvexLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, Points, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void ConvexLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Points, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::ConvexLogf");

		ConvexLogfImpl(LogOwner, CategoryName, Verbosity, Points, Color, (const TCHAR*)Fmt, Args...);
	}

	//Histogram data
	template <typename FmtType, typename... Types>
	static void HistogramDataLogf(const UObject* LogOwner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, FName GraphName, FName DataName, const FVector2D& Data, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::HistogramDataLogf");

		HistogramDataLogfImpl(LogOwner, Category.GetCategoryName(), Verbosity, GraphName, DataName, Data, Color, (const TCHAR*)Fmt, Args...);
	}
	template <typename FmtType, typename... Types>
	static void HistogramDataLogf(const UObject* LogOwner, const FName& CategoryName, ELogVerbosity::Type Verbosity, FName GraphName, FName DataName, const FVector2D& Data, const FColor& Color, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FVisualLogger::HistogramDataLogf");

		HistogramDataLogfImpl(LogOwner, CategoryName, Verbosity, GraphName, DataName, Data, Color, (const TCHAR*)Fmt, Args...);
	}

	// Navigation data debug snapshot
	static ENGINE_API void NavigationDataDump(const UObject* LogOwner, const FLogCategoryBase& Category, const ELogVerbosity::Type Verbosity, const FBox& Box);
	static ENGINE_API void NavigationDataDump(const UObject* LogOwner, const FName& CategoryName, const ELogVerbosity::Type Verbosity, const FBox& Box);

	DECLARE_MULTICAST_DELEGATE_SixParams(FNavigationDataDump, const UObject* /*Object*/, const FName& /*CategoryName*/, const ELogVerbosity::Type /*Verbosity*/, const FBox& /*Box*/, const UWorld& /*World*/, FVisualLogEntry& /*CurrentEntry*/);
	static ENGINE_API FNavigationDataDump NavigationDataDumpDelegate;

	/** Log events */
	static ENGINE_API void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FName EventTag2 = NAME_None, const FName EventTag3 = NAME_None, const FName EventTag4 = NAME_None, const FName EventTag5 = NAME_None, const FName EventTag6 = NAME_None);
	static ENGINE_API void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2);
	static ENGINE_API void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3);
	static ENGINE_API void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4);
	static ENGINE_API void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5);
	static ENGINE_API void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5, const FVisualLogEventBase& Event6);
	
	static ENGINE_API void EventLog(const UObject* LogOwner, const FVisualLogEventBase& Event1, const FName EventTag1 = NAME_None, const FName EventTag2 = NAME_None, const FName EventTag3 = NAME_None, const FName EventTag4 = NAME_None, const FName EventTag5 = NAME_None, const FName EventTag6 = NAME_None);

	// static getter
	static ENGINE_API FVisualLogger& Get();

	virtual ~FVisualLogger() {}

	UE_DEPRECATED(5.4, "Following the base class convention and using the name TearDown. Since FVisualLogger::Get() is used internally everywhere, this class isn't designed to be inherited.")
	ENGINE_API virtual void Shutdown() {}

	// called on engine shutdown to flush all, etc.
	ENGINE_API virtual void TearDown() override;

	// Removes all logged data 
	ENGINE_API void Cleanup(UWorld* OldWorld, bool bReleaseMemory = false);

	// Use when a visual logger device has discarded all of its data, waiting for new data
	ENGINE_API void OnDataReset();

	/** Set log owner redirection from one object to another, to combine logs */
	static void Redirect(const UObject* FromObject, const UObject* ToObject)
	{ 
		FVisualLogger& Logger = FVisualLogger::Get();
		UObject* NewRedirection = nullptr;
		{
			FWriteScopeLock Lock(Logger.RedirectRWLock);
			NewRedirection = Logger.RedirectInternal(FromObject, ToObject);
		}
		UE_CVLOG(FromObject != nullptr && NewRedirection != nullptr, FromObject, LogVisual, Log, TEXT("Redirected '%s' to '%s'"), *FromObject->GetName(), *NewRedirection->GetName());
	}

	/** find and return redirection object for given object*/
	static UObject* FindRedirection(const UObject* Object)
	{ 
		FVisualLogger& Logger = FVisualLogger::Get();
		FReadScopeLock Lock(Logger.RedirectRWLock);
		return Logger.FindRedirectionInternal(Object);
	}

	/** blocks all categories from logging. It can be bypassed with the category allow list */
	void BlockAllCategories(const bool bInBlock) { bBlockedAllCategories = bInBlock; }

	/** checks if all categories are blocked */
	bool IsBlockedForAllCategories() const { return !!bBlockedAllCategories; }

	/** Returns category allow list for logging */
	const TArray<FName>& GetCategoryAllowList() const { return CategoryAllowList; }

	bool IsCategoryAllowed(const FName& Name) const { return CategoryAllowList.Find(Name) != INDEX_NONE; }

	void AddCategoryToAllowList(FName Category) { CategoryAllowList.AddUnique(Category); }

	void ClearCategoryAllowList() { CategoryAllowList.Reset(); }

	/** Generates and returns Id unique for given timestamp - used to connect different logs between (ex. text log with geometry shape) */
	ENGINE_API int32 GetUniqueId(double Timestamp);

	/** Starts visual log collecting and recording */
	ENGINE_API void SetIsRecording(const bool bInIsRecording);
	/** return information is vlog recording is enabled or not */
	FORCEINLINE static bool IsRecording() { return !!bIsRecording; }

	/** Starts visual log collecting and recording */
	ENGINE_API void SetIsRecordingToFile(bool InIsRecording);
	/** return information is vlog recording is enabled or not */
	bool IsRecordingToFile() const { return !!bIsRecordingToFile; }
	/** disables recording to file and discards all data without saving it to file */
	ENGINE_API void DiscardRecordingToFile();

	/** Starts visual log collecting and recording to insights traces (for Rewind Debugger)*/
	ENGINE_API void SetIsRecordingToTrace(const bool bInIsRecording);

	void SetIsRecordingOnServer(const bool bInIsRecording) { bIsRecordingOnServer = bInIsRecording; }
	bool IsRecordingOnServer() const { return !!bIsRecordingOnServer; }

	/** Configure whether VisLog should be using decorated, unique names */
	ENGINE_API void SetUseUniqueNames(const bool bEnable);

	/** Add visual logger output device */
	void AddDevice(FVisualLogDevice* InDevice) { OutputDevices.AddUnique(InDevice); }
	/** Remove visual logger output device */
	void RemoveDevice(FVisualLogDevice* InDevice) { OutputDevices.RemoveSwap(InDevice); }
	/** Remove visual logger output device */
	const TArray<FVisualLogDevice*>& GetDevices() const { return OutputDevices; }
	/** Check if log category can be recorded, verify before using GetEntryToWrite! */
	ENGINE_API bool IsCategoryLogged(const FLogCategoryBase& Category) const;
	/** Returns  current entry for given TimeStamp or creates another one  but first it serialize previous 
	 *	entry as completed to vislog devices. Use VisualLogger::DontCreate to get current entry without serialization
	 *	@note this function can return null */
	UE_DEPRECATED_FORGAME(5.4, "Use the static GetEntryToWrite instead because this TimeStamp is inconsistent across multiple instances (or threads in Editor).  This function will be made private/protected.")
	ENGINE_API FVisualLogEntry* GetEntryToWrite(const UObject* Object, double TimeStamp, ECreateIfNeeded ShouldCreate = ECreateIfNeeded::Create);
	/** Returns  the current (or new) entry for the given object; alternatively nullptr if we aren't allowed to vlog with the given parameters.
	 * @param LogOwner - The UObject (typically an AActor) we are going to write log entries about.  This becomes the row in the Visual Logger timeline.
	 * @param LogCategory - The LogCategory we are logging about.  This function will only return a valid log entry if the visual logging is enabled for the category.
	 */
	[[nodiscard]] static ENGINE_API FVisualLogEntry* GetEntryToWrite(const UObject* LogOwner, const FLogCategoryBase& LogCategory);
	/** Retrieves last used entry for given UObject
	 *	@note this function can return null */
	ENGINE_API FVisualLogEntry* GetLastEntryForObject(const UObject* Object);
	/** flush and serialize data if timestamp allows it */
	ENGINE_API virtual void Flush() override;
	/** Moves all threads entries into the global entry map */
	ENGINE_API void FlushThreadsEntries();

	/** FileName getter to set project specific file name for vlogs - highly encouraged to use FVisualLogFilenameGetterDelegate::CreateUObject with this */
	void SetLogFileNameGetter(const FVisualLogFilenameGetterDelegate& InLogFileNameGetter) { LogFileNameGetter = InLogFileNameGetter; }

	/** Register extension to use by LogVisualizer  */
	void RegisterExtension(FName TagName, FVisualLogExtensionInterface* ExtensionInterface) { check(AllExtensions.Contains(TagName) == false); AllExtensions.Add(TagName, ExtensionInterface); }
	/**  Removes previously registered extension */
	void UnregisterExtension(FName TagName, FVisualLogExtensionInterface* ExtensionInterface) { AllExtensions.Remove(TagName); }
	/** returns extension identified by given tag */
	FVisualLogExtensionInterface* GetExtensionForTag(const FName TagName) const { return AllExtensions.Contains(TagName) ? AllExtensions[TagName] : nullptr; }
	/** Returns reference to map with all registered extension */
	const TMap<FName, FVisualLogExtensionInterface*>& GetAllExtensions() const { return AllExtensions; }

	/** internal check for each usage of visual logger */
	static ENGINE_API bool CheckVisualLogInputInternal(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, UWorld **OutWorld, FVisualLogEntry **OutCurrentEntry);
	
	/** Returns a current time stamp to associate with a recorded event that occurred on Object (used for ordering events on a timeline) */
	ENGINE_API double GetTimeStampForObject(const UObject* Object) const;

	/** Sets function to call to get a timestamp instead of the default implementation (e.g. using a network synchronized clock instead of the local world time) */
	ENGINE_API void SetGetTimeStampFunc(TFunction<double(const UObject*)> Function);

	typedef TMap<FObjectKey, TArray<TWeakObjectPtr<const UObject> > > FOwnerToChildrenRedirectionMap;
	static ENGINE_API FOwnerToChildrenRedirectionMap& GetRedirectionMap(const UObject* InObject);

	typedef TMap<FObjectKey, TWeakObjectPtr<const UObject> > FChildToOwnerRedirectionMap;
	FChildToOwnerRedirectionMap& GetChildToOwnerRedirectionMap() { return ChildToOwnerMap; }

	typedef TMap<FObjectKey, TWeakObjectPtr<const UWorld> > FObjectToWorldMapType;
	FObjectToWorldMapType& GetObjectToWorldMap() { return ObjectToWorldMap; }

	ENGINE_API void AddClassToAllowList(UClass& InClass);
	ENGINE_API bool IsClassAllowed(const UClass& InClass) const;

	ENGINE_API void AddObjectToAllowList(const UObject& InObject);
	ENGINE_API void ClearObjectAllowList();
	ENGINE_API bool IsObjectAllowed(const UObject* InObject) const;

	UE_DEPRECATED(5.0, "Use AddCategoryToAllowList instead")
	void AddCategoryToWhitelist(FName Category) { AddCategoryToAllowList(Category); }
	
	UE_DEPRECATED(5.0, "Use ClearCategoryAllowList instead")
	void ClearWhiteList() { ClearCategoryAllowList(); }

	UE_DEPRECATED(5.0, "Use AddObjectToAllowList instead")
	void AddWhitelistedClass(UClass& InClass) { AddClassToAllowList(InClass); }

	UE_DEPRECATED(5.0, "Use AddObjectToAllowList instead")
	void AddWhitelistedObject(const UObject& InObject) { AddObjectToAllowList(InObject); }

	typedef TMap<FObjectKey, FVisualLogEntry> FVisualLoggerObjectEntryMap;

private:
	ENGINE_API FVisualLogger();
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override { ensureMsgf(0, TEXT("Regular serialize is forbiden for visual logs")); }

	ENGINE_API FVisualLoggerObjectEntryMap& GetThreadCurrentEntryMap();

	/** Get global entry to write where all logs are combined together, not thread safe */
	ENGINE_API FVisualLogEntry* GetEntryToWriteInternal(const UObject* Object, double TimeStamp, ECreateIfNeeded ShouldCreate);

	/** Redirect internal implementation, not thread safe */
	ENGINE_API UObject* RedirectInternal(const UObject* FromObject, const UObject* ToObject);
	/** Find redirects internal implementation, not thread safe */
	ENGINE_API UObject* FindRedirectionInternal(const UObject* Object) const;
	/** Cleanup invalid redirects */
	ENGINE_API void CleanupRedirects();
	/** Figure out all conditions if this entry is allowed to log */
	ENGINE_API void CalculateEntryAllowLogging(FVisualLogEntry* CurrentEntry, const UObject* LogOwner, const UObject* Object);

protected:
	/** Flushes entries recorded in the frame */
	ENGINE_API void Tick(float DeltaTime);

	/**
	 * Serializes a single entry and resets it.
	 * Method expects an initialized entry and will ensure otherwise. 
	 */
	ENGINE_API void FlushEntry(FVisualLogEntry& Entry, const FObjectKey& ObjectKey);

	/** Handle to the registered ticker to flush entries */
	FTSTicker::FDelegateHandle TickerHandle;
	
	/** Array of output devices to redirect to */
	TArray<FVisualLogDevice*> OutputDevices;
	// Map for inter-objects redirections
	static ENGINE_API TMap<const UWorld*, FOwnerToChildrenRedirectionMap> WorldToRedirectionMap;

	// allowed classes - only instances of these classes will be logged. 
	// if ClassAllowList is empty (default) everything will log
	TArray<UClass*> ClassAllowList;

	// allowed objects - takes priority over class allow list and should be used to create exceptions in it
	// if ObjectAllowList is empty (default) everything will log
	// do NOT read from those pointers, they can be invalid!
	TSet<FObjectKey> ObjectAllowList;

	// list of categories that are still allowed to be logged when logging is blocking
	TArray<FName> CategoryAllowList;

	// Visual Logger extensions map
	TMap<FName, FVisualLogExtensionInterface*> AllExtensions;
	// last generated unique id for given times tamp
	TMap<double, int32> LastUniqueIds;
	// Current entry with all data
	FVisualLoggerObjectEntryMap CurrentEntryPerObject;
	// Threads current entry maps
	TArray<FVisualLoggerObjectEntryMap*> ThreadCurrentEntryMaps;
	// Read Write lock protecting object entries
	mutable FRWLock EntryRWLock;
	// Map to contain names for Objects (they can be destroyed after while)
	TMap<FObjectKey, FName> ObjectToNameMap;
	// Map to contain class names for Objects (they can be destroyed after while)
	TMap<FObjectKey, FName> ObjectToClassNameMap;
	// Cached map to world information because it's just raw pointer and not real object
	FObjectToWorldMapType ObjectToWorldMap;
	// for any object that has requested redirection this map holds where we should
	// redirect the traffic to
	FChildToOwnerRedirectionMap ChildToOwnerMap;
	// Read Write lock protecting redirection maps (ChildToOwnerMap and ObjectToWorldMap)
	mutable FRWLock RedirectRWLock;
	// if set all categories are blocked from logging
	bool bBlockedAllCategories : 1;
	// if set we are recording to file
	bool bIsRecordingToFile : 1;
	// if set we are recording to insights trace 
	bool bIsRecordingToTrace : 1;
	// variable set (from cheat manager) when logging is active on server
	bool bIsRecordingOnServer : 1;
	// controls how we generate log names. When set to TRUE there's a lower 
	// chance of name conflict, but it's more expensive
	bool bForceUniqueLogNames : 1;
	/** Indicates that entries were added/updated and that a flush is required */
	bool bIsFlushRequired : 1;
	/** Indicates there are entries in the redirection map that are invalid */
	mutable bool bContainsInvalidRedirects : 1;
	// start recording time
	double StartRecordingToFileTime;
	/** Delegate to set project specific file name for vlogs */
	FVisualLogFilenameGetterDelegate LogFileNameGetter;

	/** Function to call when recording the absolute time stamp of an event. Useful for manually aligning events across multiple instances (e.g. using FPlatformTime::Seconds() rather than WorldTime) */
	TFunction<double(const UObject*)> GetTimeStampFunc;

#if WITH_EDITOR
	/** Handle for registering with PIEStarted to reset the EditorBaseTimeStamp */
	FDelegateHandle PIEStartedHandle;
#endif

	// if set we are recording and collecting all vlog data
	static ENGINE_API int32 bIsRecording;
};

#endif //ENABLE_VISUAL_LOG
