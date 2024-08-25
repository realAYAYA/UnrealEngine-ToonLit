// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#error "Verse VM is still under developement and should not be used in a production environment."

#include "Containers/StringFwd.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "VerseVM/VVMOpResult.h"
#include <type_traits>

class FString;

namespace Verse
{
struct FAbstractVisitor;
struct FAllocationContext;
struct FCellFormatter;
struct FMarkStack;
struct FMarkStackVisitor;
struct FRunningContext;
struct VCell;
struct VValue;

// MSVC and clang-cl have a non-portable __super that can be used to validate the user's super-class declaration.
#if defined(_MSC_VER)
#define VCPPCLASSINFO_PORTABLE_MSC_SUPER         \
	/* disable non-standard extension warning */ \
	__pragma(warning(push)) __pragma(warning(disable : 4495)) __super __pragma(warning(pop))
#else
#define VCPPCLASSINFO_PORTABLE_MSC_SUPER Super
#endif

#define DECLARE_BASE_VCPPCLASSINFO(API)                             \
protected:                                                          \
	auto CheckSuperClass()                                          \
	{                                                               \
		return this;                                                \
	}                                                               \
                                                                    \
	template <typename TVisitor>                                    \
	void VisitInheritedAndNonInheritedReferences(TVisitor& Visitor) \
	{                                                               \
		VisitReferencesImpl(Visitor);                               \
	}                                                               \
                                                                    \
public:                                                             \
	API static ::Verse::VCppClassInfo StaticCppClassInfo;

#define DECLARE_DERIVED_VCPPCLASSINFO(API, SuperClass)                                  \
private:                                                                                \
	template <typename TVisitor>                                                        \
	void VisitReferencesImpl(TVisitor&); /* to be implemented by the user. */           \
                                                                                        \
protected:                                                                              \
	auto CheckSuperClass()                                                              \
	{                                                                                   \
		auto SuperThis = VCPPCLASSINFO_PORTABLE_MSC_SUPER::CheckSuperClass();           \
		static_assert(std::is_same_v<decltype(SuperThis), SuperClass*>,                 \
			"Declared super-class " #SuperClass " does not match actual super-class."); \
		return this;                                                                    \
	}                                                                                   \
                                                                                        \
	template <typename TVisitor>                                                        \
	void VisitInheritedAndNonInheritedReferences(TVisitor& Visitor)                     \
	{                                                                                   \
		Super::VisitInheritedAndNonInheritedReferences(Visitor);                        \
		VisitReferencesImpl(Visitor);                                                   \
	}                                                                                   \
                                                                                        \
public:                                                                                 \
	using Super = SuperClass;                                                           \
	API static ::Verse::VCppClassInfo StaticCppClassInfo;

#define DEFINE_BASE_OR_DERIVED_VCPPCLASSINFO(CellType, SuperClassInfoPtr)                                                                                                       \
	::Verse::VCppClassInfo CellType::StaticCppClassInfo = {                                                                                                                     \
		TEXT(#CellType),                                                                                                                                                        \
		(SuperClassInfoPtr),                                                                                                                                                    \
		[](::Verse::VCell* This, ::Verse::FMarkStackVisitor& Visitor) -> void {                                                                                                 \
			This->StaticCast<CellType>().VisitInheritedAndNonInheritedReferences(Visitor);                                                                                      \
		},                                                                                                                                                                      \
		[](::Verse::VCell* This, ::Verse::FAbstractVisitor& Visitor) -> void {                                                                                                  \
			::Verse::FAbstractVisitor::FReferrerContext Context(Visitor, This);                                                                                                 \
			This->StaticCast<CellType>().VisitInheritedAndNonInheritedReferences(Visitor);                                                                                      \
		},                                                                                                                                                                      \
		[](::Verse::VCell* This) -> void {                                                                                                                                      \
			This->StaticCast<CellType>().ConductCensusImpl();                                                                                                                   \
		},                                                                                                                                                                      \
		std::is_trivially_destructible_v<CellType> ? nullptr : [](::Verse::VCell* This) -> void {                                                                               \
			This->StaticCast<CellType>().~CellType();                                                                                                                           \
		},                                                                                                                                                                      \
		[](::Verse::FRunningContext Context, ::Verse::VCell* This, ::Verse::VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder) -> bool { \
			return This->StaticCast<CellType>().EqualImpl(Context, Other, HandlePlaceholder);                                                                                   \
		},                                                                                                                                                                      \
		[](::Verse::VCell* This) -> uint32 {                                                                                                                                    \
			return This->StaticCast<CellType>().GetTypeHashImpl();                                                                                                              \
		},                                                                                                                                                                      \
		[](::Verse::FRunningContext Context, ::Verse::VCell* This) -> ::Verse::FOpResult {                                                                                      \
			return This->StaticCast<CellType>().MeltImpl(Context);                                                                                                              \
		},                                                                                                                                                                      \
		[](::Verse::FRunningContext Context, ::Verse::VCell* This) -> ::Verse::FOpResult {                                                                                      \
			return This->StaticCast<CellType>().FreezeImpl(Context);                                                                                                            \
		},                                                                                                                                                                      \
		::Verse::Details::GetToStringMethod<CellType>(),                                                                                                                        \
		::Verse::Details::GetSerializeMethod<CellType>(),                                                                                                                       \
		::Verse::Details::GetSerializeNewMethod<CellType>(),                                                                                                                    \
	};                                                                                                                                                                          \
	::Verse::VCppClassInfoRegister CellType##_Register(&CellType::StaticCppClassInfo);

#define DEFINE_BASE_VCPPCLASSINFO(CellType) DEFINE_BASE_OR_DERIVED_VCPPCLASSINFO(CellType, nullptr)

#define DEFINE_DERIVED_VCPPCLASSINFO(CellType)                                                                                                                              \
	static_assert(!std::is_same_v<CellType::Super, CellType>, #CellType " declares itself as its super-class in DECLARE_DERIVED_VCPPCLASSINFO.");                           \
	static_assert(std::is_base_of_v<CellType::Super, CellType>, #CellType " doesn't derive from the super-class declared by DECLARE_DERIVED_VCPPCLASSINFO.");               \
	static_assert(std::is_base_of_v<::Verse::VCell, CellType::Super>, #CellType "'s super-class as declared by DECLARE_DERIVED_VCPPCLASSINFO does not derive from VCell."); \
	static_assert(!std::is_polymorphic_v<CellType>, "VCell-derived C++ classes must not have virtual methods.");                                                            \
	DEFINE_BASE_OR_DERIVED_VCPPCLASSINFO(CellType, &CellType::Super::StaticCppClassInfo)

#define DEFINE_TRIVIAL_VISIT_REFERENCES(CellType) \
	template <typename TVisitor>                  \
	void CellType::VisitReferencesImpl(TVisitor&) \
	{                                             \
	}

// C++ information; this is where the "vtable" goes.
struct VCppClassInfo
{
	const TCHAR* Name;
	VCppClassInfo* SuperClass;
	void (*MarkReferencesImpl)(VCell* This, FMarkStackVisitor&);
	void (*VisitReferencesImpl)(VCell* This, FAbstractVisitor&);
	void (*ConductCensus)(VCell* This);
	void (*RunDestructor)(VCell* This);
	bool (*Equal)(FRunningContext Context, VCell* This, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);
	uint32 (*GetTypeHash)(VCell* This);
	FOpResult (*Melt)(FRunningContext Context, VCell* This);
	FOpResult (*Freeze)(FRunningContext Context, VCell* This);
	void (*ToString)(VCell* This, FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);
	void (*Serialize)(VCell*& This, FAllocationContext Context, FAbstractVisitor& Visitor);
	VCell& (*SerializeNew)(FAllocationContext Context);

	bool IsA(const VCppClassInfo* Other) const
	{
		for (const VCppClassInfo* Current = this; Current; Current = Current->SuperClass)
		{
			if (Current == Other)
			{
				return true;
			}
		}
		return false;
	}

	FORCEINLINE void VisitReferences(VCell* This, FMarkStackVisitor& Visitor)
	{
		MarkReferencesImpl(This, Visitor);
	}

	FORCEINLINE void VisitReferences(VCell* This, FAbstractVisitor& Visitor)
	{
		VisitReferencesImpl(This, Visitor);
	}

	COREUOBJECT_API FString DebugName() const;
};

struct VCppClassInfoRegister
{
	VCppClassInfo* CppClassInfo;
	VCppClassInfoRegister* Next;

	VCppClassInfoRegister(VCppClassInfo* InCppClassInfo);
	~VCppClassInfoRegister();
};

struct VCppClassInfoRegistry
{
	static VCppClassInfo* GetCppClassInfo(FStringView Name);
};

} // namespace Verse
#endif // WITH_VERSE_VM
