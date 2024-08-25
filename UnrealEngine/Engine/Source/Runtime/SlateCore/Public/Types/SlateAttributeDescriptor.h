// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "Templates/Identity.h"

#include <type_traits>

class FSlateWidgetClassData;
class SWidget;

namespace SlateAttributePrivate
{
	enum class ESlateAttributeType : uint8;
}


/**
 * Describes the static information about a Widget's type SlateAttributes.
 **/
class FSlateAttributeDescriptor
{
public:
	/**
	 * A EInvalidationWidgetReason Attribute 
	 * It can be explicitly initialize or can be a callback static function or lambda that returns the EInvalidationReason.
	 * The signature of the function takes a const SWidget& as argument.
	 */
	struct FInvalidateWidgetReasonAttribute
	{
		friend FSlateAttributeDescriptor;

		using Arg1Type = const class SWidget&;
		// using "not checked" delegate to disable race detection. This delegate is used in a static (FSlateWidgetClassData) and will be destructed after the tls array used in the race detection code (which will result in a use after free).
		using FGetter = TDelegate<EInvalidateWidgetReason(Arg1Type), FNotThreadSafeNotCheckedDelegateUserPolicy>;


		FInvalidateWidgetReasonAttribute(const FInvalidateWidgetReasonAttribute&) = default;
		FInvalidateWidgetReasonAttribute(FInvalidateWidgetReasonAttribute&&) = default;
		FInvalidateWidgetReasonAttribute& operator=(const FInvalidateWidgetReasonAttribute&) = default;
		FInvalidateWidgetReasonAttribute& operator=(FInvalidateWidgetReasonAttribute&&) = default;

		/** Default constructor. */
		explicit FInvalidateWidgetReasonAttribute(EInvalidateWidgetReason InReason)
			: Reason(InReason)
			, Getter()
		{
		}

		template<typename... PayloadTypes>
		explicit FInvalidateWidgetReasonAttribute(TIdentity_T<typename FGetter::template TFuncPtr<PayloadTypes...>> InFuncPtr, PayloadTypes&&... InputPayload)
			: Reason(EInvalidateWidgetReason::None)
			, Getter(FGetter::CreateStatic(InFuncPtr, Forward<PayloadTypes>(InputPayload)...))
		{
		}

		template<typename LambdaType, typename... PayloadTypes>
		explicit FInvalidateWidgetReasonAttribute(LambdaType&& InCallable, PayloadTypes&&... InputPayload)
			: Reason(EInvalidateWidgetReason::None)
			, Getter(FGetter::CreateLambda(InCallable, Forward<PayloadTypes>(InputPayload)...))
		{
		}

		bool IsBound() const
		{
			return Getter.IsBound();
		}

		EInvalidateWidgetReason Get(const SWidget& Widget) const
		{
			return IsBound() ? Getter.Execute(Widget) : Reason;
		}

	private:
		EInvalidateWidgetReason Reason;
		FGetter Getter;
	};

	// using "not checked" delegate to disable race detection. This delegate is used in a static (FSlateWidgetClassData) and will be destructed after the tls array used in the race detection code (which will result in a use after free).
	using FAttributeValueChangedDelegate = TDelegate<void(SWidget&), FNotThreadSafeNotCheckedDelegateUserPolicy>;

	/** */
	enum class ECallbackOverrideType
	{
		/** Replace the callback that the base class defined. */
		ReplacePrevious,
		/** Execute the callback that the base class defined, then execute the new callback. */
		ExecuteAfterPrevious,
		/** Execute the new callback, then execute the callback that the base class defined. */
		ExecuteBeforePrevious,
	};

public:
	struct FAttribute;
	struct FContainer;
	struct FContainerInitializer;
	struct FInitializer;

	using OffsetType = uint32;

	/** The default sort order that define in which order attributes will be updated. */
	static constexpr OffsetType DefaultSortOrder(OffsetType Offset) { return Offset * 100; }


	/** */
	struct FContainer
	{
		friend FInitializer;

	public:
		FContainer() = default;
		FContainer(FName InName, OffsetType InOffset)
			: Name(InName)
			, Offset(InOffset)
		{}

		bool IsValid() const
		{
			return !Name.IsNone();
		}

		FName GetName() const
		{
			return Name;
		}

		uint32 GetSortOrder() const
		{
			return SortOrder;
		}

	private:
		FName Name;
		OffsetType Offset = 0;
		uint32 SortOrder = 0;
	};


	/** */
	struct FAttribute
	{
		friend FSlateAttributeDescriptor;
		friend FInitializer;
		friend FContainerInitializer;

	public:
		FAttribute(FName Name, OffsetType Offset, FInvalidateWidgetReasonAttribute Reason);
		FAttribute(FName ContainerName, FName Name, OffsetType Offset, FInvalidateWidgetReasonAttribute Reason);

