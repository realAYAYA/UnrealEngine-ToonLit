// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Parsers;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// Represents the FArrayProperty engine type
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "ArrayProperty", IsProperty = true)]
	public class UhtArrayProperty : UhtContainerBaseProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "ArrayProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "TArray";

		/// <inheritdoc/>
		protected override string PGetMacroText => "TARRAY";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new array property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="value">Inner property value</param>
		public UhtArrayProperty(UhtPropertySettings propertySettings, UhtProperty value) : base(propertySettings, value)
		{
			// If the creation of the value property set more flags, then copy those flags to ourselves
			PropertyFlags |= ValueProperty.PropertyFlags & EPropertyFlags.UObjectWrapper;

			if (ValueProperty.MetaData.ContainsKey(UhtNames.NativeConst))
			{
				MetaData.Add(UhtNames.NativeConstTemplateArg, "");
				ValueProperty.MetaData.Remove(UhtNames.NativeConst);
			}

			PropertyCaps |= UhtPropertyCaps.PassCppArgsByRef | UhtPropertyCaps.SupportsRigVM | UhtPropertyCaps.IsRigVMArray;
			PropertyCaps &= ~(UhtPropertyCaps.CanBeContainerValue | UhtPropertyCaps.CanBeContainerKey);
			UpdateCaps();

			ValueProperty.SourceName = SourceName;
			ValueProperty.EngineName = EngineName;
			ValueProperty.PropertyFlags = PropertyFlags & EPropertyFlags.PropagateToArrayInner;
			ValueProperty.Outer = this;
			ValueProperty.MetaData.Clear();
		}

		/// <inheritdoc/>
		protected override bool ResolveSelf(UhtResolvePhase phase)
		{
			bool results = base.ResolveSelf(phase);
			switch (phase)
			{
				case UhtResolvePhase.Final:
					PropertyFlags |= ResolveAndReturnNewFlags(ValueProperty, phase);
					MetaData.Add(ValueProperty.MetaData);
					ValueProperty.PropertyFlags = PropertyFlags & EPropertyFlags.PropagateToArrayInner;
					ValueProperty.MetaData.Clear();
					PropagateFlagsFromInnerAndHandlePersistentInstanceMetadata(this, MetaData, ValueProperty);
					UpdateCaps();
					break;
			}
			return results;
		}

		private void UpdateCaps()
		{
			PropertyCaps &= ~(UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint | UhtPropertyCaps.CanExposeOnSpawn);
			PropertyCaps |= ValueProperty.PropertyCaps & UhtPropertyCaps.CanExposeOnSpawn;
			if (ValueProperty.PropertyCaps.HasAnyFlags(UhtPropertyCaps.IsParameterSupportedByBlueprint))
			{
				PropertyCaps |= UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint;
			}
		}

		/// <inheritdoc/>
		public override void CollectReferencesInternal(IUhtReferenceCollector collector, bool templateProperty)
		{
			ValueProperty.CollectReferencesInternal(collector, true);
		}

		/// <inheritdoc/>
		public override string? GetForwardDeclarations()
		{
			return ValueProperty.GetForwardDeclarations();
		}

		/// <inheritdoc/>
		public override IEnumerable<UhtType> EnumerateReferencedTypes()
		{
			foreach (UhtType type in ValueProperty.EnumerateReferencedTypes())
			{
				yield return type;
			}
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				case UhtPropertyTextType.SparseShort:
					builder.Append("TArray");
					break;

				case UhtPropertyTextType.FunctionThunkParameterArgType:
					builder.AppendFunctionThunkParameterArrayType(ValueProperty);
					break;

				default:
					builder.Append("TArray<").AppendPropertyText(ValueProperty, textType, true);
					if (builder[^1] == '>')
					{
						// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
						builder.Append(' ');
					}
					builder.Append('>');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			builder.AppendMemberDecl(ValueProperty, context, name, GetNameSuffix(nameSuffix, "_Inner"), tabs);
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FArrayPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			builder.AppendMemberDef(ValueProperty, context, name, GetNameSuffix(nameSuffix, "_Inner"), "0", tabs);
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FArrayPropertyParams", "UECodeGen_Private::EPropertyGenFlags::Array");
			builder.Append(Allocator == UhtPropertyAllocator.MemoryImage ? "EArrayPropertyFlags::UsesMemoryImageAllocator" : "EArrayPropertyFlags::None").Append(", ");
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberPtr(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			builder.AppendMemberPtr(ValueProperty, context, name, GetNameSuffix(nameSuffix, "_Inner"), tabs);
			base.AppendMemberPtr(builder, context, name, nameSuffix, tabs);
			return builder;
		}

		/// <inheritdoc/>
		public override void AppendObjectHashes(StringBuilder builder, int startingLength, IUhtPropertyMemberContext context)
		{
			ValueProperty.AppendObjectHashes(builder, startingLength, context);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.AppendPropertyText(this, UhtPropertyTextType.Construction).Append("()");
			return builder;
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			return false;
		}

		/// <inheritdoc/>
		protected override void ValidateFunctionArgument(UhtFunction function, UhtValidationOptions options)
		{
			base.ValidateFunctionArgument(function, options);

			if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.Net))
			{
				if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetRequest))
				{
					if (RefQualifier != UhtPropertyRefQualifier.ConstRef && !IsStaticArray)
					{
						this.LogError("Replicated TArray parameters must be passed by const reference");
					}
				}
			}
		}

		/// <inheritdoc/>
		public override void Validate(UhtStruct outerStruct, UhtProperty outermostProperty, UhtValidationOptions options)
		{
			base.Validate(outerStruct, outermostProperty, options);

			if (Allocator != UhtPropertyAllocator.Default)
			{
				if (PropertyFlags.HasAnyFlags(EPropertyFlags.Net))
				{
					this.LogError("Replicated arrays with MemoryImageAllocators are not yet supported");
				}
			}

			if (ValueProperty is UhtStructProperty structProperty)
			{
				if (structProperty.ScriptStruct == outerStruct)
				{
					this.LogError($"'Struct' recursion via arrays is unsupported for properties.");
				}
			}
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtArrayProperty otherArray)
			{
				return ValueProperty.IsSameType(otherArray.ValueProperty);
			}
			return false;
		}

		#region Keyword
		[UhtPropertyType(Keyword = "TArray")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		private static UhtProperty? ArrayProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			using UhtMessageContext tokenContext = new("TArray");
			if (!tokenReader.SkipExpectedType(matchedToken.Value, propertySettings.PropertyCategory == UhtPropertyCategory.Member))
			{
				return null;
			}
			tokenReader.Require('<');

			// Parse the value type
			UhtProperty? value = UhtPropertyParser.ParseTemplateParam(resolvePhase, propertySettings, propertySettings.SourceName, tokenReader);
			if (value == null)
			{
				return null;
			}

			if (!value.PropertyCaps.HasAnyFlags(UhtPropertyCaps.CanBeContainerValue))
			{
				tokenReader.LogError($"The type \'{value.GetUserFacingDecl()}\' can not be used as a value in a TArray");
			}

			if (tokenReader.TryOptional(','))
			{
				// If we found a comma, read the next thing, assume it's an allocator, and report that
				UhtToken allocatorToken = tokenReader.GetIdentifier();
				if (allocatorToken.IsIdentifier("FMemoryImageAllocator"))
				{
					propertySettings.Allocator = UhtPropertyAllocator.MemoryImage;
				}
				else if (allocatorToken.IsIdentifier("TMemoryImageAllocator"))
				{
					tokenReader.RequireList('<', '>', "TMemoryImageAllocator template arguments");
					propertySettings.Allocator = UhtPropertyAllocator.MemoryImage;
				}
				else
				{
					throw new UhtException(tokenReader, $"Found '{allocatorToken.Value}' - explicit allocators are not supported in TArray properties.");
				}
			}
			tokenReader.Require('>');

			//@TODO: Prevent sparse delegate types from being used in a container

			return new UhtArrayProperty(propertySettings, value);
		}
		#endregion
	}
}
