// Copyright Epic Games, Inc. All Rights Reserved.

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
	/// FInterfaceProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "InterfaceProperty", IsProperty = true)]
	public class UhtInterfaceProperty : UhtProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "InterfaceProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "TScriptInterface";

		/// <inheritdoc/>
		protected override string PGetMacroText => "TINTERFACE";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Referenced interface class
		/// </summary>
		[JsonConverter(typeof(UhtTypeSourceNameJsonConverter<UhtClass>))]
		public UhtClass InterfaceClass { get; set; }

		/// <summary>
		/// Create a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="interfaceClass">Referenced interface</param>
		public UhtInterfaceProperty(UhtPropertySettings propertySettings, UhtClass interfaceClass) : base(propertySettings)
		{
			InterfaceClass = interfaceClass;
			PropertyFlags |= EPropertyFlags.UObjectWrapper;
			PropertyCaps |= UhtPropertyCaps.CanExposeOnSpawn | UhtPropertyCaps.IsParameterSupportedByBlueprint | UhtPropertyCaps.IsMemberSupportedByBlueprint | UhtPropertyCaps.PassCppArgsByRef;
			PropertyCaps &= ~(UhtPropertyCaps.CanHaveConfig | UhtPropertyCaps.CanBeContainerKey);
			if (Session.Config!.AreRigVMUInterfaceProeprtiesEnabled)
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
					if (InterfaceClass.HierarchyHasAnyClassFlags(EClassFlags.DefaultToInstanced))
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
			return !DisallowPropertyFlags.HasAnyFlags(EPropertyFlags.InstancedReference) && InterfaceClass.HierarchyHasAnyClassFlags(EClassFlags.DefaultToInstanced);
		}

		/// <inheritdoc/>
		public override void CollectReferencesInternal(IUhtReferenceCollector collector, bool templateProperty)
		{
			collector.AddCrossModuleReference(InterfaceClass, false);
		}

		/// <inheritdoc/>
		public override string? GetForwardDeclarations()
		{
			UhtClass? exportClass = InterfaceClass;
			while (exportClass != null && !exportClass.ClassFlags.HasAnyFlags(EClassFlags.Native))
			{
				exportClass = exportClass.SuperClass;
			}
			return exportClass != null ? $"class {exportClass.SourceName};" : null;
		}

		/// <inheritdoc/>
		public override IEnumerable<UhtType> EnumerateReferencedTypes()
		{
			yield return InterfaceClass;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				case UhtPropertyTextType.SparseShort:
					builder.Append("TScriptInterface");
					break;

				case UhtPropertyTextType.FunctionThunkParameterArgType:
					builder.Append(InterfaceClass.SourceName);
					break;

				default:
					builder.Append("TScriptInterface<").Append(InterfaceClass.SourceName).Append('>');
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FInterfacePropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			// FScriptInterface<USomeInterface> is valid so in that case we need to pass in the interface class and not the alternate object (which in the end is the same object)
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FInterfacePropertyParams", "UECodeGen_Private::EPropertyGenFlags::Interface");
			AppendMemberDefRef(builder, context, InterfaceClass.AlternateObject ?? InterfaceClass, false);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.Append("NULL");
			return builder;
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			return false;
		}

		/// <inheritdoc/>
		protected override void ValidateMember(UhtStruct structObj, UhtValidationOptions options)
		{
			base.ValidateMember(structObj, options);

			if (PointerType == UhtPointerType.Native)
			{
				this.LogError($"UPROPERTY pointers cannot be interfaces - did you mean TScriptInterface<{InterfaceClass.SourceName}>?");
			}
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtInterfaceProperty otherObject)
			{
				return InterfaceClass == otherObject.InterfaceClass;
			}
			return false;
		}

		/// <inheritdoc/>
		public override bool MustBeConstArgument([NotNullWhen(true)] out UhtType? errorType)
		{
			errorType = InterfaceClass;
			return InterfaceClass.ClassFlags.HasAnyFlags(EClassFlags.Const);
		}

		#region Keywords
		[UhtPropertyType(Keyword = "FScriptInterface", Options = UhtPropertyTypeOptions.Simple)] // This can't be immediate due to the reference to UInterface
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? FScriptInterfaceProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			return new UhtInterfaceProperty(propertySettings, propertySettings.Outer.Session.IInterface);
		}

		[UhtPropertyType(Keyword = "TScriptInterface")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? TScriptInterfaceProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtClass? propertyClass = UhtObjectPropertyBase.ParseTemplateObject(propertySettings, tokenReader, matchedToken, false);
			if (propertyClass == null)
			{
				return null;
			}

			if (propertyClass.ClassFlags.HasAnyFlags(EClassFlags.Interface))
			{
				return new UhtInterfaceProperty(propertySettings, propertyClass);
			}
			return new UhtObjectProperty(propertySettings, propertyClass, null, EPropertyFlags.UObjectWrapper);
		}
		#endregion
	}
}