		FName GetName() const
		{
			return Name;
		}

		uint32 GetSortOrder() const
		{
			return SortOrder;
		}

		EInvalidateWidgetReason GetInvalidationReason(const SWidget& Widget) const
		{
			return InvalidationReason.Get(Widget);
		}

		SlateAttributePrivate::ESlateAttributeType GetAttributeType() const
		{
			return AttributeType;
		}

		bool DoesAffectVisibility() const
		{
			return bAffectVisibility;
		}

		void ExecuteOnValueChangedIfBound(SWidget& Widget) const
		{
			OnValueChanged.ExecuteIfBound(Widget);
		}

	private:
		FName Name;
		OffsetType Offset;
		FName Prerequisite;
		FName ContainerName; // if the container IsNone, then the container is the SWidget itself.
		uint32 SortOrder;
		uint8 ContainerIndex;
		FInvalidateWidgetReasonAttribute InvalidationReason;
		FAttributeValueChangedDelegate OnValueChanged;
		SlateAttributePrivate::ESlateAttributeType AttributeType;
		bool bAffectVisibility;
	};

	/** Internal class to initialize the SlateAttributeDescriptor::FContainer attributes (Add attributes or modify existing attributes). */
	struct FContainerInitializer
	{
	private:
		friend FSlateAttributeDescriptor;
		SLATECORE_API FContainerInitializer(FSlateAttributeDescriptor& InDescriptor, FName ContainerName);
		SLATECORE_API FContainerInitializer(FSlateAttributeDescriptor& InDescriptor, const FSlateAttributeDescriptor& ParentDescriptor, FName ContainerName);

	public:
		FContainerInitializer() = delete;
		FContainerInitializer(const FContainerInitializer&) = delete;
		FContainerInitializer& operator= (const FContainerInitializer&) = delete;

		struct FAttributeEntry
		{
			SLATECORE_API FAttributeEntry(FSlateAttributeDescriptor& Descriptor, FName ContainerName, int32 AttributeIndex);

			/**
			 * Update the attribute after the prerequisite.
			 * The order is guaranteed but other attributes may be updated in between.
			 * No order is guaranteed if the prerequisite or this property is updated manually.
			 */
			SLATECORE_API FAttributeEntry& UpdatePrerequisite(FName Prerequisite);

			/**
			 * Notified when the attribute value changed.
			 * It's preferable that you delay any action to the Tick or Paint function.
			 * You are not allowed to make changes that would affect the SWidget ChildOrder or its Visibility.
			 * It will not be called when the SWidget is in its construction phase.
			 * @see SWidget::IsConstructed
			 */
			SLATECORE_API FAttributeEntry& OnValueChanged(FAttributeValueChangedDelegate Callback);

		private:
			FSlateAttributeDescriptor& Descriptor;
			FName ContainerName;
			int32 AttributeIndex;
		};

		SLATECORE_API FAttributeEntry AddContainedAttribute(FName AttributeName, OffsetType Offset, const FInvalidateWidgetReasonAttribute& ReasonGetter);
		SLATECORE_API FAttributeEntry AddContainedAttribute(FName AttributeName, OffsetType Offset, FInvalidateWidgetReasonAttribute&& ReasonGetter);

	public:
		/** Change the InvalidationReason of an attribute defined in a base class. */
		SLATECORE_API void OverrideInvalidationReason(FName AttributeName, const FInvalidateWidgetReasonAttribute& Reason);
		/** Change the InvalidationReason of an attribute defined in a base class. */
		SLATECORE_API void OverrideInvalidationReason(FName AttributeName, FInvalidateWidgetReasonAttribute&& Reason);

		/** Change the FAttributeValueChangedDelegate of an attribute defined in a base class. */
		SLATECORE_API void OverrideOnValueChanged(FName AttributeName, ECallbackOverrideType OverrideType, FAttributeValueChangedDelegate Callback);

	private:
		FSlateAttributeDescriptor& Descriptor;
		FName ContainerName;
	};

	/** Internal class to initialize the SlateAttributeDescriptor (Add attributes or modify existing attributes). */
	struct FInitializer
	{
	private:
		friend FSlateWidgetClassData;
		SLATECORE_API FInitializer(FSlateAttributeDescriptor& InDescriptor);
		SLATECORE_API FInitializer(FSlateAttributeDescriptor& InDescriptor, const FSlateAttributeDescriptor& ParentDescriptor);
		FInitializer(const FInitializer&) = delete;
		FInitializer& operator=(const FInitializer&) = delete;

	public:
		SLATECORE_API ~FInitializer();

		struct FAttributeEntry
		{
			SLATECORE_API FAttributeEntry(FSlateAttributeDescriptor& Descriptor, int32 InAttributeIndex);

