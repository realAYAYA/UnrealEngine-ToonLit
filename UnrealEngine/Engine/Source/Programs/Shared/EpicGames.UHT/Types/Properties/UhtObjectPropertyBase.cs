// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Text;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{
	/// <summary>
	/// FObjectPropertyBase
	/// </summary>
	[UhtEngineClass(Name = "ObjectPropertyBase", IsProperty = true)]
	public abstract class UhtObjectPropertyBase : UhtProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "ObjectPropertyBase";

		/// <inheritdoc/>
		protected override string CppTypeText => "Object";

		/// <inheritdoc/>
		protected override string PGetMacroText => "OBJECT";

		/// <summary>
		/// Referenced UCLASS
		/// </summary>
		[JsonConverter(typeof(UhtTypeSourceNameJsonConverter<UhtClass>))]
		public UhtClass Class { get; set; }

		/// <summary>
		/// Referenced UCLASS for class properties
		/// </summary>
		[JsonConverter(typeof(UhtNullableTypeSourceNameJsonConverter<UhtClass>))]
		public UhtClass? MetaClass { get; set; }

		/// <summary>
		/// Construct a property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="classObj">Referenced UCLASS</param>
		/// <param name="metaClass">Referenced UCLASS used by class properties</param>
		protected UhtObjectPropertyBase(UhtPropertySettings propertySettings, UhtClass classObj, UhtClass? metaClass) : base(propertySettings)
		{
			Class = classObj;
			MetaClass = metaClass;

			// This applies to EVERYTHING including raw pointer
			// Imply const if it's a parameter that is a pointer to a const class
			// NOTE: We shouldn't be automatically adding const param because in some cases with functions and blueprint native event, the 
			// generated code won't match.  For now, just disabled the auto add in that case and check for the error in the validation code.
			// Otherwise, the user is not warned and they will get compile errors.
			if (propertySettings.PropertyCategory != UhtPropertyCategory.Member && Class.ClassFlags.HasAnyFlags(EClassFlags.Const) && !propertySettings.Options.HasAnyFlags(UhtPropertyOptions.NoAutoConst))
			{
				PropertyFlags |= EPropertyFlags.ConstParm;
			}

			PropertyCaps &= ~(UhtPropertyCaps.CanHaveConfig);
			if (Session.Config!.AreRigVMUObjectPropertiesEnabled)
			{
				PropertyCaps |= UhtPropertyCaps.SupportsRigVM;
			}
		}

		/// <inheritdoc/>
		protected override bool ResolveSelf(UhtResolvePhase phase)
		{
			bool results = base.ResolveSelf(phase);
			switch (phase)
			{
				case UhtResolvePhase.Final:
					if (Class.HierarchyHasAnyClassFlags(EClassFlags.DefaultToInstanced))
					{
						PropertyFlags |= (EPropertyFlags.InstancedReference | EPropertyFlags.ExportObject) & ~DisallowPropertyFlags;
					}
					break;
			}
			return results;
		}

		/// <inheritdoc/>
		public override bool ScanForInstancedReferenced(bool deepScan)
		{
			return !DisallowPropertyFlags.HasAnyFlags(EPropertyFlags.InstancedReference) && Class.HierarchyHasAnyClassFlags(EClassFlags.DefaultToInstanced);
		}

		/// <inheritdoc/>
		public override void CollectReferencesInternal(IUhtReferenceCollector collector, bool templateProperty)
		{
			collector.AddCrossModuleReference(Class, false);
			collector.AddCrossModuleReference(MetaClass, false);
		}

		/// <inheritdoc/>
		public override string? GetForwardDeclarations()
		{
			return $"class {Class.SourceName};";
		}

		/// <inheritdoc/>
		public override IEnumerable<UhtType> EnumerateReferencedTypes()
		{
			yield return Class;
			if (MetaClass != null)
			{
				yield return MetaClass;
			}
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			if (defaultValueReader.TryOptional("NULL") ||
				defaultValueReader.TryOptional("nullptr") ||
				(defaultValueReader.TryOptionalConstInt(out int value) && value == 0))
			{
				innerDefaultValue.Append("None");
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override void ValidateDeprecated()
		{
			ValidateDeprecatedClass(Class);
			ValidateDeprecatedClass(MetaClass);
		}

		/// <inheritdoc/>
		public override bool MustBeConstArgument([NotNullWhen(true)] out UhtType? errorType)
		{
			errorType = Class;
			return Class.ClassFlags.HasAnyFlags(EClassFlags.Const);
		}

		#region Parsing support methods
		/// <summary>
		/// Parse a template type
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="matchedToken">Token matched for type</param>
		/// <param name="returnUInterface">If true, return the UInterface instead of the type listed</param>
		/// <returns>Referenced class</returns>
		public static UhtClass? ParseTemplateObject(UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken, bool returnUInterface)
		{
			UhtSession session = propertySettings.Outer.Session;
			if (tokenReader.TryOptional("const"))
			{
				propertySettings.MetaData.Add(UhtNames.NativeConst, "");
			}
			if (!tokenReader.SkipExpectedType(matchedToken.Value, propertySettings.PropertyCategory == UhtPropertyCategory.Member))
			{
				return null;
			}
			bool isNativeConstTemplateArg = false;
			UhtToken identifier = new();
			tokenReader
				.Require('<')
				.Optional("const", () => { isNativeConstTemplateArg = true; })
				.Optional("class")
				.RequireIdentifier((ref UhtToken token) => { identifier = token; })
				.Optional("const", () => { isNativeConstTemplateArg = true; })
				.Require('>');

			if (isNativeConstTemplateArg)
			{
				propertySettings.MetaData.Add(UhtNames.NativeConstTemplateArg, "");
			}
			session.Config!.RedirectTypeIdentifier(ref identifier);
			UhtClass? returnClass = propertySettings.Outer.FindType(UhtFindOptions.SourceName | UhtFindOptions.Class, ref identifier, tokenReader) as UhtClass;
			if (returnClass != null && returnClass.AlternateObject != null && returnUInterface)
			{
				returnClass = returnClass.AlternateObject as UhtClass;
			}
			return returnClass;
		}

		/// <summary>
		/// Parse a template type
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="matchedToken">Token matched for type</param>
		/// <returns>Referenced class</returns>
		public static UhtClass? ParseTemplateClass(UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtSession session = propertySettings.Outer.Session;
			UhtToken identifier = new();

			if (tokenReader.TryOptional("const"))
			{
				propertySettings.MetaData.Add(UhtNames.NativeConst, "");
			}

			tokenReader.Optional("class");

			if (!tokenReader.SkipExpectedType(matchedToken.Value, propertySettings.PropertyCategory == UhtPropertyCategory.Member))
			{
				return null;
			}

			tokenReader
				.Require('<')
				.Optional("class")
				.RequireIdentifier((ref UhtToken token) => { identifier = token; })
				.Require('>');

			session.Config!.RedirectTypeIdentifier(ref identifier);
			UhtClass? returnClass = propertySettings.Outer.FindType(UhtFindOptions.SourceName | UhtFindOptions.Class, ref identifier, tokenReader) as UhtClass;
			if (returnClass != null && returnClass.AlternateObject != null)
			{
				returnClass = returnClass.AlternateObject as UhtClass;
			}
			return returnClass;
		}

		/// <summary>
		/// Logs message for Object pointers to convert UObject* to TObjectPtr or the reverse
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="engineBehavior">Expected behavior for engine types</param>
		/// <param name="enginePluginBehavior">Expected behavior for engine plugin types</param>
		/// <param name="nonEngineBehavior">Expected behavior for non-engine types</param>
		/// <param name="pointerTypeDesc">Description of the pointer type</param>
		/// <param name="tokenReader">Token reader for type being parsed</param>
		/// <param name="typeStartPos">Starting character position of the type</param>
		/// <param name="alternativeTypeDesc">Suggested alternate declaration</param>
		/// <exception cref="UhtIceException">Thrown if the behavior type is unexpected</exception>
		public static void ConditionalLogPointerUsage(UhtPropertySettings propertySettings, UhtIssueBehavior engineBehavior, UhtIssueBehavior enginePluginBehavior,
			UhtIssueBehavior nonEngineBehavior, string pointerTypeDesc, IUhtTokenReader tokenReader, int typeStartPos, string? alternativeTypeDesc)
		{
			if (propertySettings.PropertyCategory != UhtPropertyCategory.Member)
			{
				return;
			}

			UhtPackage package = propertySettings.Outer.Package;
			UhtIssueBehavior behavior = nonEngineBehavior;
			if (package.IsPartOfEngine)
			{
				if (package.IsPlugin)
				{
					behavior = enginePluginBehavior;
				}
				else
				{
					behavior = engineBehavior;
				}
			}

			if (behavior == UhtIssueBehavior.AllowSilently)
			{
				return;
			}

			string type = tokenReader.GetStringView(typeStartPos, tokenReader.InputPos - typeStartPos).ToString();
			type = type.Replace("\n", "\\n", StringComparison.Ordinal);
			type = type.Replace("\r", "\\r", StringComparison.Ordinal);
			type = type.Replace("\t", "\\t", StringComparison.Ordinal);

			switch (behavior)
			{
				case UhtIssueBehavior.Disallow:
					if (!String.IsNullOrEmpty(alternativeTypeDesc))
					{
						tokenReader.LogError($"{pointerTypeDesc} usage in member declaration detected [[[{type}]]].  This is disallowed for the target/module, consider {alternativeTypeDesc} as an alternative.");
					}
					else
					{
						tokenReader.LogError($"{pointerTypeDesc} usage in member declaration detected [[[{type}]]].");
					}
					break;

				case UhtIssueBehavior.AllowAndLog:
					if (!String.IsNullOrEmpty(alternativeTypeDesc))
					{
						tokenReader.LogInfo($"{pointerTypeDesc} usage in member declaration detected [[[{type}]]].  Consider {alternativeTypeDesc} as an alternative.");
					}
					else
					{
						tokenReader.LogInfo("{PointerTypeDesc} usage in member declaration detected [[[{Type}]]].");
					}
					break;

				default:
					throw new UhtIceException("Unknown enum value");
			}
		}
		#endregion

		/// <inheritdoc/>
		protected override bool NeedsGCBarrierWhenPassedToFunctionImpl(UhtFunction function)
		{
			Type type = GetType();
			return type == typeof(UhtObjectProperty)
				|| (type == typeof(UhtClassProperty) && !PropertyFlags.HasFlag(EPropertyFlags.UObjectWrapper));
		}
	}
}
