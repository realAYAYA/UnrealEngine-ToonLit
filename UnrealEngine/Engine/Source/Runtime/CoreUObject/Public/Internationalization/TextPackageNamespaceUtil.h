// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextNamespaceFwd.h"
#include "Internationalization/TextNamespaceUtil.h"

class FTextProperty;
class UObject;
class UPackage;

namespace TextNamespaceUtil
{

#if USE_STABLE_LOCALIZATION_KEYS

/**
 * Given a package, try and get the namespace it should use for localization.
 *
 * @param InPackage			The package to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace.
 */
COREUOBJECT_API FString GetPackageNamespace(const UPackage* InPackage);

/**
 * Given an object, try and get the namespace it should use for localization (from its owner package).
 *
 * @param InObject			The object to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace.
 */
COREUOBJECT_API FString GetPackageNamespace(const UObject* InObject);

/**
 * Given a package, try and ensure it has a namespace it should use for localization.
 *
 * @param InPackage			The package to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace and one could not be added.
 */
COREUOBJECT_API FString EnsurePackageNamespace(UPackage* InPackage);

/**
 * Given an object, try and ensure it has a namespace it should use for localization (from its owner package).
 *
 * @param InObject			The object to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace and one could not be added.
 */
COREUOBJECT_API FString EnsurePackageNamespace(UObject* InObject);

/**
 * Given a package, clear any namespace it has set for localization.
 *
 * @param InPackage			The package to clear the namespace for.
 */
COREUOBJECT_API void ClearPackageNamespace(UPackage* InPackage);

/**
 * Given an object, clear any namespace it has set for localization (from its owner package).
 *
 * @param InObject			The object to clear the namespace for.
 */
COREUOBJECT_API void ClearPackageNamespace(UObject* InObject);

/**
 * Given a package, force it to have the given namespace for localization (even if a transient package!).
 *
 * @param InPackage			The package to set the namespace for.
 * @param InNamespace		The namespace to set.
 */
COREUOBJECT_API void ForcePackageNamespace(UPackage* InPackage, const FString& InNamespace);

/**
 * Given an object, force it to have the given namespace for localization (from its owner package, even if a transient package!).
 *
 * @param InObject			The object to set the namespace for.
 * @param InNamespace		The namespace to set.
 */
COREUOBJECT_API void ForcePackageNamespace(UObject* InObject, const FString& InNamespace);

#endif // USE_STABLE_LOCALIZATION_KEYS

/**
 * Make a copy of the given text that's valid to use with the given package, optionally preserving its existing key.
 * @note Returns the result verbatim if there is no change when applying the package namespace to the text.
 *
 * @param InText						The current FText instance.
 * @param InPackage/InObject			The package (or object to get the owner package from) to get the namespace for (will call EnsurePackageNamespace).
 * @param InCopyMethod					The method that should be used to copy the FText instance.
 * @param bAlwaysApplyPackageNamespace	If true, this will always apply the package namespace to the text namespace (always treated as ETextCopyMethod::Verbatim when USE_STABLE_LOCALIZATION_KEYS is false).
 *										If false, this will only apply the package namespace if the text namespace already contains package namespace markers.
 *
 * @return A copy of the given text that's valid to use with the given package.
 */
COREUOBJECT_API FText CopyTextToPackage(const FText& InText, UPackage* InPackage, const ETextCopyMethod InCopyMethod = ETextCopyMethod::NewKey, const bool bAlwaysApplyPackageNamespace = false);
COREUOBJECT_API FText CopyTextToPackage(const FText& InText, UObject* InObject, const ETextCopyMethod InCopyMethod = ETextCopyMethod::NewKey, const bool bAlwaysApplyPackageNamespace = false);

/**
 * Generate a random text key.
 * @note This key will be a GUID.
 */
COREUOBJECT_API FString GenerateRandomTextKey();

/**
 * Generate a deterministic text key based on the given object and property info.
 * @note This key will be formatted like a GUID, but the value will actually be based on deterministic hashes.
 *
 * @param InTextOwner					The object that owns the given TextProperty.
 * @param InTextProperty				The text property to generate the key for.
 * @param bApplyPackageNamespace		If true, apply the package namespace to the generated key hash (when USE_STABLE_LOCALIZATION_KEYS is true).
 */
COREUOBJECT_API FString GenerateDeterministicTextKey(UObject* InTextOwner, const FTextProperty* InTextProperty, const bool bApplyPackageNamespace = true);
COREUOBJECT_API FString GenerateDeterministicTextKey(UObject* InTextOwner, const FName InTextPropertyName, const bool bApplyPackageNamespace = true);

enum class ETextEditAction : uint8
{
	Namespace,
	Key,
	SourceString,
};

/**
 * Called when editing a text property to determine the new ID for the text, ideally using the proposed text ID when possible (and when USE_STABLE_LOCALIZATION_KEYS is true).
 * 
 * @param InPackage						The package to query the namespace for.
 * @param InEditAction					How has the given text been edited?
 * @param InTextSource					The current source string for the text being edited. Can be empty when InEditAction is ETextEditAction::SourceString.
 * @param InProposedNamespace			The namespace we'd like to assign to the edited text.
 * @param InProposedKey					The key we'd like to assign to the edited text.
 * @param OutStableNamespace			The namespace that should be assigned to the edited text.
 * @param OutStableKey					The key that should be assigned to the edited text.
 * @param InTextKeyGenerator			Generator for the new text key. Will generate a random key by default.
 * @param bApplyPackageNamespace		If true, apply the package namespace to the generated text ID (when USE_STABLE_LOCALIZATION_KEYS is true).
 */
COREUOBJECT_API void GetTextIdForEdit(UPackage* InPackage, const ETextEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey, TFunctionRef<FString()> InTextKeyGenerator = &GenerateRandomTextKey, const bool bApplyPackageNamespace = true);

/**
 * Edit an attribute of the given text property, akin to what happens when editing a text property in a details panel.
 *
 * @param InTextOwner					The object that owns the given TextProperty to be edited.
 * @param InTextProperty				The text property to edit. This must be a property that exists on TextOwner.
 * @param InEditAction					How has the given text been edited?
 * @param InEditValue					The new value of the attribute that was edited.
 * @param InTextKeyGenerator			Generator for the new text key. Will generate a random key by default.
 * @param bApplyPackageNamespace		If true, apply the package namespace to the generated text ID (when USE_STABLE_LOCALIZATION_KEYS is true).
 * 
 * @return True if edit was possible, or false if not.
 */
COREUOBJECT_API bool EditTextProperty(UObject* InTextOwner, const FTextProperty* InTextProperty, const ETextEditAction InEditAction, const FString& InEditValue, TFunctionRef<FString()> InTextKeyGenerator = &GenerateRandomTextKey, const bool bApplyPackageNamespace = true);

}