			/**
			 * Update the attribute after the prerequisite.
			 * The order is guaranteed but other attributes may be updated in between.
			 * No order is guaranteed if the prerequisite or this property is updated manually.
			 */
			SLATECORE_API FAttributeEntry& UpdatePrerequisite(FName Prerequisite);

			/**
			 * The attribute affect the visibility of the widget.
			 * We only update the attributes that can change the visibility of the widget when the widget is collapsed.
			 * Attributes that affect visibility must have the Visibility attribute as a Prerequisite or the Visibility attribute must have it as a Prerequisite.
			 */
			SLATECORE_API FAttributeEntry& AffectVisibility();

			/**
			 * Notified when the attribute value changed.
			 * It's preferable that you delay any action to the Tick or Paint function.
			 * You are not allowed to make changes that would affect the SWidget ChildOrder or its Visibility.
			 * It will not be called when the SWidget is in its construction phase.
			 * @see SWidget::IsConstructed
			 */
			SLATECORE_API FAttributeEntry& OnValueChanged(FAttributeValueChangedDelegate Callback);

		private:
			FSlateAttributeDescriptor& Descriptor;
			int32 AttributeIndex;
		};

		SLATECORE_API FAttributeEntry AddMemberAttribute(FName AttributeName, OffsetType Offset, const FInvalidateWidgetReasonAttribute& ReasonGetter);
		SLATECORE_API FAttributeEntry AddMemberAttribute(FName AttributeName, OffsetType Offset, FInvalidateWidgetReasonAttribute&& ReasonGetter);

		SLATECORE_API FContainerInitializer AddContainer(FName ContainerName, OffsetType Offset);

	public:
		/** Change the InvalidationReason of an attribute defined in a base class. */
		SLATECORE_API void OverrideInvalidationReason(FName AttributeName, const FInvalidateWidgetReasonAttribute& Reason);
		/** Change the InvalidationReason of an attribute defined in a base class. */
		SLATECORE_API void OverrideInvalidationReason(FName AttributeName, FInvalidateWidgetReasonAttribute&& Reason);

		/** Change the FAttributeValueChangedDelegate of an attribute defined in a base class. */
		SLATECORE_API void OverrideOnValueChanged(FName AttributeName, ECallbackOverrideType OverrideType, FAttributeValueChangedDelegate Callback);

		/** Change the update type of an attribute defined in a base class. */
		SLATECORE_API void SetAffectVisibility(FName AttributeName, bool bAffectVisibility);

	private:
		FSlateAttributeDescriptor& Descriptor;
	};

	/** @returns the number of Attributes registered. */
	int32 GetAttributeNum() const
	{
		return Attributes.Num();
	}

	/** @returns the Attribute at the index previously found with IndexOfMemberAttribute */
	SLATECORE_API const FAttribute& GetAttributeAtIndex(int32 Index) const;

	/** @returns the Container with the corresponding name. */
	SLATECORE_API const FContainer* FindContainer(FName ContainerName) const;

	/** @returns the Attribute with the corresponding name. */
	SLATECORE_API const FAttribute* FindAttribute(FName AttributeName) const;

	/** @returns the Attribute of a SlateAttribute that have the corresponding memory offset. */
	SLATECORE_API const FAttribute* FindMemberAttribute(OffsetType AttributeOffset) const;

	/** @returns the Attribute of a SlateAttribute that have the corresponding memory offset. */
	SLATECORE_API const FAttribute* FindContainedAttribute(FName ContainerName, OffsetType AttributeOffset) const;

	/** @returns the index of the Container with the corresponding name. */
	SLATECORE_API int32 IndexOfContainer(FName AttributeName) const;

	/** @returns the index of a SlateAttribute that have the corresponding memory offset. */
	SLATECORE_API int32 IndexOfAttribute(FName AttributeName) const;

	/** @returns the index of a SlateAttribute that have the corresponding memory offset. */
	SLATECORE_API int32 IndexOfMemberAttribute(OffsetType AttributeOffset) const;

	/** @returns the index of a SlateAttribute that have the corresponding memory offset. */
	SLATECORE_API int32 IndexOfContainedAttribute(FName ContainerName, OffsetType AttributeOffset) const;

private:
	SLATECORE_API FAttribute* FindAttribute(FName AttributeName);

