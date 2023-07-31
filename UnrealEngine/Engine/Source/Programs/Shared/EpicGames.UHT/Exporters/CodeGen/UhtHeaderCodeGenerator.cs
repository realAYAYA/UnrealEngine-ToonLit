// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	class UhtHeaderCodeGenerator : UhtPackageCodeGenerator
	{
		#region Define block macro names
		public const string EventParamsMacroSuffix = "EVENT_PARMS";
		public const string CallbackWrappersMacroSuffix = "CALLBACK_WRAPPERS";
		public const string SparseDataMacroSuffix = "SPARSE_DATA";
		public const string EditorOnlyRpcWrappersMacroSuffix = "EDITOR_ONLY_RPC_WRAPPERS";
		public const string RpcWrappersMacroSuffix = "RPC_WRAPPERS";
		public const string EditorOnlyRpcWrappersNoPureDeclsMacroSuffix = "EDITOR_ONLY_RPC_WRAPPERS_NO_PURE_DECLS";
		public const string RpcWrappersNoPureDeclsMacroSuffix = "RPC_WRAPPERS_NO_PURE_DECLS";
		public const string AccessorsMacroSuffix = "ACCESSORS";
		public const string ArchiveSerializerMacroSuffix = "ARCHIVESERIALIZER";
		public const string StandardConstructorsMacroSuffix = "STANDARD_CONSTRUCTORS";
		public const string EnchancedConstructorsMacroSuffix = "ENHANCED_CONSTRUCTORS";
		public const string GeneratedBodyMacroSuffix = "GENERATED_BODY";
		public const string GeneratedBodyLegacyMacroSuffix = "GENERATED_BODY_LEGACY";
		public const string GeneratedUInterfaceBodyMacroSuffix = "GENERATED_UINTERFACE_BODY()";
		public const string FieldNotifyMacroSuffix = "FIELDNOTIFY";
		public const string InClassMacroSuffix = "INCLASS";
		public const string InClassNoPureDeclsMacroSuffix = "INCLASS_NO_PURE_DECLS";
		public const string InClassIInterfaceMacroSuffix = "INCLASS_IINTERFACE";
		public const string InClassIInterfaceNoPureDeclsMacroSuffix = "INCLASS_IINTERFACE_NO_PURE_DECLS";
		public const string PrologMacroSuffix = "PROLOG";
		public const string DelegateMacroSuffix = "DELEGATE";
		#endregion

		public readonly UhtHeaderFile HeaderFile;
		public string FileId => HeaderInfos[HeaderFile.HeaderFileTypeIndex].FileId;

		public UhtHeaderCodeGenerator(UhtCodeGenerator codeGenerator, UhtPackage package, UhtHeaderFile headerFile)
			: base(codeGenerator, package)
		{
			HeaderFile = headerFile;
		}

		#region Event parameter
		protected static StringBuilder AppendEventParameter(StringBuilder builder, UhtFunction function, string strippedFunctionName, UhtPropertyTextType textType, bool outputConstructor, int tabs, string endl)
		{
			if (!WillExportEventParameters(function))
			{
				return builder;
			}

			string eventParameterStructName = GetEventStructParametersName(function.Outer, strippedFunctionName);

			builder.AppendTabs(tabs).Append("struct ").Append(eventParameterStructName).Append(endl);
			builder.AppendTabs(tabs).Append('{').Append(endl);

			++tabs;
			foreach (UhtProperty property in function.Children)
			{
				bool emitConst = property.PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm) && property is UhtObjectProperty;

				//@TODO: UCREMOVAL: This is awful code duplication to avoid a double-const
				{
					//@TODO: bEmitConst will only be true if we have an object, so checking interface here doesn't do anything.
					// export 'const' for parameters
					bool isConstParam = property is UhtInterfaceProperty && !property.PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm); //@TODO: This should be const once that flag exists
					bool isOnConstClass = false;
					if (property is UhtObjectProperty objectProperty)
					{
						isOnConstClass = objectProperty.Class.ClassFlags.HasAnyFlags(EClassFlags.Const);
					}

					if (isConstParam || isOnConstClass)
					{
						emitConst = false; // ExportCppDeclaration will do it for us
					}
				}

				builder.AppendTabs(tabs);
				if (emitConst)
				{
					builder.Append("const ");
				}

				builder.AppendFullDecl(property, textType, false);
				builder.Append(';').Append(endl);
			}

			if (outputConstructor)
			{
				UhtProperty? returnProperty = function.ReturnProperty;
				if (returnProperty != null && returnProperty.PropertyCaps.HasAnyFlags(UhtPropertyCaps.RequiresNullConstructorArg))
				{
					builder.Append(endl);
					builder.AppendTabs(tabs).Append("/** Constructor, initializes return property only **/").Append(endl);
					builder.AppendTabs(tabs).Append(eventParameterStructName).Append("()").Append(endl);
					builder.AppendTabs(tabs + 1).Append(": ").Append(returnProperty.SourceName).Append('(').AppendNullConstructorArg(returnProperty, true).Append(')').Append(endl);
					builder.AppendTabs(tabs).Append('{').Append(endl);
					builder.AppendTabs(tabs).Append('}').Append(endl);
				}
			}
			--tabs;

			builder.AppendTabs(tabs).Append("};").Append(endl);

			return builder;
		}

		protected static bool WillExportEventParameters(UhtFunction function)
		{
			return function.Children.Count > 0;
		}

		protected static string GetEventStructParametersName(UhtType? outer, string functionName)
		{
			string outerName = String.Empty;
			if (outer == null)
			{
				throw new UhtIceException("Outer type not set on delegate function");
			}
			else if (outer is UhtClass classObj)
			{
				outerName = classObj.EngineName;
			}
			else if (outer is UhtHeaderFile)
			{
				string packageName = outer.Package.EngineName;
				outerName = packageName.Replace('/', '_');
			}

			string result = $"{outerName}_event{functionName}_Parms";
			if (UhtFCString.IsDigit(result[0]))
			{
				result = "_" + result;
			}
			return result;
		}
		#endregion

		#region Function helper methods
		protected StringBuilder AppendNativeFunctionHeader(StringBuilder builder, UhtFunction function, UhtPropertyTextType textType, bool isDeclaration,
			string? alternateFunctionName, string? extraParam, UhtFunctionExportFlags extraExportFlags, string endl)
		{
			UhtClass? outerClass = function.Outer as UhtClass;
			UhtFunctionExportFlags exportFlags = function.FunctionExportFlags | extraExportFlags;
			bool isDelegate = function.FunctionFlags.HasAnyFlags(EFunctionFlags.Delegate);
			bool isInterface = !isDelegate && (outerClass != null && outerClass.ClassFlags.HasAnyFlags(EClassFlags.Interface));
			bool isK2Override = function.FunctionFlags.HasAnyFlags(EFunctionFlags.BlueprintEvent);

			if (!isDelegate)
			{
				builder.Append('\t');
			}

			if (isDeclaration)
			{
				// If the function was marked as 'RequiredAPI', then add the *_API macro prefix.  Note that if the class itself
				// was marked 'RequiredAPI', this is not needed as C++ will exports all methods automatically.
				if (textType != UhtPropertyTextType.EventFunctionArgOrRetVal &&
					!(outerClass != null && outerClass.ClassFlags.HasAnyFlags(EClassFlags.RequiredAPI)) &&
					exportFlags.HasAnyFlags(UhtFunctionExportFlags.RequiredAPI))
				{
					builder.Append(PackageApi);
				}

				if (textType == UhtPropertyTextType.InterfaceFunctionArgOrRetVal)
				{
					builder.Append("static ");
				}
				else if (isK2Override)
				{
					builder.Append("virtual ");
				}
				// if the owning class is an interface class
				else if (isInterface)
				{
					builder.Append("virtual ");
				}
				// this is not an event, the function is not a static function and the function is not marked final
				else if (textType != UhtPropertyTextType.EventFunctionArgOrRetVal && !function.FunctionFlags.HasAnyFlags(EFunctionFlags.Static) && !exportFlags.HasAnyFlags(UhtFunctionExportFlags.Final))
				{
					builder.Append("virtual ");
				}
				else if (exportFlags.HasAnyFlags(UhtFunctionExportFlags.Inline))
				{
					builder.Append("inline ");
				}
			}

			UhtProperty? returnProperty = function.ReturnProperty;
			if (returnProperty != null)
			{
				if (returnProperty.PropertyFlags.HasAnyFlags(EPropertyFlags.ConstParm))
				{
					builder.Append("const ");
				}
				builder.AppendPropertyText(returnProperty, textType);
			}
			else
			{
				builder.Append("void");
			}

			builder.Append(' ');
			if (!isDeclaration && outerClass != null)
			{
				builder.AppendClassSourceNameOrInterfaceName(outerClass).Append("::");
			}

			if (alternateFunctionName != null)
			{
				builder.Append(alternateFunctionName);
			}
			else
			{
				switch (textType)
				{
					case UhtPropertyTextType.InterfaceFunctionArgOrRetVal:
						builder.Append("Execute_").Append(function.SourceName);
						break;
					case UhtPropertyTextType.EventFunctionArgOrRetVal:
						builder.Append(function.MarshalAndCallName);
						break;
					case UhtPropertyTextType.ClassFunctionArgOrRetVal:
						builder.Append(function.CppImplName);
						break;
					default:
						throw new UhtIceException("Unexpected type text");
				}
			}

			AppendParameters(builder, function, textType, extraParam, false);

			if (textType != UhtPropertyTextType.InterfaceFunctionArgOrRetVal)
			{
				if (!isDelegate && function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const))
				{
					builder.Append(" const");
				}

				if (isInterface && isDeclaration)
				{
					// all methods in interface classes are pure virtuals
					if (isK2Override)
					{
						// For BlueprintNativeEvent methods we emit a stub implementation. This allows Blueprints that implement the interface class to be nativized.
						builder.Append(" {");
						if (returnProperty != null)
						{
							if (returnProperty is UhtByteProperty byteProperty && byteProperty.Enum != null)
							{
								builder.Append(" return TEnumAsByte<").Append(byteProperty.Enum.CppType).Append(">(").AppendNullConstructorArg(returnProperty, false).Append("); ");
							}
							else if (returnProperty is UhtEnumProperty enumProperty && enumProperty.Enum.CppForm != UhtEnumCppForm.EnumClass)
							{
								builder.Append(" return TEnumAsByte<").Append(enumProperty.Enum.CppType).Append(">(").AppendNullConstructorArg(returnProperty, false).Append("); ");
							}
							else
							{
								builder.Append(" return ").AppendNullConstructorArg(returnProperty, false).Append("; ");
							}
						}
						builder.Append('}');
					}
					else
					{
						builder.Append("=0");
					}
				}
			}
			builder.Append(endl);

			return builder;
		}

		protected static StringBuilder AppendEventFunctionPrologue(StringBuilder builder, UhtFunction function, string functionName, int tabs, string endl)
		{
			builder.AppendTabs(tabs).Append('{').Append(endl);
			if (function.Children.Count == 0)
			{
				return builder;
			}

			++tabs;

			string eventParameterStructName = GetEventStructParametersName(function.Outer, functionName);

			builder.AppendTabs(tabs).Append(eventParameterStructName).Append(" Parms;").Append(endl);

			// Declare a parameter struct for this event/delegate and assign the struct members using the values passed into the event/delegate call.
			foreach (UhtProperty property in function.ParameterProperties.Span)
			{
				if (property.IsStaticArray)
				{
					builder.AppendTabs(tabs).Append("FMemory::Memcpy(Parm.")
						.Append(property.SourceName).Append(',')
						.Append(property.SourceName).Append(",sizeof(Parms.")
						.Append(property.SourceName).Append(");").Append(endl);
				}
				else
				{
					builder.AppendTabs(tabs).Append("Parms.").Append(property.SourceName).Append('=').Append(property.SourceName);
					if (property is UhtBoolProperty)
					{
						builder.Append(" ? true : false");
					}
					builder.Append(';').Append(endl);
				}
			}
			return builder;
		}

		protected static StringBuilder AppendEventFunctionEpilogue(StringBuilder builder, UhtFunction function, int tabs, string endl)
		{
			++tabs;

			// Out parm copying.
			foreach (UhtProperty property in function.ParameterProperties.Span)
			{
				if (property.PropertyFlags.HasExactFlags(EPropertyFlags.ConstParm | EPropertyFlags.OutParm, EPropertyFlags.OutParm))
				{
					if (property.IsStaticArray)
					{
						builder
							.AppendTabs(tabs)
							.Append("FMemory::Memcpy(&")
							.Append(property.SourceName)
							.Append(",&Parms.")
							.Append(property.SourceName)
							.Append(",sizeof(")
							.Append(property.SourceName)
							.Append("));").Append(endl);
					}
					else
					{
						builder
							.AppendTabs(tabs)
							.Append(property.SourceName)
							.Append("=Parms.")
							.Append(property.SourceName)
							.Append(';').Append(endl);
					}
				}
			}

			// Return value.
			UhtProperty? returnProperty = function.ReturnProperty;
			if (returnProperty != null)
			{
				// Make sure uint32 -> bool is supported
				if (returnProperty is UhtBoolProperty)
				{
					builder
						.AppendTabs(tabs)
						.Append("return !!Parms.")
						.Append(returnProperty.SourceName)
						.Append(';').Append(endl);
				}
				else
				{
					builder
						.AppendTabs(tabs)
						.Append("return Parms.")
						.Append(returnProperty.SourceName)
						.Append(';').Append(endl);
				}
			}

			--tabs;
			builder.AppendTabs(tabs).Append('}').Append(endl);
			return builder;
		}

		protected static StringBuilder AppendParameters(StringBuilder builder, UhtFunction function, UhtPropertyTextType textType, string? extraParameter, bool skipParameterName)
		{
			bool needsSeperator = false;

			builder.Append('(');

			if (extraParameter != null)
			{
				builder.Append(extraParameter);
				needsSeperator = true;
			}

			foreach (UhtProperty parameter in function.ParameterProperties.Span)
			{
				if (needsSeperator)
				{
					builder.Append(", ");
				}
				else
				{
					needsSeperator = true;
				}
				builder.AppendFullDecl(parameter, textType, skipParameterName);
			}

			builder.Append(')');
			return builder;
		}

		protected static bool IsRpcFunction(UhtFunction function)
		{
			return function.FunctionFlags.HasAnyFlags(EFunctionFlags.Native) &&
				!function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.CustomThunk);
		}

		protected static bool IsRpcFunction(UhtFunction function, bool editorOnly)
		{
			return IsRpcFunction(function) && function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly) == editorOnly;
		}

		/// <summary>
		/// Determines whether the glue version of the specified native function should be exported.
		/// </summary>
		/// <param name="function">The function to check</param>
		/// <returns>True if the glue version of the function should be exported.</returns>
		protected static bool ShouldExportFunction(UhtFunction function)
		{
			// export any script stubs for native functions declared in interface classes
			bool isBlueprintNativeEvent = function.FunctionFlags.HasAllFlags(EFunctionFlags.BlueprintEvent | EFunctionFlags.Native);
			if (!isBlueprintNativeEvent)
			{
				if (function.Outer != null)
				{
					if (function.Outer is UhtClass classObj)
					{
						if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
						{
							return true;
						}
					}
				}
			}

			// always export if the function is static
			if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.Static))
			{
				return true;
			}

			// don't export the function if this is not the original declaration and there is
			// at least one parent version of the function that is declared native
			for (UhtFunction? parentFunction = function.SuperFunction; parentFunction != null; parentFunction = parentFunction.SuperFunction)
			{
				if (parentFunction.FunctionFlags.HasAnyFlags(EFunctionFlags.Native))
				{
					return false;
				}
			}
			return true;
		}

		protected static string GetDelegateFunctionExportName(UhtFunction function)
		{
			const string DelegatePrefix = "delegate";

			if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.Delegate))
			{
				throw new UhtIceException("Attempt to export a function that isn't a delegate as a delegate");
			}
			if (!function.MarshalAndCallName.StartsWith(DelegatePrefix, StringComparison.Ordinal))
			{
				throw new UhtIceException("Marshal and call name must begin with 'delegate'");
			}
			return $"F{function.MarshalAndCallName[DelegatePrefix.Length..]}_DelegateWrapper";
		}

		protected static string GetDelegateFunctionExtraParameter(UhtFunction function)
		{
			string returnType = function.FunctionFlags.HasAnyFlags(EFunctionFlags.MulticastDelegate) ? "FMulticastScriptDelegate" : "FScriptDelegate";
			return $"const {returnType}& {function.SourceName[1..]}";
		}
		#endregion

		#region Field notify support
		protected static bool NeedFieldNotifyCodeGen(UhtClass classObj)
		{
			return
				!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasCustomFieldNotify) &&
				classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasFieldNotify);
		}

		protected static void GetFieldNotifyStats(UhtClass classObj, out bool hasProperties, out bool hasFunctions, out bool hasEditorFields, out bool allEditorFields)
		{
			// Scan the children to see what we have
			hasProperties = false;
			hasFunctions = false;
			hasEditorFields = false;
			allEditorFields = true;
			foreach (UhtType type in classObj.Children)
			{
				if (type is UhtProperty property)
				{
					if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.FieldNotify))
					{
						hasProperties = true;
						hasEditorFields |= property.IsEditorOnlyProperty;
						allEditorFields &= property.IsEditorOnlyProperty;
					}
				}
				else if (type is UhtFunction function)
				{
					if (function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.FieldNotify))
					{
						hasFunctions = true;
						hasEditorFields |= function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly);
						allEditorFields &= function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly);
					}
				}
			}

			// If we have no editor fields, then by definition, all fields can't be editor fields
			allEditorFields &= hasEditorFields;
		}

		protected static StringBuilder AppendFieldNotify(StringBuilder builder, UhtClass classObj,
			bool hasProperties, bool hasFunctions, bool hasEditorFields, bool allEditorFields,
			bool includeEditorOnlyFields, bool appendDefine, Action<StringBuilder, UhtClass, string> appendAction)
		{
			if (hasProperties && !allEditorFields)
			{
				foreach (UhtType child in classObj.Children)
				{
					if (child is UhtProperty property)
					{
						if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.FieldNotify) && !property.IsEditorOnlyProperty)
						{
							appendAction(builder, classObj, property.SourceName);
						}
					}
				}
			}

			if (hasFunctions && !allEditorFields)
			{
				foreach (UhtType child in classObj.Children)
				{
					if (child is UhtFunction function)
					{
						if (function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.FieldNotify) && !function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly))
						{
							appendAction(builder, classObj, function.CppImplName);
						}
					}
				}
			}

			if (hasEditorFields && includeEditorOnlyFields)
			{
				if (!allEditorFields && appendDefine)
				{
					builder.Append("#if WITH_EDITORONLY_DATA\r\n");
				}

				if (hasProperties)
				{
					foreach (UhtType child in classObj.Children)
					{
						if (child is UhtProperty property)
						{
							if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.FieldNotify) && property.IsEditorOnlyProperty)
							{
								appendAction(builder, classObj, property.SourceName);
							}
						}
					}
				}

				if (hasFunctions)
				{
					foreach (UhtType child in classObj.Children)
					{
						if (child is UhtFunction function)
						{
							if (function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.FieldNotify) && function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly))
							{
								appendAction(builder, classObj, function.CppImplName);
							}
						}
					}
				}

				if (!allEditorFields && appendDefine)
				{
					builder.Append("#endif // WITH_EDITORONLY_DATA\r\n");
				}
			}
			return builder;
		}
		#endregion
	}

	internal static class UhtHaederCodeGeneratorStringBuilderExtensions
	{
		public static StringBuilder AppendMacroName(this StringBuilder builder, string fileId, int lineNumber, string macroSuffix)
		{
			return builder.Append(fileId).Append('_').Append(lineNumber).Append('_').Append(macroSuffix);
		}

		public static StringBuilder AppendMacroName(this StringBuilder builder, UhtHeaderCodeGenerator generator, int lineNumber, string macroSuffix)
		{
			return builder.AppendMacroName(generator.FileId, lineNumber, macroSuffix);
		}

		public static StringBuilder AppendMacroName(this StringBuilder builder, UhtHeaderCodeGenerator generator, UhtClass classObj, string macroSuffix)
		{
			return builder.AppendMacroName(generator, classObj.GeneratedBodyLineNumber, macroSuffix);
		}

		public static StringBuilder AppendMacroName(this StringBuilder builder, UhtHeaderCodeGenerator generator, UhtScriptStruct scriptStruct, string macroSuffix)
		{
			return builder.AppendMacroName(generator, scriptStruct.MacroDeclaredLineNumber, macroSuffix);
		}

		public static StringBuilder AppendMacroName(this StringBuilder builder, UhtHeaderCodeGenerator generator, UhtFunction function, string macroSuffix)
		{
			return builder.AppendMacroName(generator, function.MacroLineNumber, macroSuffix);
		}
	}
}
