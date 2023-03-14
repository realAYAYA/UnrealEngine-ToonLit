// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneWidgetMaterialSystem.h"
#include "Animation/MovieSceneUMGComponentTypes.h"
#include "Animation/WidgetMaterialTrackUtilities.h"

#include "EntitySystem/MovieSceneEntityMutations.h"

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

#include "Systems/FloatChannelEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"

#include "Materials/MaterialInstanceDynamic.h"

#include "Components/Widget.h"

#include "String/Join.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneWidgetMaterialSystem)

namespace UE::MovieScene
{

FWidgetMaterialAccessor::FWidgetMaterialAccessor(const FWidgetMaterialKey& InKey)
	: Widget(CastChecked<UWidget>(InKey.Object.ResolveObjectPtr()))
	, WidgetMaterialPath(InKey.WidgetMaterialPath)
{}

FWidgetMaterialAccessor::FWidgetMaterialAccessor(UObject* InObject, FWidgetMaterialPath InWidgetMaterialPath)
	: Widget(Cast<UWidget>(InObject))
	, WidgetMaterialPath(MoveTemp(InWidgetMaterialPath))
{
	// Object must be a widget
	check(!InObject || Widget);
}

FString FWidgetMaterialAccessor::ToString() const
{
	return FString::Printf(TEXT("Brush %s.%s"), *Widget->GetPathName(), *FString::JoinBy(WidgetMaterialPath.Path, TEXT("."), UE_PROJECTION_MEMBER(FName, ToString)));
}

UMaterialInterface* FWidgetMaterialAccessor::GetMaterial() const
{
	FWidgetMaterialHandle Handle = WidgetMaterialTrackUtilities::GetMaterialHandle(Widget, WidgetMaterialPath.Path);
	if (Handle.IsValid())
	{
		return Handle.GetMaterial();
	}

	return nullptr;
}

void FWidgetMaterialAccessor::SetMaterial(UMaterialInterface* InMaterial) const
{
	FWidgetMaterialHandle Handle = WidgetMaterialTrackUtilities::GetMaterialHandle(Widget, WidgetMaterialPath.Path);
	if (Handle.IsValid())
	{
		Handle.SetMaterial(InMaterial, Widget);
	}
}

UMaterialInstanceDynamic* FWidgetMaterialAccessor::CreateDynamicMaterial(UMaterialInterface* InMaterial)
{
	// Need to create a new MID, either because the parent has changed, or because one doesn't already exist
	TStringBuilder<128> DynamicName;
	InMaterial->GetFName().ToString(DynamicName);
	DynamicName.Append(TEXT("_Animated"));
	FName UniqueDynamicName = MakeUniqueObjectName(Widget, UMaterialInstanceDynamic::StaticClass() , DynamicName.ToString());

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(InMaterial, Widget, UniqueDynamicName);
	SetMaterial(MID);
	return MID;
}

TAutoRegisterPreAnimatedStorageID<FPreAnimatedWidgetMaterialSwitcherStorage> FPreAnimatedWidgetMaterialSwitcherStorage::StorageID;
TAutoRegisterPreAnimatedStorageID<FPreAnimatedWidgetMaterialParameterStorage> FPreAnimatedWidgetMaterialParameterStorage::StorageID;

} // namespace UE::MovieScene

UMovieSceneWidgetMaterialSystem::UMovieSceneWidgetMaterialSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneUMGComponentTypes*    WidgetComponents  = FMovieSceneUMGComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	RelevantComponent = WidgetComponents->WidgetMaterialPath;
	Phase = ESystemPhase::Instantiation | ESystemPhase::Evaluation;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), BuiltInComponents->ObjectResult);
		DefineComponentConsumer(GetClass(), BuiltInComponents->BoundObject);
		DefineComponentProducer(GetClass(), TracksComponents->BoundMaterial);
		DefineImplicitPrerequisite(UMovieSceneCachePreAnimatedStateSystem::StaticClass(), GetClass());
	}
}

void UMovieSceneWidgetMaterialSystem::OnLink()
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*       BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneUMGComponentTypes* WidgetComponents  = FMovieSceneUMGComponentTypes::Get();

	SystemImpl.MaterialSwitcherStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedWidgetMaterialSwitcherStorage>();
	SystemImpl.MaterialParameterStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedWidgetMaterialParameterStorage>();

	SystemImpl.OnLink(Linker, BuiltInComponents->BoundObject, WidgetComponents->WidgetMaterialPath);
}

void UMovieSceneWidgetMaterialSystem::OnUnlink()
{
	SystemImpl.OnUnlink(Linker);
}

void UMovieSceneWidgetMaterialSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*       BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneUMGComponentTypes* WidgetComponents  = FMovieSceneUMGComponentTypes::Get();

	SystemImpl.OnRun(Linker, BuiltInComponents->BoundObject, WidgetComponents->WidgetMaterialPath, InPrerequisites, Subsequents);
}

void UMovieSceneWidgetMaterialSystem::SavePreAnimatedState(const FPreAnimationParameters& InParameters)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*       BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneUMGComponentTypes* WidgetComponents  = FMovieSceneUMGComponentTypes::Get();

	SystemImpl.SavePreAnimatedState(Linker, BuiltInComponents->BoundObject, WidgetComponents->WidgetMaterialPath, InParameters);
}