	SLATECORE_API FContainerInitializer AddContainer(FName AttributeName, OffsetType Offset);
	SLATECORE_API FInitializer::FAttributeEntry AddMemberAttribute(FName AttributeName, OffsetType Offset, FInvalidateWidgetReasonAttribute ReasonGetter);
	SLATECORE_API FContainerInitializer::FAttributeEntry AddContainedAttribute(FName ContainerName, FName AttributeName, OffsetType Offset, FInvalidateWidgetReasonAttribute ReasonGetter);
	SLATECORE_API void OverrideInvalidationReason(FName ContainerName, FName AttributeName, FInvalidateWidgetReasonAttribute ReasonGetter);
	SLATECORE_API void OverrideOnValueChanged(FName ContainerName, FName AttributeName, ECallbackOverrideType OverrideType, FAttributeValueChangedDelegate Callback);
	SLATECORE_API void SetPrerequisite(FName ContainerName, FAttribute& Attribute, FName Prerequisite);
	SLATECORE_API void SetAffectVisibility(FAttribute& Attribute, bool bUpdate);

private:
	TArray<FAttribute> Attributes;
	TArray<FContainer, TInlineAllocator<1>> Containers;
};

/**
 * Add a TSlateAttribute to the descriptor.
 * @param _Initializer The FSlateAttributeInitializer from the PrivateRegisterAttributes function.
 * @param _Property The TSlateAttribute property
 * @param _Reason The EInvalidationWidgetReason or a static function/lambda that takes a const SWidget& and that returns the invalidation reason.
 */
#define SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION_WITH_NAME(_Initializer, _Name, _Property, _Reason) \
		static_assert(decltype(_Property)::AttributeType == SlateAttributePrivate::ESlateAttributeType::Member, "The SlateProperty is not a TSlateAttribute. Do not use SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION"); \
		static_assert(!decltype(_Property)::HasDefinedInvalidationReason, "When implementing the SLATE_DECLARE_WIDGET pattern, use TSlateAttribute without the invalidation reason."); \
		static_assert(!std::is_same<decltype(_Reason), EInvalidateWidgetReason>::value || FSlateAttributeBase::IsInvalidateWidgetReasonSupported(_Reason), "The invalidation is not supported by the SlateAttribute system."); \
		_Initializer.AddMemberAttribute(_Name, STRUCT_OFFSET(PrivateThisType, _Property), FSlateAttributeDescriptor::FInvalidateWidgetReasonAttribute{_Reason})

#define SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(_Initializer, _Property, _Reason) \
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION_WITH_NAME(_Initializer, GET_MEMBER_NAME_CHECKED(PrivateThisType, _Property), _Property, _Reason)

#define SLATE_ADD_PANELCHILDREN_DEFINITION_WITH_NAME(_Initializer, _Name, _Container) \
		_Initializer.AddContainer(_Name, STRUCT_OFFSET(PrivateThisType, _Container))

#define SLATE_ADD_PANELCHILDREN_DEFINITION(_Initializer, _Container) \
		SLATE_ADD_PANELCHILDREN_DEFINITION_WITH_NAME(_Initializer, GET_MEMBER_NAME_CHECKED(PrivateThisType, _Container), _Container)

#define SLATE_ADD_SLOT_ATTRIBUTE_DEFINITION_WITH_NAME(_SlotType, _Initializer, _Name, _Property, _Reason) \
		static_assert(decltype(_Property)::AttributeType == SlateAttributePrivate::ESlateAttributeType::Contained, "The SlateProperty is not a TSlateAttribute. Do not use SLATE_ADD_CONTAINED_ATTRIBUTE_DEFINITION"); \
		static_assert(!decltype(_Property)::HasDefinedInvalidationReason, "When implementing the SLATE_DECLARE_WIDGET pattern, use TSlateSlotAttribute without the invalidation reason."); \
		static_assert(!std::is_same<decltype(_Reason), EInvalidateWidgetReason>::value || FSlateAttributeBase::IsInvalidateWidgetReasonSupported(_Reason), "The invalidation is not supported by the SlateAttribute system."); \
		/** We do not use STRUCT_OFFSET here. At runtime we use the ISlateAttributeContainer as the base pointer. FSlot uses multi-inheritance. */ \
		_Initializer.AddContainedAttribute(_Name, ((SIZE_T)(&(((_SlotType*)(0x1000))->_Property)) - (SIZE_T)(static_cast<SlateAttributePrivate::ISlateAttributeContainer*>((_SlotType*)(0x1000)))), FSlateAttributeDescriptor::FInvalidateWidgetReasonAttribute{_Reason})

#define SLATE_ADD_SLOT_ATTRIBUTE_DEFINITION(_SlotType, _Initializer, _Property, _Reason) \
	SLATE_ADD_SLOT_ATTRIBUTE_DEFINITION_WITH_NAME(_SlotType, _Initializer, GET_MEMBER_NAME_CHECKED(_SlotType, _Property), _Property, _Reason)

using FSlateAttributeInitializer = FSlateAttributeDescriptor::FInitializer;
using FSlateWidgetSlotAttributeInitializer = FSlateAttributeDescriptor::FContainerInitializer;
