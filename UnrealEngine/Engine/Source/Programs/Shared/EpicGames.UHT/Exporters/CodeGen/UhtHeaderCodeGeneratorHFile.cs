// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	internal class UhtHeaderCodeGeneratorHFile
		: UhtHeaderCodeGenerator
	{
		public static string RigVMExecuteContextDeclaration = "FRigVMExtendedExecuteContext& RigVMExecuteContext";
		public static string RigVMExecuteContextPublicDeclaration = "const FRigVMExecuteContext& RigVMExecuteContext";

		/// <summary>
		/// Construct an instance of this generator object
		/// </summary>
		/// <param name="codeGenerator">The base code generator</param>
		/// <param name="package">Package being generated</param>
		/// <param name="headerFile">Header file being generated</param>
		public UhtHeaderCodeGeneratorHFile(UhtCodeGenerator codeGenerator, UhtPackage package, UhtHeaderFile headerFile)
			: base(codeGenerator, package, headerFile)
		{
		}

		/// <summary>
		/// For a given UE header file, generated the generated H file
		/// </summary>
		/// <param name="factory">Requesting factory</param>
		public void Generate(IUhtExportFactory factory)
		{
			ref UhtCodeGenerator.HeaderInfo headerInfo = ref HeaderInfos[HeaderFile.HeaderFileTypeIndex];
			{
				using BorrowStringBuilder borrower = new(StringBuilderCache.Big);
				StringBuilder builder = borrower.StringBuilder;

				builder.Append(HeaderCopyright);
				builder.Append("// IWYU pragma: private, include \"").Append(this.HeaderFile.IncludeFilePath).Append("\"\r\n");
				builder.Append("#include \"UObject/ObjectMacros.h\"\r\n");
				builder.Append("#include \"UObject/ScriptMacros.h\"\r\n");
				if (headerInfo.NeedsPushModelHeaders)
				{
					builder.Append("#include \"Net/Core/PushModel/PushModelMacros.h\"\r\n");
				}
				builder.Append("\r\n");
				builder.Append(DisableDeprecationWarnings).Append("\r\n");

				string strippedName = Path.GetFileNameWithoutExtension(HeaderFile.FilePath);
				string defineName = $"{Package.ShortName.ToString().ToUpper()}_{strippedName}_generated_h";

				if (HeaderFile.References.ForwardDeclarations.Count > 0)
				{
					string[] sorted = new string[HeaderFile.References.ForwardDeclarations.Count];
					int index = 0;
					foreach (string forwardDeclaration in HeaderFile.References.ForwardDeclarations)
					{
						sorted[index++] = forwardDeclaration;
					}
					Array.Sort(sorted, StringComparerUE.OrdinalIgnoreCase);
					foreach (string forwardDeclaration in sorted)
					{
						builder.Append(forwardDeclaration).Append("\r\n");
					}
				}

				builder.Append("#ifdef ").Append(defineName).Append("\r\n");
				builder.Append("#error \"").Append(strippedName).Append(".generated.h already included, missing '#pragma once' in ").Append(strippedName).Append(".h\"\r\n");
				builder.Append("#endif\r\n");
				builder.Append("#define ").Append(defineName).Append("\r\n");
				builder.Append("\r\n");

				foreach (UhtField field in HeaderFile.References.ExportTypes)
				{
					if (field is UhtEnum enumObj)
					{
					}
					else if (field is UhtScriptStruct scriptStruct)
					{
						if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
						{
							AppendScriptStruct(builder, scriptStruct);
						}
					}
					else if (field is UhtFunction function)
					{
						AppendDelegate(builder, function);
					}
					else if (field is UhtClass classObj)
					{
						if (!classObj.ClassFlags.HasAnyFlags(EClassFlags.Intrinsic))
						{
							AppendClass(builder, classObj);
						}
					}
				}

				builder.Append("#undef CURRENT_FILE_ID\r\n");
				builder.Append("#define CURRENT_FILE_ID ").Append(headerInfo.FileId).Append("\r\n\r\n\r\n");

				foreach (UhtField field in HeaderFile.References.ExportTypes)
				{
					if (field is UhtEnum enumObject)
					{
						AppendEnum(builder, enumObject);
					}
				}

				builder.Append(EnableDeprecationWarnings).Append("\r\n");

				if (SaveExportedHeaders)
				{
					string headerFilePath = factory.MakePath(HeaderFile, ".generated.h");
					factory.CommitOutput(headerFilePath, builder);
				}
			}
		}

		private StringBuilder AppendScriptStruct(StringBuilder builder, UhtScriptStruct scriptStruct)
		{
			if (scriptStruct.RigVMStructInfo != null)
			{
				builder.Append("\r\n");
				foreach (UhtRigVMMethodInfo methodInfo in scriptStruct.RigVMStructInfo.Methods)
				{
					builder.Append("#define ").Append(scriptStruct.SourceName).Append('_').Append(methodInfo.Name).Append("() \\\r\n");
					builder.Append('\t').Append(methodInfo.ReturnType).Append(' ').Append(scriptStruct.SourceName).Append("::Static").Append(methodInfo.Name).Append("( \\\r\n");
					builder.Append("\t\t").Append(RigVMExecuteContextPublicDeclaration);
					builder.AppendParameterDecls(scriptStruct.RigVMStructInfo.Members, true, ", \\\r\n\t\t", true, false);
					builder.AppendParameterDecls(methodInfo.Parameters, true, ", \\\r\n\t\t", false, false);
					builder.Append(" \\\r\n");
					builder.Append("\t)\r\n");
				}
				builder.Append("\r\n");
			}

			if (scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.Native))
			{
				using (UhtMacroCreator macro = new(builder, this, scriptStruct, GeneratedBodyMacroSuffix))
				{
					builder.Append("\tfriend struct Z_Construct_UScriptStruct_").Append(scriptStruct.SourceName).Append("_Statics; \\\r\n");
					builder.Append('\t');
					if (!scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.RequiredAPI))
					{
						builder.Append(PackageApi);
					}
					builder.Append("static class UScriptStruct* StaticStruct(); \\\r\n");

					// if we have RigVM methods on this struct we need to 
					// declare the static method as well as the stub method
					if (scriptStruct.RigVMStructInfo != null)
					{
						foreach (UhtRigVMMethodInfo methodInfo in scriptStruct.RigVMStructInfo.Methods)
						{
							builder.Append("\tstatic ").Append(methodInfo.ReturnType).Append(" Static").Append(methodInfo.Name).Append("( \\\r\n");
							builder.Append("\t\t").Append(RigVMExecuteContextPublicDeclaration);
							builder.AppendParameterDecls(scriptStruct.RigVMStructInfo.Members, true, ", \\\r\n\t\t", true, false);
							builder.AppendParameterDecls(methodInfo.Parameters, true, ", \\\r\n\t\t", false, false);
							builder.Append(" \\\r\n");
							builder.Append("\t); \\\r\n");

							builder.Append("\tFORCEINLINE_DEBUGGABLE static ").Append(methodInfo.ReturnType).Append(" RigVM").Append(methodInfo.Name).Append("( \\\r\n");
							builder.Append("\t\t").Append(RigVMExecuteContextDeclaration).Append(", \\\r\n");
							builder.Append("\t\tFRigVMMemoryHandleArray RigVMMemoryHandles \\\r\n");
							builder.Append("\t) \\\r\n");
							builder.Append("\t{ \\\r\n");

							// implement inline stub method body
							if (methodInfo.Parameters.Count > 0)
							{
								for (int parameterIndex = 0; parameterIndex < methodInfo.Parameters.Count; parameterIndex++)
								{
									UhtRigVMParameter parameter = methodInfo.Parameters[parameterIndex];
									builder.Append("\t\t").Append(parameter.Declaration()).Append(" = *(").Append(parameter.TypeNoRef())
										.Append("*)RigVMExecuteContext.OpaqueArguments[").Append(parameterIndex).Append("]; \\\r\n");
								}
								builder.Append("\t\t \\\r\n");
							}

							if (scriptStruct.RigVMStructInfo.Members.Count > 0)
							{
								int operandIndex = 0;
								foreach (UhtRigVMParameter parameter in scriptStruct.RigVMStructInfo.Members)
								{
									string paramTypeOriginal = parameter.TypeOriginal(true);
									string paramNameOriginal = parameter.NameOriginal(false);
									string additionalParameters = String.Empty;
									if (!parameter.Input && !parameter.Output && !parameter.Singleton)
									{
										additionalParameters = ", RigVMExecuteContext.GetSlice().GetIndex()";
									}

									if (parameter.IsArray)
									{
										string extendedType = parameter.ExtendedType();
										builder
											.Append("\t\tTArray")
											.Append(extendedType)
											.Append("& ")
											.Append(paramNameOriginal)
											.Append(" = *(TArray")
											.Append(extendedType)
											.Append("*)RigVMMemoryHandles[")
											.Append(operandIndex)
											.Append("].GetData(false")
											.Append(additionalParameters)
											.Append("); \\\r\n");
										operandIndex++;
									}
									else
									{
										string variableType = parameter.TypeVariableRef(true);
										string extractedType = parameter.TypeOriginal();

										builder
											.Append("\t\t")
											.Append(variableType)
											.Append(' ')
											.Append(paramNameOriginal)
											.Append(" = *(")
											.Append(extractedType)
											.Append("*)RigVMMemoryHandles[")
											.Append(operandIndex)
											.Append("].GetData(false")
											.Append(additionalParameters)
											.Append("); \\\r\n");
										operandIndex++;
									}
								}
								builder.Append("\t\t \\\r\n");
							}

							builder.Append("\t\t").Append(methodInfo.ReturnPrefix()).Append("Static").Append(methodInfo.Name).Append("( \\\r\n");
							builder.Append("\t\t\tRigVMExecuteContext.PublicData");
							builder.AppendParameterNames(scriptStruct.RigVMStructInfo.Members, true, ", \\\r\n\t\t\t", false);
							builder.AppendParameterNames(methodInfo.Parameters, true, ", \\\r\n\t\t\t");
							builder.Append(" \\\r\n");
							builder.Append("\t\t); \\\r\n");
							builder.Append("\t} \\\r\n");
						}
					}

					if (scriptStruct.SuperScriptStruct != null)
					{
						builder.Append("\ttypedef ").Append(scriptStruct.SuperScriptStruct.SourceName).Append(" Super; \\\r\n");
					}
					UhtProperty? FastArrayProperty = ObjectInfos[scriptStruct.ObjectTypeIndex].FastArrayProperty;
					if (FastArrayProperty != null)
					{
						builder
							.Append("\tUE_NET_DECLARE_FASTARRAY(")
							.Append(scriptStruct.SourceName)
							.Append(", ")
							.Append(FastArrayProperty.SourceName)
							.Append(", ");
						if (!scriptStruct.ScriptStructFlags.HasAnyFlags(EStructFlags.RequiredAPI))
						{
							builder.Append(PackageApi);
						}
						builder.Append("); \\\r\n");
					}
				}

				// Forward declare the StaticStruct specialization in the header
				builder.Append("template<> ").Append(PackageApi).Append("UScriptStruct* StaticStruct<struct ").Append(scriptStruct.SourceName).Append(">();\r\n");
				builder.Append("\r\n");
			}
			return builder;
		}

		private StringBuilder AppendEnum(StringBuilder builder, UhtEnum enumObj)
		{
			// Export FOREACH macro
			builder.Append("#define FOREACH_ENUM_").Append(enumObj.EngineName.ToUpper()).Append("(op) ");
			bool hasExistingMax = EnumHasExistingMax(enumObj);
			long maxEnumValue = hasExistingMax ? GetMaxEnumValue(enumObj) : 0;
			foreach (UhtEnumValue enumValue in enumObj.EnumValues)
			{
				if (!hasExistingMax || enumValue.Value != maxEnumValue)
				{
					builder.Append("\\\r\n\top(").Append(enumValue.Name).Append(") ");
				}
			}
			builder.Append("\r\n");

			if (enumObj.CppForm == UhtEnumCppForm.EnumClass)
			{
				builder.Append("\r\n");
				builder.Append("enum class ").Append(enumObj.CppType);
				if (enumObj.UnderlyingType != UhtEnumUnderlyingType.Unspecified)
				{
					builder.Append(" : ").Append(enumObj.UnderlyingType.ToString());
				}
				builder.Append(";\r\n");
				
				// Add TIsUEnumClass typetraits
				builder.Append("template<> struct TIsUEnumClass<").Append(enumObj.CppType).Append("> { enum { Value = true }; };\r\n");

				// Forward declare the StaticEnum<> specialization for enum classes
				builder.Append("template<> ").Append(PackageApi).Append("UEnum* StaticEnum<").Append(enumObj.CppType).Append(">();\r\n");
				builder.Append("\r\n");
			}

			return builder;
		}

		private StringBuilder AppendDelegate(StringBuilder builder, UhtFunction function)
		{
			using (UhtMacroCreator macro = new(builder, this, function, DelegateMacroSuffix))
			{
				int tabs = 0;
				string strippedFunctionName = function.StrippedFunctionName;
				string exportFunctionName = GetDelegateFunctionExportName(function);
				string extraParameter = GetDelegateFunctionExtraParameter(function);

				AppendEventParameter(builder, function, strippedFunctionName, UhtPropertyTextType.EventParameterMember, true, tabs, " \\\r\n");
				builder.Append("static ");
				AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.EventFunctionArgOrRetVal, true, exportFunctionName, extraParameter, UhtFunctionExportFlags.Inline, " \\\r\n");
				AppendEventFunctionPrologue(builder, function, strippedFunctionName, tabs, " \\\r\n");
				builder
					.Append('\t')
					.Append(strippedFunctionName)
					.Append('.')
					.Append(function.FunctionFlags.HasAnyFlags(EFunctionFlags.MulticastDelegate) ? "ProcessMulticastDelegate" : "ProcessDelegate")
					.Append("<UObject>(")
					.Append(function.Children.Count > 0 ? "&Parms" : "NULL")
					.Append("); \\\r\n");
				AppendEventFunctionEpilogue(builder, function, tabs, " \\\r\n");
			}
			return builder;
		}

		private StringBuilder AppendClass(StringBuilder builder, UhtClass classObj)
		{
			string api = classObj.ClassFlags.HasAnyFlags(EClassFlags.MinimalAPI) ? PackageApi : "NO_API ";
			UhtClass? nativeInterface = ObjectInfos[classObj.ObjectTypeIndex].NativeInterface;

			// Write the spare declarations
			AppendSparseDeclarations(builder, classObj);

			// Collect the functions in reversed order
			List<UhtFunction> reversedFunctions = new(classObj.Functions);
			reversedFunctions.Reverse();

			// Check to see if we have any RPC functions for the editor
			bool hasEditorRpc = reversedFunctions.Any(x => IsRpcFunction(x, true));

			// Output the RPC methods
			AppendRpcFunctions(builder, classObj, reversedFunctions, false);
			if (hasEditorRpc)
			{
				AppendRpcFunctions(builder, classObj, reversedFunctions, true);
			}

			// Output property accessors
			AppendPropertyAccessors(builder, classObj);

			// Collect the callback function and sort by name to make the order stable
			List<UhtFunction> callbackFunctions = new(classObj.Functions.Where(x => x.FunctionFlags.HasAnyFlags(EFunctionFlags.Event) && x.SuperFunction == null));
			callbackFunctions.Sort((x, y) => StringComparerUE.OrdinalIgnoreCase.Compare(x.EngineName, y.EngineName));

			// This is now done in.gen.cpp - Generate the callback parameter structures
			//AppendCallbackParametersDecls(builder, classObj, callbackFunctions);

			// Generate the RPC wrappers for the callbacks
			AppendCallbackRpcWrapperDecls(builder, classObj, callbackFunctions);

			// Only write out adapters if the user has provided one or the other of the Serialize overloads
			if (classObj.SerializerArchiveType != UhtSerializerArchiveType.None && classObj.SerializerArchiveType != UhtSerializerArchiveType.All)
			{
				AppendSerializer(builder, classObj, api, UhtSerializerArchiveType.Archive, "DECLARE_FARCHIVE_SERIALIZER");
				AppendSerializer(builder, classObj, api, UhtSerializerArchiveType.StructuredArchiveRecord, "DECLARE_FSTRUCTUREDARCHIVE_SERIALIZER");
			}

			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
			{
				AppendStandardConstructors(builder, classObj, api);
				AppendEnhancedConstructors(builder, classObj, api);

				using (UhtMacroCreator macro = new(builder, this, classObj, GeneratedUInterfaceBodyMacroSuffix))
				{
					AppendCommonGeneratedBody(builder, classObj, api);
				}

				using (UhtMacroCreator macro = new(builder, this, classObj, GeneratedBodyLegacyMacroSuffix))
				{
					builder.Append('\t');
					AppendGeneratedMacroDeprecationWarning(builder, "GENERATED_UINTERFACE_BODY");
					builder.Append('\t').Append(DisableDeprecationWarnings).Append(" \\\r\n");
					builder.Append('\t').AppendMacroName(this, classObj, GeneratedUInterfaceBodyMacroSuffix).Append(" \\\r\n");
					builder.Append('\t').AppendMacroName(this, classObj, StandardConstructorsMacroSuffix).Append(" \\\r\n");
					builder.Append('\t').Append(EnableDeprecationWarnings).Append(" \\\r\n");
				}

				using (UhtMacroCreator macro = new(builder, this, classObj, GeneratedBodyMacroSuffix))
				{
					builder.Append('\t').Append(DisableDeprecationWarnings).Append(" \\\r\n");
					builder.Append('\t').AppendMacroName(this, classObj, GeneratedUInterfaceBodyMacroSuffix).Append(" \\\r\n");
					builder.Append('\t').AppendMacroName(this, classObj, EnchancedConstructorsMacroSuffix).Append(" \\\r\n");
					AppendAccessSpecifier(builder, classObj);
					builder.Append(" \\\r\n");
					builder.Append('\t').Append(EnableDeprecationWarnings).Append(" \\\r\n");
				}

				using (UhtMacroCreator macro = new(builder, this, classObj, InClassIInterfaceNoPureDeclsMacroSuffix))
				{
					AppendInClassIInterface(builder, classObj, callbackFunctions, api);
				}

				using (UhtMacroCreator macro = new(builder, this, classObj, InClassIInterfaceMacroSuffix))
				{
					AppendInClassIInterface(builder, classObj, callbackFunctions, api);
				}
			}
			else
			{
				using (UhtMacroCreator macro = new(builder, this, classObj, InClassNoPureDeclsMacroSuffix))
				{
					AppendClassGeneratedBody(builder, classObj, api);
				}

				using (UhtMacroCreator macro = new(builder, this, classObj, InClassMacroSuffix))
				{
					AppendClassGeneratedBody(builder, classObj, api);
				}

				AppendStandardConstructors(builder, classObj, api);
				AppendEnhancedConstructors(builder, classObj, api);
				AppendFieldNotify(builder, classObj);
			}

			using (UhtMacroCreator macro = new(builder, this, classObj.PrologLineNumber, PrologMacroSuffix))
			{
				//if (callbackFunctions.Count > 0)
				//{
				//	builder.Append('\t').AppendMacroName(this, classObj, EventParamsMacroSuffix).Append(" \\\r\n");
				//}
			}

			bool hasCallbacks = callbackFunctions.Count > 0;
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
			{
				if (nativeInterface != null)
				{
					AppendGeneratedBodyMacroBlock(builder, classObj, nativeInterface, true, hasEditorRpc, hasCallbacks, null);
					AppendGeneratedBodyMacroBlock(builder, classObj, nativeInterface, false, hasEditorRpc, hasCallbacks, null);
				}
			}
			else
			{
				AppendGeneratedBodyMacroBlock(builder, classObj, classObj, true, hasEditorRpc, hasCallbacks, "GENERATED_UCLASS_BODY");
				AppendGeneratedBodyMacroBlock(builder, classObj, classObj, false, hasEditorRpc, hasCallbacks, null);
			}

			// Forward declare the StaticClass specialization in the header
			builder.Append("template<> ").Append(PackageApi).Append("UClass* StaticClass<class ").Append(classObj.SourceName).Append(">();\r\n");
			builder.Append("\r\n");
			return builder;
		}

		private StringBuilder AppendFieldNotify(StringBuilder builder, UhtClass classObj)
		{
			if (!NeedFieldNotifyCodeGen(classObj))
			{
				return builder;
			}

			// Scan the children to see what we have
			GetFieldNotifyStats(classObj, out bool hasProperties, out bool hasFunctions, out bool hasEditorFields, out bool allEditorFields);

			// If we only have editor fields or no editor fields, then we only emit one block
			if (hasEditorFields)
			{
				builder.Append("#if WITH_EDITORONLY_DATA\r\n");
				using (UhtMacroCreator macro = new(builder, this, classObj, FieldNotifyMacroSuffix))
				{
					AppendFieldNotify(builder, classObj, hasProperties, hasFunctions, hasEditorFields, allEditorFields, true);
				}
				builder.Append("#else //WITH_EDITORONLY_DATA\r\n");
				using (UhtMacroCreator macro = new(builder, this, classObj, FieldNotifyMacroSuffix))
				{
					AppendFieldNotify(builder, classObj, hasProperties, hasFunctions, hasEditorFields, allEditorFields, false);
				}
				builder.Append("#endif // WITH_EDITORONLY_DATA\r\n");
			}
			else
			{
				using UhtMacroCreator macro = new(builder, this, classObj, FieldNotifyMacroSuffix);
				AppendFieldNotify(builder, classObj, hasProperties, hasFunctions, hasEditorFields, allEditorFields, false);
			}
			return builder;
		}

		private StringBuilder AppendFieldNotify(StringBuilder builder, UhtClass classObj,
			bool hasProperties, bool hasFunctions, bool hasEditorFields, bool allEditorFields,
			bool includeEditorOnlyFields)
		{
			builder.Append("\tUE_FIELD_NOTIFICATION_DECLARE_CLASS_DESCRIPTOR_BEGIN(").Append(PackageApi).Append(") \\\r\n");

			//UE_FIELD_NOTIFICATION_DECLARE_FIELD
			AppendFieldNotify(builder, classObj, hasProperties, hasFunctions, hasEditorFields, allEditorFields,
				includeEditorOnlyFields, false, (StringBuilder builder, UhtClass classObj, string name) =>
			{
				builder.Append("\tUE_FIELD_NOTIFICATION_DECLARE_FIELD(").Append(name).Append(") \\\r\n");
			});

			//UE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD
			bool isFirst = true;
			AppendFieldNotify(builder, classObj, hasProperties, hasFunctions, hasEditorFields, allEditorFields,
				includeEditorOnlyFields, false, (StringBuilder builder, UhtClass classObj, string name) =>
			{
				if (isFirst)
				{
					isFirst = false;
					builder.Append("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD_BEGIN(").Append(name).Append(") \\\r\n");
				}
				else
				{
					builder.Append("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD(").Append(name).Append(") \\\r\n");
				}
			});

			builder.Append("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD_END() \\\r\n");
			builder.Append("\tUE_FIELD_NOTIFICATION_DECLARE_CLASS_DESCRIPTOR_END(); \\\r\n");
			return builder;
		}

		private StringBuilder AppendSparseDeclarations(StringBuilder builder, UhtClass classObj)
		{
			// Format the sparse data
			using (UhtMacroCreator macro = new(builder, this, classObj, SparseDataMacroSuffix))
			{
				string[]? sparseDataTypes = classObj.MetaData.GetStringArray(UhtNames.SparseClassDataTypes);
				if (sparseDataTypes != null)
				{
					string[]? baseSparseDataTypes = classObj.SuperClass?.MetaData.GetStringArray(UhtNames.SparseClassDataTypes);
					if (baseSparseDataTypes == null || !Enumerable.SequenceEqual(sparseDataTypes, baseSparseDataTypes))
					{
						string api = classObj.ClassFlags.HasAnyFlags(EClassFlags.MinimalAPI) ? PackageApi : "";

						foreach (string sparseDataType in sparseDataTypes)
						{
							builder.Append(api).Append('F').Append(sparseDataType).Append("* Get").Append(sparseDataType).Append("(); \\\r\n");
							builder.Append(api).Append('F').Append(sparseDataType).Append("* Get").Append(sparseDataType).Append("() const; \\\r\n");
							builder.Append(api).Append("const F").Append(sparseDataType).Append("* Get").Append(sparseDataType).Append("(EGetSparseClassDataMethod GetMethod) const; \\\r\n");
							builder.Append(api).Append("static UScriptStruct* StaticGet").Append(sparseDataType).Append("ScriptStruct(); \\\r\n");
						}
					}

					foreach (string sparseDataType in sparseDataTypes)
					{
						UhtScriptStruct? sparseScriptStruct = classObj.FindType(UhtFindOptions.EngineName | UhtFindOptions.ScriptStruct | UhtFindOptions.NoSelf, sparseDataType) as UhtScriptStruct;
						while (sparseScriptStruct != null)
						{
							foreach (UhtProperty sparseProperty in sparseScriptStruct.Properties)
							{
								if (!sparseProperty.MetaData.ContainsKey(UhtNames.NoGetter))
								{
									string propertyName = sparseProperty.SourceName;
									string cleanPropertyName = propertyName;
									if (sparseProperty is UhtBoolProperty && propertyName.StartsWith("b", StringComparison.Ordinal))
									{
										cleanPropertyName = propertyName[1..];
									}

									bool getByRef = sparseProperty.MetaData.ContainsKey(UhtNames.GetByRef);

									if (getByRef)
									{
										builder.Append("const ").AppendSparse(sparseProperty).Append("& Get").Append(cleanPropertyName).Append("() \\\r\n");
									}
									else
									{
										builder.AppendSparse(sparseProperty).Append(" Get").Append(cleanPropertyName).Append("() \\\r\n");
									}
									builder.Append("{ \\\r\n");
									builder.Append("\treturn Get").Append(sparseDataType).Append("()->").Append(propertyName).Append("; \\\r\n");
									builder.Append("} \\\r\n");

									if (getByRef)
									{
										builder.Append("const ").AppendSparse(sparseProperty).Append("& Get").Append(cleanPropertyName).Append("() const \\\r\n");
									}
									else
									{
										builder.AppendSparse(sparseProperty).Append(" Get").Append(cleanPropertyName).Append("() const \\\r\n");
									}
									builder.Append("{ \\\r\n");
									builder.Append("\treturn Get").Append(sparseDataType).Append("()->").Append(propertyName).Append("; \\\r\n");
									builder.Append("} \\\r\n");
								}
							}

							sparseScriptStruct = sparseScriptStruct.SuperScriptStruct;
						}
					}
				}
			}
			return builder;
		}

		private StringBuilder AppendRpcFunctions(StringBuilder builder, UhtClass classObj, List<UhtFunction> reversedFunctions, bool editorOnly)
		{
			builder.AppendBeginEditorOnlyGuard(editorOnly);

			using (UhtMacroCreator macro = new(builder, this, classObj, editorOnly ? EditorOnlyRpcWrappersMacroSuffix : RpcWrappersMacroSuffix))
			{
				AppendAutogeneratedBlueprintFunctionDeclarations(builder, classObj, reversedFunctions, editorOnly);
				AppendRpcWrappers(builder, reversedFunctions, editorOnly);
			}

			using (UhtMacroCreator macro = new(builder, this, classObj, editorOnly ? EditorOnlyRpcWrappersNoPureDeclsMacroSuffix : RpcWrappersNoPureDeclsMacroSuffix))
			{
				if (classObj.GeneratedCodeVersion <= EGeneratedCodeVersion.V1)
				{
					AppendAutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared(builder, classObj, reversedFunctions, editorOnly);
				}
				AppendRpcWrappers(builder, reversedFunctions, editorOnly);
			}

			if (editorOnly)
			{
				builder.Append("#else\r\n");
				using (UhtMacroCreator macro = new(builder, this, classObj, editorOnly ? EditorOnlyRpcWrappersMacroSuffix : RpcWrappersMacroSuffix))
				{
				}

				using (UhtMacroCreator macro = new(builder, this, classObj, editorOnly ? EditorOnlyRpcWrappersNoPureDeclsMacroSuffix : RpcWrappersNoPureDeclsMacroSuffix))
				{
				}
				builder.AppendEndEditorOnlyGuard();
			}
			return builder;
		}

		private StringBuilder AppendPropertyAccessors(StringBuilder builder, UhtClass classObj)
		{
			using (UhtMacroCreator macro = new(builder, this, classObj, AccessorsMacroSuffix))
			{
				foreach (UhtType type in classObj.Children)
				{
					if (type is UhtProperty property)
					{
						if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.GetterFound))
						{
							builder.Append("static void ").AppendPropertyGetterWrapperName(property).Append("(const void* Object, void* OutValue); \\\r\n");
						}
						if (property.PropertyExportFlags.HasAnyFlags(UhtPropertyExportFlags.SetterFound))
						{
							builder.Append("static void ").AppendPropertySetterWrapperName(property).Append("(void* Object, const void* InValue); \\\r\n");
						}
					}
				}
			}
			return builder;
		}

		private StringBuilder AppendAutogeneratedBlueprintFunctionDeclarations(StringBuilder builder, UhtClass classObj, List<UhtFunction> reversedFunctions, bool editorOnly)
		{
			foreach (UhtFunction function in reversedFunctions)
			{
				if (!IsRpcFunction(function, editorOnly) || function.CppImplName == function.SourceName)
				{
					continue;
				}
				AppendNetValidateDeclaration(builder, classObj, function);
				AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.ClassFunctionArgOrRetVal, true, null, null, UhtFunctionExportFlags.None, "; \\\r\n");
			}
			return builder;
		}

		private StringBuilder AppendAutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared(StringBuilder builder, UhtClass classObj, List<UhtFunction> reversedFunctions, bool editorOnly)
		{
			foreach (UhtFunction function in reversedFunctions)
			{
				if (!IsRpcFunction(function, editorOnly) || function.CppImplName == function.SourceName)
				{
					continue;
				}
				if (!function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.ValidationImplFound))
				{
					AppendNetValidateDeclaration(builder, classObj, function);
				}
				if (!function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.ImplFound))
				{
					AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.ClassFunctionArgOrRetVal, true, null, null, UhtFunctionExportFlags.None, "; \\\r\n");
				}
			}
			return builder;
		}

		private StringBuilder AppendNetValidateDeclaration(StringBuilder builder, UhtClass classObj, UhtFunction function)
		{
			if (function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetValidate))
			{
				builder.Append('\t');

				if (!classObj.ClassFlags.HasAnyFlags(EClassFlags.RequiredAPI) && function.FunctionFlags.HasAnyFlags(EFunctionFlags.RequiredAPI))
				{
					builder.Append(PackageApi);
				}

				if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.Static) && !function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.Final))
				{
					builder.Append("virtual");
				}
				builder.Append(" bool ").Append(function.CppValidationImplName);
				AppendParameters(builder, function, UhtPropertyTextType.ClassFunctionArgOrRetVal, null, true);
				builder.Append("; \\\r\n");
			}
			return builder;
		}

		private static StringBuilder AppendRpcWrappers(StringBuilder builder, List<UhtFunction> reversedFunctions, bool editorOnly)
		{
			bool first = true;
			foreach (UhtFunction function in reversedFunctions)
			{
				if (!IsRpcFunction(function, editorOnly))
				{
					continue;
				}
				if (!ShouldExportFunction(function))
				{
					continue;
				}
				//COMPATIBILITY-TODO - Remove once we transition to C# version
				if (first)
				{
					builder.Append(" \\\r\n");
					first = false;
				}
				builder.Append("\tDECLARE_FUNCTION(").Append(function.UnMarshalAndCallName).Append("); \\\r\n");
			}
			return builder;
		}

		private StringBuilder AppendCallbackParametersDecls(StringBuilder builder, UhtClass classObj, List<UhtFunction> callbackFunctions)
		{
			if (callbackFunctions.Count > 0)
			{
				using UhtMacroCreator macro = new(builder, this, classObj, EventParamsMacroSuffix);
				foreach (UhtFunction function in callbackFunctions)
				{
					AppendEventParameter(builder, function, function.StrippedFunctionName, UhtPropertyTextType.EventParameterMember, true, 1, " \\\r\n");
				}
			}
			return builder;
		}

		private StringBuilder AppendCallbackRpcWrapperDecls(StringBuilder builder, UhtClass classObj, List<UhtFunction> callbackFunctions)
		{
			if (callbackFunctions.Count > 0)
			{
				using UhtMacroCreator macro = new(builder, this, classObj, CallbackWrappersMacroSuffix);
				foreach (UhtFunction function in callbackFunctions)
				{
					if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.NetResponse) &&
						function.EngineName != function.MarshalAndCallName)
					{
						AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.EventFunctionArgOrRetVal, true, null, null, UhtFunctionExportFlags.None, " \\\r\n");
						builder.Append(" \\\r\n");
					}
				}
			}
			return builder;
		}

		private StringBuilder AppendSerializer(StringBuilder builder, UhtClass classObj, string api, UhtSerializerArchiveType type, string declare)
		{
			if (!classObj.SerializerArchiveType.HasAnyFlags(type))
			{
				if (classObj.EnclosingDefine.Length != 0)
				{
					builder.Append("#if ").Append(classObj.EnclosingDefine).Append("\r\n");
				}
				AppendSerializerFunction(builder, classObj, api, declare);
				if (classObj.EnclosingDefine.Length != 0)
				{
					builder.Append("#else\r\n");
					using (UhtMacroCreator macro = new(builder, this, classObj, ArchiveSerializerMacroSuffix))
					{
					}
					builder.Append("#endif\r\n");
				}
			}
			return builder;
		}

		private StringBuilder AppendSerializerFunction(StringBuilder builder, UhtClass classObj, string api, string declare)
		{
			using (UhtMacroCreator macro = new(builder, this, classObj, ArchiveSerializerMacroSuffix))
			{
				builder.Append('\t').Append(declare).Append('(').Append(classObj.SourceName).Append(", ").Append(api[0..^1]).Append(") \\\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Generates standard constructor declarations
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private StringBuilder AppendStandardConstructors(StringBuilder builder, UhtClass classObj, string api)
		{
			using (UhtMacroCreator macro = new(builder, this, classObj, StandardConstructorsMacroSuffix))
			{
				if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasCustomConstructor))
				{
					builder.Append("\t/** Standard constructor, called after all reflected properties have been initialized */ \\\r\n");
					builder.Append('\t').Append(api).Append(classObj.SourceName).Append("(const FObjectInitializer& ObjectInitializer");
					if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasDefaultConstructor))
					{
						builder.Append(" = FObjectInitializer::Get()");
					}
					builder.Append("); \\\r\n");
				}
				if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Abstract))
				{
					builder.Append("\tDEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(").Append(classObj.SourceName).Append(") \\\r\n");
				}
				else
				{
					builder.Append("\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(").Append(classObj.SourceName).Append(") \\\r\n");
				}

				AppendVTableHelperCtorAndCaller(builder, classObj, api);
				AppendCopyConstructorDefinition(builder, classObj, api);
				AppendDestructorDefinition(builder, classObj, api);
			}
			return builder;
		}

		/// <summary>
		/// Generates enhanced constructor declaration.
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private StringBuilder AppendEnhancedConstructors(StringBuilder builder, UhtClass classObj, string api)
		{
			using (UhtMacroCreator macro = new(builder, this, classObj, EnchancedConstructorsMacroSuffix))
			{
				AppendConstructorDefinition(builder, classObj, api);
				AppendVTableHelperCtorAndCaller(builder, classObj, api);
				AppendDefaultConstructorCallDefinition(builder, classObj);
				AppendDestructorDefinition(builder, classObj, api);
			}
			return builder;
		}

		/// <summary>
		/// Generates vtable helper caller and eventual constructor body.
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private static StringBuilder AppendVTableHelperCtorAndCaller(StringBuilder builder, UhtClass classObj, string api)
		{
			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasCustomVTableHelperConstructor))
			{
				builder.Append("\tDECLARE_VTABLE_PTR_HELPER_CTOR(").Append(api[0..^1]).Append(", ").Append(classObj.SourceName).Append("); \\\r\n");
			}
			builder.Append("\tDEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(").Append(classObj.SourceName).Append("); \\\r\n");
			return builder;
		}

		/// <summary>
		/// Generates private copy-constructor declaration.
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private static StringBuilder AppendCopyConstructorDefinition(StringBuilder builder, UhtClass classObj, string api)
		{
			builder.Append("private: \\\r\n");
			builder.Append("\t/** Private move- and copy-constructors, should never be used */ \\\r\n");
			builder.Append('\t').Append(api).Append(classObj.SourceName).Append('(').Append(classObj.SourceName).Append("&&); \\\r\n");
			builder.Append('\t').Append(api).Append(classObj.SourceName).Append("(const ").Append(classObj.SourceName).Append("&); \\\r\n");
			builder.Append("public: \\\r\n");
			return builder;
		}

		/// <summary>
		/// Generates a destructor
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private static StringBuilder AppendDestructorDefinition(StringBuilder builder, UhtClass classObj, string api)
		{
			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasDestructor))
			{
				builder.Append('\t').Append(api).Append("virtual ~").Append(classObj.SourceName).Append("(); \\\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Type of constructor on the class regardless of explicit or generated.
		/// </summary>
		enum ConstructorType
		{
			ObjectInitializer,
			Default,
			ForbiddenDefault,
		}

		/// <summary>
		/// Return the type of constructor the class will have regardless of if one has been explicitly 
		/// declared or will be generated.
		/// </summary>
		/// <param name="classObj">Class in question</param>
		/// <returns>Constructor type</returns>
		private static ConstructorType GetConstructorType(UhtClass classObj)
		{
			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasConstructor))
			{

				// Assume super class has OI constructor, this may not always be true but we should always be able to check this.
				// In any case, it will default to old behavior before we even checked this.
				bool superClassObjectInitializerConstructorDeclared = true;
				UhtClass? superClass = classObj.SuperClass;
				if (superClass != null)
				{
					if (!superClass.HeaderFile.IsNoExportTypes)
					{
						superClassObjectInitializerConstructorDeclared = GetConstructorType(superClass) == ConstructorType.ObjectInitializer;
					}
				}

				if (superClassObjectInitializerConstructorDeclared)
				{
					return ConstructorType.ObjectInitializer;
				}
				else
				{
					return ConstructorType.Default;
				}
			}
			else
			{
				if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasObjectInitializerConstructor))
				{
					return ConstructorType.ObjectInitializer;
				}
				else if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasDefaultConstructor))
				{
					return ConstructorType.Default;
				}
				else
				{
					return ConstructorType.ForbiddenDefault;
				}
			}
		}

		/// <summary>
		/// Generates private copy-constructor declaration.
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private static StringBuilder AppendConstructorDefinition(StringBuilder builder, UhtClass classObj, string api)
		{
			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasConstructor))
			{
				builder.Append("\t/** Standard constructor, called after all reflected properties have been initialized */ \\\r\n");
				switch (GetConstructorType(classObj))
				{
					case ConstructorType.ObjectInitializer:
						builder.Append('\t').Append(api).Append(classObj.SourceName).Append("(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { }; \\\r\n");
						break;

					case ConstructorType.Default:
						builder.Append('\t').Append(api).Append(classObj.SourceName).Append("() { }; \\\r\n");
						break;
				}
			}
			AppendCopyConstructorDefinition(builder, classObj, api);
			return builder;
		}

		/// <summary>
		/// Generates constructor call definition
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <returns>Output builder</returns>
		private static StringBuilder AppendDefaultConstructorCallDefinition(StringBuilder builder, UhtClass classObj)
		{
			switch (GetConstructorType(classObj))
			{
				case ConstructorType.ObjectInitializer:
					if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Abstract))
					{
						builder.Append("\tDEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(").Append(classObj.SourceName).Append(") \\\r\n");
					}
					else
					{
						builder.Append("\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(").Append(classObj.SourceName).Append(") \\\r\n");
					}
					break;

				case ConstructorType.Default:
					if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Abstract))
					{
						builder.Append("\tDEFINE_ABSTRACT_DEFAULT_CONSTRUCTOR_CALL(").Append(classObj.SourceName).Append(") \\\r\n");
					}
					else
					{
						builder.Append("\tDEFINE_DEFAULT_CONSTRUCTOR_CALL(").Append(classObj.SourceName).Append(") \\\r\n");
					}
					break;

				case ConstructorType.ForbiddenDefault:
					builder.Append("\tDEFINE_FORBIDDEN_DEFAULT_CONSTRUCTOR_CALL(").Append(classObj.SourceName).Append(") \\\r\n");
					break;

				default:
					throw new UhtIceException("Unexpected constructor type");
			}
			return builder;
		}

		/// <summary>
		/// Generates generated body code for classes
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private StringBuilder AppendClassGeneratedBody(StringBuilder builder, UhtClass classObj, string api)
		{
			AppendCommonGeneratedBody(builder, classObj, api);

			// export the class's config name
			UhtClass? superClass = classObj.SuperClass;
			if (superClass != null && classObj.Config.Length > 0 && classObj.Config != superClass.Config)
			{
				builder.Append("\tstatic const TCHAR* StaticConfigName() {return TEXT(\"").Append(classObj.Config).Append("\");} \\\r\n \\\r\n");
			}

			// export implementation of _getUObject for classes that implement interfaces
			foreach (UhtStruct baseStruct in classObj.Bases)
			{
				if (baseStruct is UhtClass baseClass)
				{
					if (baseClass.ClassFlags.HasAnyFlags(EClassFlags.Interface))
					{
						builder.Append("\tvirtual UObject* _getUObject() const override { return const_cast<").Append(classObj.SourceName).Append("*>(this); } \\\r\n");
						break;
					}
				}
			}

			if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.SelfHasReplicatedProperties))
			{
				AppendReplicatedMacroData(builder, classObj, api);
			}

			return builder;
		}

		/// <summary>
		/// Generates standard generated body code for interfaces and non-interfaces
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class being exported</param>
		/// <param name="api">API text to be used</param>
		/// <returns>Output builder</returns>
		private StringBuilder AppendCommonGeneratedBody(StringBuilder builder, UhtClass classObj, string api)
		{
			// Export the class's native function registration.
			builder.Append("private: \\\r\n");
			builder.Append("\tstatic void StaticRegisterNatives").Append(classObj.SourceName).Append("(); \\\r\n");
			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.NoExport))
			{
				builder.Append("\tfriend struct ").Append(ObjectInfos[classObj.ObjectTypeIndex].RegisteredSingletonName).Append("_Statics; \\\r\n");
			}
			builder.Append("public: \\\r\n");

			UhtClass? superClass = classObj.SuperClass;
			bool castedClass = classObj.ClassCastFlags.HasAnyFlags(EClassCastFlags.AllFlags) && superClass != null && classObj.ClassCastFlags != superClass.ClassCastFlags;

			builder
				.Append("\tDECLARE_CLASS(")
				.Append(classObj)
				.Append(", ")
				.Append(superClass != null ? superClass.SourceName : "None")
				.Append(", COMPILED_IN_FLAGS(")
				.Append(classObj.ClassFlags.HasAnyFlags(EClassFlags.Abstract) ? "CLASS_Abstract" : "0");

			AppendClassFlags(builder, classObj);
			builder.Append("), ");
			if (castedClass)
			{
				builder
					.Append("CASTCLASS_")
					.Append(classObj.SourceName);
			}
			else
			{
				builder.Append("CASTCLASS_None");
			}
			builder.Append(", TEXT(\"").Append(Package.SourceName).Append("\"), ").Append(api[0..^1]).Append(") \\\r\n");

			builder.Append("\tDECLARE_SERIALIZER(").Append(classObj.SourceName).Append(") \\\r\n");

			// Add the serialization function declaration if we generated one
			if (classObj.SerializerArchiveType != UhtSerializerArchiveType.None && classObj.SerializerArchiveType != UhtSerializerArchiveType.All)
			{
				builder.Append('\t').AppendMacroName(this, classObj, ArchiveSerializerMacroSuffix).Append(" \\\r\n");
			}

			if (superClass != null && classObj.ClassWithin != superClass.ClassWithin)
			{
				builder.Append("\tDECLARE_WITHIN(").Append(classObj.ClassWithin.SourceName).Append(") \\\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Appends the class flags in the form of CLASS_Something|CLASS_Something which represents all class flags that are set 
		/// for the specified class which need to be exported as part of the DECLARE_CLASS macro
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="classObj">Class in question</param>
		/// <returns>Output builder</returns>
		private static StringBuilder AppendClassFlags(StringBuilder builder, UhtClass classObj)
		{
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Transient))
			{
				builder.Append(" | CLASS_Transient");
			}
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Optional))
			{
				builder.Append(" | CLASS_Optional");
			}
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.DefaultConfig))
			{
				builder.Append(" | CLASS_DefaultConfig");
			}
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.GlobalUserConfig))
			{
				builder.Append(" | CLASS_GlobalUserConfig");
			}
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.ProjectUserConfig))
			{
				builder.Append(" | CLASS_ProjectUserConfig");
			}
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Config))
			{
				builder.Append(" | CLASS_Config");
			}
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface))
			{
				builder.Append(" | CLASS_Interface");
			}
			if (classObj.ClassFlags.HasAnyFlags(EClassFlags.Deprecated))
			{
				builder.Append(" | CLASS_Deprecated");
			}
			return builder;
		}

		/// <summary>
		/// Appends preprocessor string to emit GENERATED_U*_BODY() macro is deprecated.
		/// </summary>
		/// <param name="builder">Output builder</param>
		/// <param name="macroName">Name of the macro to deprecate</param>
		/// <returns>Output builder</returns>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Referencing code commented out")]
		private static StringBuilder AppendGeneratedMacroDeprecationWarning(StringBuilder builder, string macroName)
		{
			// Deprecation warning is disabled right now. After people get familiar with the new macro it should be re-enabled.
			//Builder.Append("EMIT_DEPRECATED_WARNING_MESSAGE(\"").Append(MacroName).Append("() macro is deprecated. Please use GENERATED_BODY() macro instead.\") \\\r\n");
			return builder;
		}

		private static StringBuilder AppendAccessSpecifier(StringBuilder builder, UhtClass classObj)
		{
			switch (classObj.GeneratedBodyAccessSpecifier)
			{
				case UhtAccessSpecifier.Public:
					builder.Append("public:");
					break;
				case UhtAccessSpecifier.Private:
					builder.Append("private:");
					break;
				case UhtAccessSpecifier.Protected:
					builder.Append("protected:");
					break;
				default:
					builder.Append("static_assert(false, \"Unknown access specifier for GENERATED_BODY() macro in class ").Append(classObj.EngineName).Append(".\");");
					break;
			}
			return builder;
		}

		private StringBuilder AppendInClassIInterface(StringBuilder builder, UhtClass classObj, List<UhtFunction> callbackFunctions, string api)
		{
			string interfaceSourceName = "I" + classObj.EngineName;

			builder.Append("protected: \\\r\n");
			builder.Append("\tvirtual ~").Append(interfaceSourceName).Append("() {} \\\r\n");
			builder.Append("public: \\\r\n");
			builder.Append("\ttypedef ").Append(classObj.SourceName).Append(" UClassType; \\\r\n");
			builder.Append("\ttypedef ").Append(interfaceSourceName).Append(" ThisClass; \\\r\n");

			AppendInterfaceCallFunctions(builder, callbackFunctions);

			// we'll need a way to get to the UObject portion of a native interface, so that we can safely pass native interfaces
			// to script VM functions
			if (classObj.SuperClass != null && classObj.SuperClass.IsChildOf(Session.UInterface))
			{
				// Note: This used to be declared as a pure virtual function, but it was changed here in order to allow the Blueprint nativization process
				// to detect C++ interface classes that explicitly declare pure virtual functions via type traits. This code will no longer trigger that check.
				builder.Append("\tvirtual UObject* _getUObject() const { return nullptr; } \\\r\n");
			}

			if (classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.SelfHasReplicatedProperties))
			{
				AppendReplicatedMacroData(builder, classObj, api);
			}

			return builder;
		}

		private static StringBuilder AppendReplicatedMacroData(StringBuilder builder, UhtClass classObj, string api)
		{
			if (!classObj.ClassExportFlags.HasAnyFlags(UhtClassExportFlags.HasGetLifetimeReplicatedProps))
			{
				// Default version autogenerates declarations.
				if (classObj.GeneratedCodeVersion == EGeneratedCodeVersion.V1)
				{
					builder.Append('\t').Append(api).Append("void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; \\\r\n");
				}
			}

			AppendNetData(builder, classObj, api);

			// If this class has replicated properties and it owns the first one, that means
			// it's the base most replicated class. In that case, go ahead and add our interface macro.
			if (classObj.ClassExportFlags.HasExactFlags(UhtClassExportFlags.HasReplciatedProperties, UhtClassExportFlags.SelfHasReplicatedProperties))
			{
				builder.Append("private: \\\r\n");
				builder.Append("\tREPLICATED_BASE_CLASS(").Append(classObj.SourceName).Append(") \\\r\n");
				builder.Append("public: \\\r\n");
			}
			return builder;
		}

		private static StringBuilder AppendNetData(StringBuilder builder, UhtClass classObj, string api)
		{
			bool hasArray = false;
			foreach (UhtProperty property in classObj.EnumerateReplicatedProperties(false))
			{
				if (property.IsStaticArray)
				{
					if (!hasArray)
					{
						hasArray = true;
						builder.Append("\tenum class EArrayDims_Private : uint16 \\\r\n");
						builder.Append("\t{ \\\r\n");
					}
					builder.Append("\t\t").Append(property.SourceName).Append('=').Append(property.ArrayDimensions).Append(", \\\r\n");
				}
			}

			if (hasArray)
			{
				builder.Append("\t}; \\\r\n");
			}

			builder.Append("\tenum class ENetFields_Private : uint16 \\\r\n");
			builder.Append("\t{ \\\r\n");
			builder.Append("\t\tNETFIELD_REP_START=(uint16)((int32)Super::ENetFields_Private::NETFIELD_REP_END + (int32)1), \\\r\n");

			bool isFirst = true;
			UhtProperty? lastProperty = null;
			foreach (UhtProperty property in classObj.EnumerateReplicatedProperties(false))
			{
				lastProperty = property;
				if (!property.IsStaticArray)
				{
					if (isFirst)
					{
						builder.Append("\t\t").Append(property.SourceName).Append("=NETFIELD_REP_START, \\\r\n");
						isFirst = false;
					}
					else
					{
						builder.Append("\t\t").Append(property.SourceName).Append(", \\\r\n");
					}
				}
				else
				{
					if (isFirst)
					{
						builder.Append("\t\t").Append(property.SourceName).Append("_STATIC_ARRAY=NETFIELD_REP_START, \\\r\n");
						isFirst = false;
					}
					else
					{
						builder.Append("\t\t").Append(property.SourceName).Append("_STATIC_ARRAY, \\\r\n");
					}

					builder
						.Append("\t\t")
						.Append(property.SourceName)
						.Append("_STATIC_ARRAY_END=((uint16)")
						.Append(property.SourceName)
						.Append("_STATIC_ARRAY + (uint16)EArrayDims_Private::")
						.Append(property.SourceName)
						.Append(" - (uint16)1), \\\r\n");
				}
			}

			if (lastProperty != null)
			{
				builder.Append("\t\tNETFIELD_REP_END=").Append(lastProperty.SourceName);
				if (lastProperty.IsStaticArray)
				{
					builder.Append("_STATIC_ARRAY_END");
				}
			}
			builder.Append("\t}; \\\r\n");

			builder.Append('\t').Append(api).Append("virtual void ValidateGeneratedRepEnums(const TArray<struct FRepRecord>& ClassReps) const override; \\\r\n");

			return builder;
		}

		private StringBuilder AppendInterfaceCallFunctions(StringBuilder builder, List<UhtFunction> callbackFunctions)
		{
			const string ExtraArg = "UObject* O";
			const string ConstExtraArg = "const UObject* O";

			foreach (UhtFunction function in callbackFunctions)
			{
				AppendNativeFunctionHeader(builder, function, UhtPropertyTextType.InterfaceFunctionArgOrRetVal, true, null,
					function.FunctionFlags.HasAnyFlags(EFunctionFlags.Const) ? ConstExtraArg : ExtraArg, UhtFunctionExportFlags.None,
					"; \\\r\n");
			}
			return builder;
		}

		private StringBuilder AppendGeneratedBodyMacroBlock(StringBuilder builder, UhtClass classObj, UhtClass bodyClassObj, bool isLegacy, bool hasEditorRpc, bool hasCallbacks, string? deprecatedMacroName)
		{
			bool isInterface = classObj.ClassFlags.HasAnyFlags(EClassFlags.Interface);
			using (UhtMacroCreator macro = new(builder, this, bodyClassObj, isLegacy ? GeneratedBodyLegacyMacroSuffix : GeneratedBodyMacroSuffix))
			{
				if (deprecatedMacroName != null)
				{
					AppendGeneratedMacroDeprecationWarning(builder, deprecatedMacroName);
				}
				builder.Append(DisableDeprecationWarnings).Append(" \\\r\n");
				builder.Append("public: \\\r\n");
				builder.Append('\t').AppendMacroName(this, classObj, SparseDataMacroSuffix).Append(" \\\r\n");
				if (isLegacy)
				{
					builder.Append('\t').AppendMacroName(this, classObj, RpcWrappersMacroSuffix).Append(" \\\r\n");
				}
				else
				{
					builder.Append('\t').AppendMacroName(this, classObj, RpcWrappersNoPureDeclsMacroSuffix).Append(" \\\r\n");
				}
				if (hasEditorRpc)
				{
					if (isLegacy)
					{
						builder.Append('\t').AppendMacroName(this, classObj, EditorOnlyRpcWrappersMacroSuffix).Append(" \\\r\n");
					}
					else
					{
						builder.Append('\t').AppendMacroName(this, classObj, EditorOnlyRpcWrappersNoPureDeclsMacroSuffix).Append(" \\\r\n");
					}
				}
				builder.Append('\t').AppendMacroName(this, classObj, AccessorsMacroSuffix).Append(" \\\r\n");
				if (hasCallbacks)
				{
					builder.Append('\t').AppendMacroName(this, classObj, CallbackWrappersMacroSuffix).Append(" \\\r\n");
				}
				if (isInterface)
				{
					if (isLegacy)
					{
						builder.Append('\t').AppendMacroName(this, classObj, InClassIInterfaceMacroSuffix).Append(" \\\r\n");
					}
					else
					{
						builder.Append('\t').AppendMacroName(this, classObj, InClassIInterfaceNoPureDeclsMacroSuffix).Append(" \\\r\n");
					}
				}
				else
				{
					if (isLegacy)
					{
						builder.Append('\t').AppendMacroName(this, classObj, InClassMacroSuffix).Append(" \\\r\n");
					}
					else
					{
						builder.Append('\t').AppendMacroName(this, classObj, InClassNoPureDeclsMacroSuffix).Append(" \\\r\n");
					}
				}
				if (!isInterface)
				{
					if (isLegacy)
					{
						builder.Append('\t').AppendMacroName(this, classObj, StandardConstructorsMacroSuffix).Append(" \\\r\n");
					}
					else
					{
						builder.Append('\t').AppendMacroName(this, classObj, EnchancedConstructorsMacroSuffix).Append(" \\\r\n");
					}
					if (NeedFieldNotifyCodeGen(classObj))
					{
						builder.Append('\t').AppendMacroName(this, classObj, FieldNotifyMacroSuffix).Append(" \\\r\n");
					}
				}
				if (isLegacy)
				{
					builder.Append("public: \\\r\n");
				}
				else
				{
					AppendAccessSpecifier(builder, classObj);
					builder.Append(" \\\r\n");
				}
				builder.Append(EnableDeprecationWarnings).Append(" \\\r\n");
			}
			return builder;
		}

		#region Enum helper methods
		private static bool IsFullEnumName(string inEnumName)
		{
			return inEnumName.Contains("::", StringComparison.Ordinal);
		}

		private static StringView GenerateEnumPrefix(UhtEnum enumObj)
		{
			StringView prefix = new();
			if (enumObj.EnumValues.Count > 0)
			{
				prefix = enumObj.EnumValues[0].Name;

				// For each item in the enumeration, trim the prefix as much as necessary to keep it a prefix.
				// This ensures that once all items have been processed, a common prefix will have been constructed.
				// This will be the longest common prefix since as little as possible is trimmed at each step.
				for (int nameIdx = 1; nameIdx < enumObj.EnumValues.Count; ++nameIdx)
				{
					StringView enumItemName = enumObj.EnumValues[nameIdx].Name;

					// Find the length of the longest common prefix of Prefix and EnumItemName.
					int prefixIdx = 0;
					while (prefixIdx < prefix.Length && prefixIdx < enumItemName.Length && prefix.Span[prefixIdx] == enumItemName.Span[prefixIdx])
					{
						prefixIdx++;
					}

					// Trim the prefix to the length of the common prefix.
					prefix = new StringView(prefix, 0, prefixIdx);
				}

				// Find the index of the rightmost underscore in the prefix.
				int underscoreIdx = prefix.Span.LastIndexOf('_');

				// If an underscore was found, trim the prefix so only the part before the rightmost underscore is included.
				if (underscoreIdx > 0)
				{
					prefix = new StringView(prefix, 0, underscoreIdx);
				}
				else
				{
					// no underscores in the common prefix - this probably indicates that the names
					// for this enum are not using Epic's notation, so just empty the prefix so that
					// the max item will use the full name of the enum
					prefix = new StringView();
				}
			}

			// If no common prefix was found, or if the enum does not contain any entries,
			// use the name of the enumeration instead.
			if (prefix.Length == 0)
			{
				prefix = enumObj.EngineName;
			}
			return prefix;
		}

		private static string GenerateFullEnumName(UhtEnum enumObj, string inEnumName)
		{
			if (enumObj.CppForm == UhtEnumCppForm.Regular || IsFullEnumName(inEnumName))
			{
				return inEnumName;
			}
			return $"{enumObj.EngineName}::{inEnumName}";
		}

		private static bool EnumHasExistingMax(UhtEnum enumObj)
		{
			if (enumObj.GetIndexByName(GenerateFullEnumName(enumObj, "MAX")) != -1)
			{
				return true;
			}

			string maxEnumItem = GenerateFullEnumName(enumObj, GenerateEnumPrefix(enumObj).ToString() + "_MAX");
			if (enumObj.GetIndexByName(maxEnumItem) != -1)
			{
				return true;
			}
			return false;
		}

		private static long GetMaxEnumValue(UhtEnum enumObj)
		{
			if (enumObj.EnumValues.Count == 0)
			{
				return 0;
			}

			long maxValue = enumObj.EnumValues[0].Value;
			for (int i = 1; i < enumObj.EnumValues.Count; ++i)
			{
				long currentValue = enumObj.EnumValues[i].Value;
				if (currentValue > maxValue)
				{
					maxValue = currentValue;
				}
			}

			return maxValue;
		}
		#endregion
	}

	static class UhtClassExtensions
	{
		public static IEnumerable<UhtProperty> EnumerateReplicatedProperties(this UhtClass classObj, bool includeSuper)
		{
			if (includeSuper && classObj.SuperClass != null)
			{
				foreach (UhtProperty property in classObj.SuperClass.EnumerateReplicatedProperties(true))
				{
					yield return property;
				}
			}

			foreach (UhtProperty property in classObj.Properties)
			{
				if (property.PropertyFlags.HasAnyFlags(EPropertyFlags.Net))
				{
					yield return property;
				}
			}
		}
	}
}
