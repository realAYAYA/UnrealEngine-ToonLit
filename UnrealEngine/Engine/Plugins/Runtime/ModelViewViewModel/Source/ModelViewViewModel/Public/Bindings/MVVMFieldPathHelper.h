// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FieldNotification/FieldId.h"
#include "Templates/SubclassOf.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMFieldContext.h"
#include "UObject/FieldPath.h"
#include "UObject/Object.h"


namespace UE::MVVM::FieldPathHelper
{
	/**
	 * Info about the viewmodel and field.
	 */
	struct FParsedBindingInfo
	{
		using FFieldVariantArray = TArray<FMVVMConstFieldVariant, TInlineAllocator<4>>;

		/**
		 * The viewmodel that is used to create the binding. When it's invalid the binding can be valid but only OneTime mode is supported.
		 * @return the class of "ViewmodelB" when the path is "ViewmodelA.ViewmodelB.Vector.X"
		 */
		TSubclassOf<UObject> NotifyFieldClass;

		/**
		 * The property we will bind to. When it's invalid the binding can be valid but only OneTime mode is supported.
		 * @return "Vector" when the path is "ViewmodelA.ViewmodelB.Vector.X"
		 */
		FFieldNotificationId NotifyFieldId;

		/**
		 * The property path to get the viewmodel at runtime.
		 * @return "ViewmodelA.ViewmodelB" or "ViewmodelA.GetViewmodelB" when the path is "ViewmodelA.ViewmodelB.Vector.X"
		 */
		FFieldVariantArray NotifyFieldInterfacePath;
		/**
		 * The viewmodel index in the original path.
		 * @return 1 when the path is "ViewmodelA.ViewmodelB.Vector.X"
		 */
		int32 ViewModelIndex = INDEX_NONE;
	};

	/** */
	UE_NODISCARD MODELVIEWVIEWMODEL_API TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GenerateFieldPathList(const UClass* From, FStringView FieldPath, bool bForSourceBinding);

	/** */
	UE_NODISCARD MODELVIEWVIEWMODEL_API TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GenerateFieldPathList(TArrayView<const FMVVMConstFieldVariant> Fields, bool bForSourceBinding);

	/** */
	UE_NODISCARD MODELVIEWVIEWMODEL_API TValueOrError<FParsedBindingInfo, FText> GetBindingInfoFromFieldPath(const UClass* Accessor, TArrayView<const FMVVMConstFieldVariant> Fields);

	/** */
	UE_NODISCARD MODELVIEWVIEWMODEL_API FString ToString(TArrayView<const FMVVMFieldVariant> Fields);
	UE_NODISCARD MODELVIEWVIEWMODEL_API FString ToString(TArrayView<const FMVVMConstFieldVariant> Fields);
	UE_NODISCARD MODELVIEWVIEWMODEL_API FText ToText(TArrayView<const FMVVMFieldVariant> Fields);
	UE_NODISCARD MODELVIEWVIEWMODEL_API FText ToText(TArrayView<const FMVVMConstFieldVariant> Fields);

	/** */
	UE_NODISCARD MODELVIEWVIEWMODEL_API TValueOrError<UObject*, void> EvaluateObjectProperty(const FFieldContext& InSource);

} // namespace
