// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{
	/// <summary>
	/// FClassProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "ClassProperty", IsProperty = true)]
	public class UhtClassProperty : UhtObjectProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "ClassProperty";

		/// <inheritdoc/>
		protected override bool PGetPassAsNoPtr => PropertyFlags.HasAnyFlags(EPropertyFlags.UObjectWrapper);

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.TypeText;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		/// <param name="classObj">Referenced class</param>
		/// <param name="metaClass">Reference meta class</param>
		/// <param name="extraFlags">Extra flags to apply to the property.</param>
		public UhtClassProperty(UhtPropertySettings propertySettings, UhtClass classObj, UhtClass metaClass, EPropertyFlags extraFlags = EPropertyFlags.None)
			: base(propertySettings, classObj, metaClass)
		{
			PropertyFlags |= extraFlags;
			PropertyCaps |= UhtPropertyCaps.CanHaveConfig;
			PropertyCaps &= ~(UhtPropertyCaps.CanBeInstanced);
		}

		/// <inheritdoc/>
		public override string? GetForwardDeclarations()
		{
			return MetaClass != null ? $"class {MetaClass.SourceName};" : null;
		}

		/// <inheritdoc/>
		private StringBuilder AppendSubClassText(StringBuilder builder)
		{
			builder.Append("TSubclassOf<").Append(MetaClass?.SourceName).Append(">");
			return builder;
		}

		/// <inheritdoc/>
		private StringBuilder AppendText(StringBuilder builder)
		{
			if (PropertyFlags.HasAnyFlags(EPropertyFlags.UObjectWrapper))
			{
				AppendSubClassText(builder);
			}
			else
			{
				builder.Append(Class.SourceName).Append('*');
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				case UhtPropertyTextType.Generic:
				case UhtPropertyTextType.Sparse:
				case UhtPropertyTextType.SparseShort:
				case UhtPropertyTextType.GenericFunctionArgOrRetVal:
				case UhtPropertyTextType.GenericFunctionArgOrRetValImpl:
				case UhtPropertyTextType.ClassFunctionArgOrRetVal:
				case UhtPropertyTextType.EventFunctionArgOrRetVal:
				case UhtPropertyTextType.InterfaceFunctionArgOrRetVal:
				case UhtPropertyTextType.ExportMember:
				case UhtPropertyTextType.Construction:
				case UhtPropertyTextType.FunctionThunkParameterArrayType:
				case UhtPropertyTextType.RigVMType:
				case UhtPropertyTextType.GetterSetterArg:
					AppendText(builder);
					break;

				case UhtPropertyTextType.FunctionThunkRetVal:
					if (PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm))
					{
						builder.Append("const ");
					}
					if (PropertyFlags.HasAnyFlags(EPropertyFlags.UObjectWrapper))
					{
						AppendText(builder);
					}
					else
					{
						builder.Append("UClass*");
					}
					break;

				case UhtPropertyTextType.EventParameterMember:
				case UhtPropertyTextType.EventParameterFunctionMember:
					if (!PropertyFlags.HasAnyFlags(EPropertyFlags.UObjectWrapper))
					{
						builder.Append("UClass*");
					}
					else
					{
						AppendText(builder);
					}
					break;

				case UhtPropertyTextType.FunctionThunkParameterArgType:
					if (PropertyFlags.HasAllFlags(EPropertyFlags.OutParm | EPropertyFlags.UObjectWrapper))
					{
						AppendSubClassText(builder);
					}
					else
					{
						builder.Append(Class.SourceName);
					}
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FClassPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FClassPropertyParams", "UECodeGen_Private::EPropertyGenFlags::Class");
			AppendMemberDefRef(builder, context, Class, false);
			AppendMemberDefRef(builder, context, MetaClass, false);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		#region Keyword
		[UhtPropertyType(Keyword = "TSubclassOf")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? SubclassOfProperty(UhtPropertyResolvePhase resolvePhase, UhtPropertySettings propertySettings, IUhtTokenReader tokenReader, UhtToken matchedToken)
		{
			UhtClass? metaClass = UhtObjectPropertyBase.ParseTemplateClass(propertySettings, tokenReader, matchedToken);
			if (metaClass == null)
			{
				return null;
			}

			// With TSubclassOf, MetaClass is used as a class limiter.  
			return new UhtClassProperty(propertySettings, metaClass.Session.UClass, metaClass, EPropertyFlags.UObjectWrapper);
		}
		#endregion
	}
}
