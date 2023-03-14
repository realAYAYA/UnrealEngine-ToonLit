// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"

#include <string>

enum class ENativeGameplayTagToken { PRIVATE_USE_MACRO_INSTEAD };

namespace UE::GameplayTags::Private
{
	// Used to prevent people from putting UE_DEFINE_GAMEPLAY_TAG_STATIC and UE_DEFINE_GAMEPLAY_TAG in their headers.
	constexpr bool HasFileExtension(std::string_view file, std::string_view file_ext)
	{
		const auto _Rightsize = file_ext.length();
		if (file.length() < _Rightsize) {
			return false;
		}
		return file.compare((file.length() - _Rightsize), _Rightsize, file_ext) == 0;
	}
}

/**
 * Declares a native gameplay tag that is defined in a cpp with UE_DEFINE_GAMEPLAY_TAG to allow other modules or code to use the created tag variable.
 */
#define UE_DECLARE_GAMEPLAY_TAG_EXTERN(TagName) extern FNativeGameplayTag TagName;

/**
 * Defines a native gameplay tag that is externally declared in a header to allow other modules or code to use the created tag variable.
 */
#define UE_DEFINE_GAMEPLAY_TAG(TagName, Tag) FNativeGameplayTag TagName(UE_PLUGIN_NAME, UE_MODULE_NAME, Tag, TEXT(""), ENativeGameplayTagToken::PRIVATE_USE_MACRO_INSTEAD); static_assert(UE::GameplayTags::Private::HasFileExtension(__FILE__, ".cpp"), "UE_DEFINE_GAMEPLAY_TAG can only be used in .cpp files, if you're trying to share tags across modules, use UE_DECLARE_GAMEPLAY_TAG_EXTERN in the public header, and UE_DEFINE_GAMEPLAY_TAG in the private .cpp");

/**
 * Defines a native gameplay tag such that it's only available to the cpp file you define it in.
 */
#define UE_DEFINE_GAMEPLAY_TAG_STATIC(TagName, Tag) static FNativeGameplayTag TagName(UE_PLUGIN_NAME, UE_MODULE_NAME, Tag, TEXT(""), ENativeGameplayTagToken::PRIVATE_USE_MACRO_INSTEAD); static_assert(UE::GameplayTags::Private::HasFileExtension(__FILE__, ".cpp"), "UE_DEFINE_GAMEPLAY_TAG_STATIC can only be used in .cpp files, if you're trying to share tags across modules, use UE_DECLARE_GAMEPLAY_TAG_EXTERN in the public header, and UE_DEFINE_GAMEPLAY_TAG in the private .cpp");

/**
 * Holds a gameplay tag that was registered during static construction of the module, and will
 * be unregistered when the module unloads.  Each registration is based on the native tag pointer
 * so even if two modules register the same tag and one is unloaded, the tag will still be registered
 * by the other one.
 */
class GAMEPLAYTAGS_API FNativeGameplayTag : public FNoncopyable
{
public:
	FNativeGameplayTag(FName PluginName, FName ModuleName, FName TagName, const FString& TagDevComment, ENativeGameplayTagToken);
	~FNativeGameplayTag();

	operator FGameplayTag() const { return InternalTag; }

	FGameplayTag GetTag() const { return InternalTag; }

	FGameplayTagTableRow GetGameplayTagTableRow() const
	{
#if !UE_BUILD_SHIPPING
		ValidateTagRegistration();
#endif

#if WITH_EDITORONLY_DATA
		return FGameplayTagTableRow(InternalTag.GetTagName(), DeveloperComment);
#else
		return FGameplayTagTableRow(InternalTag.GetTagName());
#endif
	}

private:
	FGameplayTag InternalTag;

#if !UE_BUILD_SHIPPING
	FName PluginName;
	FName ModuleName;
	mutable bool bValidated = false;

	void ValidateTagRegistration() const;
#endif

#if WITH_EDITORONLY_DATA
	FString DeveloperComment;
#endif

	static TSet<const class FNativeGameplayTag*>& GetRegisteredNativeTags();

	friend class UGameplayTagsManager;
};
