// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Schema;
using System.Xml.Serialization;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Functions for manipulating the XML config cache
	/// </summary>
	static class XmlConfig
	{
		/// <summary>
		/// An input config file
		/// </summary>
		public class InputFile
		{
			/// <summary>
			/// Location of the file
			/// </summary>
			public FileReference Location;

			/// <summary>
			/// Which folder to display the config file under in the generated project files
			/// </summary>
			public string FolderName;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="Location"></param>
			/// <param name="FolderName"></param>
			public InputFile(FileReference Location, string FolderName)
			{
				this.Location = Location;
				this.FolderName = FolderName;
			}
		}

		/// <summary>
		/// The cache file that is being used
		/// </summary>
		public static FileReference? CacheFile;

		/// <summary>
		/// Parsed config values
		/// </summary>
		static XmlConfigData? Values;

		/// <summary>
		/// Cached serializer for the XML schema
		/// </summary>
		static XmlSerializer? CachedSchemaSerializer;

		/// <summary>
		/// Initialize the config system with the given types
		/// </summary>
		/// <param name="OverrideCacheFile">Force use of the cached XML config without checking if it's valid (useful for remote builds)</param>
		/// <param name="Logger">Logger for output</param>
		public static void ReadConfigFiles(FileReference? OverrideCacheFile, ILogger Logger)
		{
			// Find all the configurable types
			List<Type> ConfigTypes = FindConfigurableTypes();

			// Update the cache if necessary
			if(OverrideCacheFile != null)
			{
				// Set the cache file to the overriden value
				CacheFile = OverrideCacheFile;

				// Never rebuild the cache; just try to load it.
				if(!XmlConfigData.TryRead(CacheFile, ConfigTypes, out Values))
				{
					throw new BuildException("Unable to load XML config cache ({0})", CacheFile);
				}
			}
			else
			{
				// Get the default cache file
				CacheFile = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "Build", "XmlConfigCache.bin");
				if(Unreal.IsEngineInstalled())
				{
					DirectoryReference? UserSettingsDir = Utils.GetUserSettingDirectory();
					if(UserSettingsDir != null)
					{
						CacheFile = FileReference.Combine(UserSettingsDir, "UnrealEngine", String.Format("XmlConfigCache-{0}.bin", Unreal.RootDirectory.FullName.Replace(":", "").Replace(Path.DirectorySeparatorChar, '+')));
					}
				}

				// Find all the input files
				FileReference[] InputFileLocations = InputFiles.Select(x => x.Location).ToArray();

				// Get the path to the schema
				FileReference SchemaFile = GetSchemaLocation();

				// Try to read the existing cache from disk
				XmlConfigData? CachedValues;
				if(IsCacheUpToDate(CacheFile, InputFileLocations) && FileReference.Exists(SchemaFile))
				{
					if(XmlConfigData.TryRead(CacheFile, ConfigTypes, out CachedValues) && Enumerable.SequenceEqual(InputFileLocations, CachedValues.InputFiles))
					{
						Values = CachedValues;
					}
				}

				// If that failed, regenerate it
				if(Values == null)
				{
					// Find all the configurable fields from the given types
					Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields = new Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>>();
					FindConfigurableFields(ConfigTypes, CategoryToFields);

					// Create a schema for the config files
					XmlSchema Schema = CreateSchema(CategoryToFields);
					if(!Unreal.IsEngineInstalled())
					{
						WriteSchema(Schema, SchemaFile);
					}

					// Read all the XML files and validate them against the schema
					Dictionary<Type, Dictionary<XmlConfigData.TargetMember, XmlConfigData.ValueInfo>> TypeToValues =
						new Dictionary<Type, Dictionary<XmlConfigData.TargetMember, XmlConfigData.ValueInfo>>();
					foreach(FileReference InputFile in InputFileLocations)
					{
						if(!TryReadFile(InputFile, CategoryToFields, TypeToValues, Schema, Logger))
						{
							throw new BuildException("Failed to properly read XML file : {0}", InputFile.FullName);
						}
					}

					// Make sure the cache directory exists
					DirectoryReference.CreateDirectory(CacheFile.Directory);

					// Create the new cache
					Values = new XmlConfigData(InputFileLocations, TypeToValues.ToDictionary(
							x => x.Key, 
							x => x.Value.Select(x => x.Value).ToArray()));
					Values.Write(CacheFile);
				}
			}

			// Apply all the static field values
			foreach(KeyValuePair<Type, XmlConfigData.ValueInfo[]> TypeValuesPair in Values.TypeToValues)
			{
				foreach(XmlConfigData.ValueInfo MemberValue in TypeValuesPair.Value)
				{
					if(MemberValue.Target.IsStatic)
					{
						object Value = InstanceValue(MemberValue.Value, MemberValue.Target.Type);
						MemberValue.Target.SetValue(null, Value);
					}
				}
			}
		}

		/// <summary>
		/// Find all the configurable types in the current assembly
		/// </summary>
		/// <returns>List of configurable types</returns>
		static List<Type> FindConfigurableTypes()
		{
			List<Type> ConfigTypes = new List<Type>();
			try
			{
				foreach (Type ConfigType in Assembly.GetExecutingAssembly().GetTypes())
				{
					if (HasXmlConfigFileAttribute(ConfigType))
					{
						ConfigTypes.Add(ConfigType);
					}
				}
			}
			catch (ReflectionTypeLoadException Ex)
			{
				Console.WriteLine("TypeLoadException: {0}", string.Join("\n", Ex.LoaderExceptions.Select(x => x?.Message)));
				throw;
			}
			return ConfigTypes;
		}

		/// <summary>
		/// Determines whether the given type has a field with an XmlConfigFile attribute
		/// </summary>
		/// <param name="Type">The type to check</param>
		/// <returns>True if the type has a field with the XmlConfigFile attribute</returns>
		static bool HasXmlConfigFileAttribute(Type Type)
		{
			foreach(FieldInfo Field in Type.GetFields(BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic))
			{
				foreach(CustomAttributeData CustomAttribute in Field.CustomAttributes)
				{
					if(CustomAttribute.AttributeType == typeof(XmlConfigFileAttribute))
					{
						return true;
					}
				}
			}
			foreach (PropertyInfo Property in Type.GetProperties(BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic))
			{
				foreach (CustomAttributeData CustomAttribute in Property.CustomAttributes)
				{
					if (CustomAttribute.AttributeType == typeof(XmlConfigFileAttribute))
					{
						return true;
					}
				}
			}
			return false;
		}

		/// <summary>
		/// Find the location of the XML config schema
		/// </summary>
		/// <returns>The location of the schema file</returns>
		public static FileReference GetSchemaLocation()
		{
			return FileReference.Combine(Unreal.EngineDirectory, "Saved", "UnrealBuildTool", "BuildConfiguration.Schema.xsd");
		}

		static InputFile[]? CachedInputFiles;

		/// <summary>
		/// Initialize the list of input files
		/// </summary>
		public static InputFile[] InputFiles
		{
			get
			{
				if (CachedInputFiles != null)
				{
					return CachedInputFiles;
				}

				ILogger Logger = Log.Logger;
				
				// Find all the input file locations
				List<InputFile> InputFilesFound = new List<InputFile>(4);

				// Skip all the config files under the Engine folder if it's an installed build
				if (!Unreal.IsEngineInstalled())
				{
					// Check for the config file under /Engine/Programs/NotForLicensees/UnrealBuildTool
					FileReference NotForLicenseesConfigLocation = FileReference.Combine(Unreal.EngineDirectory, "Restricted", "NotForLicensees", "Programs", "UnrealBuildTool", "BuildConfiguration.xml");
					if (FileReference.Exists(NotForLicenseesConfigLocation))
					{
						InputFilesFound.Add(new InputFile(NotForLicenseesConfigLocation, "NotForLicensees"));
					}
					else
					{
						Logger.LogDebug("No config file at {NotForLicenseesConfigLocation}", NotForLicenseesConfigLocation);
					}

					// Check for the user config file under /Engine/Saved/UnrealBuildTool
					FileReference UserConfigLocation = FileReference.Combine(Unreal.EngineDirectory, "Saved", "UnrealBuildTool", "BuildConfiguration.xml");
					if (!FileReference.Exists(UserConfigLocation))
					{
						Logger.LogDebug("Creating default config file at {UserConfigLocation}", UserConfigLocation);
						CreateDefaultConfigFile(UserConfigLocation);
					}
					InputFilesFound.Add(new InputFile(UserConfigLocation, "User"));
				}

				// Check for the global config file under AppData/Unreal Engine/UnrealBuildTool
				string AppDataFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
				if (!String.IsNullOrEmpty(AppDataFolder))
				{
					FileReference AppDataConfigLocation = FileReference.Combine(new DirectoryReference(AppDataFolder), "Unreal Engine", "UnrealBuildTool", "BuildConfiguration.xml");
					if (!FileReference.Exists(AppDataConfigLocation))
					{
						Logger.LogDebug("Creating default config file at {AppDataConfigLocation}", AppDataConfigLocation);
						CreateDefaultConfigFile(AppDataConfigLocation);
					}
					InputFilesFound.Add(new InputFile(AppDataConfigLocation, "Global (AppData)"));
				}

				// Check for the global config file under My Documents/Unreal Engine/UnrealBuildTool
				string PersonalFolder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
				if (!String.IsNullOrEmpty(PersonalFolder))
				{
					FileReference PersonalConfigLocation = FileReference.Combine(new DirectoryReference(PersonalFolder), "Unreal Engine", "UnrealBuildTool", "BuildConfiguration.xml");
					if (FileReference.Exists(PersonalConfigLocation))
					{
						InputFilesFound.Add(new InputFile(PersonalConfigLocation, "Global (Documents)"));
					}
					else
					{
						Logger.LogDebug("No config file at {PersonalConfigLocation}", PersonalConfigLocation);
					}
				}

				CachedInputFiles = InputFilesFound.ToArray();

				Logger.LogDebug("Configuration will be read from:");
				foreach (InputFile InputFile in InputFiles)
				{
					Logger.LogDebug("  {File}", InputFile.Location.FullName);
				}

				return CachedInputFiles;
			}
		}

		/// <summary>
		/// Create a default config file at the given location
		/// </summary>
		/// <param name="Location">Location to read from</param>
		static void CreateDefaultConfigFile(FileReference Location)
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using (StreamWriter Writer = new StreamWriter(Location.FullName))
			{
				Writer.WriteLine("<?xml version=\"1.0\" encoding=\"utf-8\" ?>");
				Writer.WriteLine("<Configuration xmlns=\"{0}\">", XmlConfigFile.SchemaNamespaceURI);
				Writer.WriteLine("</Configuration>");
			}
		}

		/// <summary>
		/// Applies config values to the given object
		/// </summary>
		/// <param name="TargetObject">The object instance to be configured</param>
		public static void ApplyTo(object TargetObject)
		{
			ILogger Logger = Log.Logger;
			for (Type? TargetType = TargetObject.GetType(); TargetType != null; TargetType = TargetType.BaseType)
			{ 
				XmlConfigData.ValueInfo[]? FieldValues;
				if(Values!.TypeToValues.TryGetValue(TargetType, out FieldValues))
				{
					foreach(XmlConfigData.ValueInfo FieldValue in FieldValues)
					{
						if (!FieldValue.Target.IsStatic)
						{
							XmlConfigData.TargetMember TargetToWrite = FieldValue.Target;
							
							// Check if setting has been deprecated
							if (FieldValue.XmlConfigAttribute.Deprecated)
							{
								Logger.LogWarning("Deprecated setting found in \"{SourceFile}\":", FieldValue.SourceFile);
								Logger.LogWarning("The setting \"{Setting}\" is deprecated. Support for this setting will be removed in a future version of Unreal Engine.", FieldValue.Target.MemberInfo.Name);

								if (FieldValue.XmlConfigAttribute.NewAttributeName != null)
								{
									Logger.LogWarning("Use \"{NewAttributeName}\" in place of \"{OldAttributeName}\"", FieldValue.XmlConfigAttribute.NewAttributeName, FieldValue.Target.MemberInfo.Name);
									Logger.LogInformation("The value provided for deprecated setting \"{OldName}\" will be applied to \"{NewName}\"", FieldValue.Target.MemberInfo.Name, FieldValue.XmlConfigAttribute.NewAttributeName);

									TargetToWrite = GetTargetMember(TargetType, FieldValue.XmlConfigAttribute.NewAttributeName) ?? TargetToWrite;
								}
							}

							object ValueInstance = InstanceValue(FieldValue.Value, FieldValue.Target.Type);
							TargetToWrite.SetValue(TargetObject, ValueInstance);
						}
					}
				}
			}
		}

		private static XmlConfigData.TargetMember? GetTargetMember(Type TargetType, string MemberName)
		{
			// First, try to find the new field to which the setting should be actually applied.
			FieldInfo? FieldInfo = TargetType.GetRuntimeFields()
				.First(x => x.Name == MemberName);

			if (FieldInfo != null)
			{
				return new XmlConfigData.TargetField(FieldInfo);
			}

			// If not found, try to find the new property to which the setting should be actually applied.
			PropertyInfo? PropertyInfo = TargetType.GetRuntimeProperties()
				.First(x => x.Name == MemberName);

			if (PropertyInfo != null)
			{
				return new XmlConfigData.TargetProperty(PropertyInfo);
			}

			return null;
		}

		/// <summary>
		/// Instances a value for assignment to a target object
		/// </summary>
		/// <param name="Value">The value to instance</param>
		/// <param name="ValueType">The type of value</param>
		/// <returns>New instance of the given value, if necessary</returns>
		static object InstanceValue(object Value, Type ValueType)
		{
			if(ValueType == typeof(string[]))
			{
				return ((string[])Value).Clone();
			}
			else
			{
				return Value;
			}
		}

		/// <summary>
		/// Gets a config value for a single value, without writing it to an instance of that class
		/// </summary>
		/// <param name="TargetType">Type to find config values for</param>
		/// <param name="Name">Name of the field to receive</param>
		/// <param name="Value">On success, receives the value of the field</param>
		/// <returns>True if the value was read, false otherwise</returns>
		public static bool TryGetValue(Type TargetType, string Name, [NotNullWhen(true)] out object? Value)
		{
			// Find all the config values for this type
			XmlConfigData.ValueInfo[]? FieldValues;
			if(!Values!.TypeToValues.TryGetValue(TargetType, out FieldValues))
			{
				Value = null;
				return false;
			}

			// Find the value with the matching name
			foreach(XmlConfigData.ValueInfo FieldValue in FieldValues)
			{
				if(FieldValue.Target.MemberInfo.Name == Name)
				{
					Value = FieldValue.Value;
					return true;
				}
			}

			// Not found
			Value = null;
			return false;
		}

		/// <summary>
		/// Find all the configurable fields in the given types by searching for XmlConfigFile attributes.
		/// </summary>
		/// <param name="ConfigTypes">Array of types to search</param>
		/// <param name="CategoryToFields">Dictionaries populated with category -> name -> field mappings on return</param>
		static void FindConfigurableFields(IEnumerable<Type> ConfigTypes, Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields)
		{
			foreach(Type ConfigType in ConfigTypes)
			{
				foreach(FieldInfo FieldInfo in ConfigType.GetFields(BindingFlags.Instance | BindingFlags.Static | BindingFlags.GetField | BindingFlags.Public | BindingFlags.NonPublic))
				{
					ProcessConfigurableMember<FieldInfo>(ConfigType, FieldInfo, CategoryToFields, FieldInfo => new XmlConfigData.TargetField(FieldInfo));
				}
				foreach (PropertyInfo PropertyInfo in ConfigType.GetProperties(BindingFlags.Instance | BindingFlags.Static | BindingFlags.GetField | BindingFlags.Public | BindingFlags.NonPublic))
				{
					ProcessConfigurableMember<PropertyInfo>(ConfigType, PropertyInfo, CategoryToFields, PropertyInfo => new XmlConfigData.TargetProperty(PropertyInfo));
				}
			}
		}

		private static void ProcessConfigurableMember<MEMBER>(Type Type, MEMBER MemberInfo, Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields, Func<MEMBER, XmlConfigData.TargetMember> CreateTarget)
			where MEMBER : System.Reflection.MemberInfo
		{
			IEnumerable<XmlConfigFileAttribute> Attributes = MemberInfo.GetCustomAttributes<XmlConfigFileAttribute>();
			foreach (XmlConfigFileAttribute Attribute in Attributes)
			{
				string CategoryName = Attribute.Category ?? Type.Name;

				Dictionary<string, XmlConfigData.TargetMember>? NameToTarget;
				if (!CategoryToFields.TryGetValue(CategoryName, out NameToTarget))
				{
					NameToTarget = new Dictionary<string, XmlConfigData.TargetMember>();
					CategoryToFields.Add(CategoryName, NameToTarget);
				}

				NameToTarget[Attribute.Name ?? MemberInfo.Name] = CreateTarget(MemberInfo);
			}
		}

		/// <summary>
		/// Creates a schema from attributes in the given types
		/// </summary>
		/// <param name="CategoryToFields">Lookup for all field settings</param>
		/// <returns>New schema instance</returns>
		static XmlSchema CreateSchema(Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields)
		{
			// Create elements for all the categories
			XmlSchemaAll RootAll = new XmlSchemaAll();
			foreach(KeyValuePair<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryPair in CategoryToFields)
			{
				string CategoryName = CategoryPair.Key;

				XmlSchemaAll CategoryAll = new XmlSchemaAll();
				foreach (KeyValuePair<string, XmlConfigData.TargetMember> FieldPair in CategoryPair.Value)
				{
					XmlSchemaElement Element = CreateSchemaFieldElement(FieldPair.Key, FieldPair.Value.Type);
					CategoryAll.Items.Add(Element);
				}

				XmlSchemaComplexType CategoryType = new XmlSchemaComplexType();
				CategoryType.Particle = CategoryAll;

				XmlSchemaElement CategoryElement = new XmlSchemaElement();
				CategoryElement.Name = CategoryName;
				CategoryElement.SchemaType = CategoryType;
				CategoryElement.MinOccurs = 0;
				CategoryElement.MaxOccurs = 1;

				RootAll.Items.Add(CategoryElement);
			}

			// Create the root element and schema object
			XmlSchemaComplexType RootType = new XmlSchemaComplexType();
			RootType.Particle = RootAll;

			XmlSchemaElement RootElement = new XmlSchemaElement();
			RootElement.Name = XmlConfigFile.RootElementName;
			RootElement.SchemaType = RootType;

			XmlSchema Schema = new XmlSchema();
			Schema.TargetNamespace = XmlConfigFile.SchemaNamespaceURI;
			Schema.ElementFormDefault = XmlSchemaForm.Qualified;
			Schema.Items.Add(RootElement);

			// Finally compile it
			XmlSchemaSet SchemaSet = new XmlSchemaSet();
			SchemaSet.Add(Schema);
			SchemaSet.Compile();
			return SchemaSet.Schemas().OfType<XmlSchema>().First();
		}

		/// <summary>
		/// Creates an XML schema element for reading a value of the given type
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Type">Type of the field</param>
		/// <returns>New schema element representing the field</returns>
		static XmlSchemaElement CreateSchemaFieldElement(string Name, Type Type)
		{
			XmlSchemaElement Element = new XmlSchemaElement();
			Element.Name = Name;
			Element.MinOccurs = 0;
			Element.MaxOccurs = 1;

			if (TryGetNullableStructType(Type, out Type? InnerType))
			{
				Type = InnerType;
			}

			if (Type == typeof(string))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.String).QualifiedName;
			}
			else if(Type == typeof(bool))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Boolean).QualifiedName;
			}
			else if(Type == typeof(int))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Int).QualifiedName;
			}
			else if(Type == typeof(float))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Float).QualifiedName;
			}
			else if(Type == typeof(double))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Double).QualifiedName;
			}
			else if(Type == typeof(FileReference))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.String).QualifiedName;
			}
			else if(Type.IsEnum)
			{
				XmlSchemaSimpleTypeRestriction Restriction = new XmlSchemaSimpleTypeRestriction();
				Restriction.BaseTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.String).QualifiedName;

				foreach(string EnumName in Enum.GetNames(Type))
				{
					XmlSchemaEnumerationFacet Facet = new XmlSchemaEnumerationFacet();
					Facet.Value = EnumName;
					Restriction.Facets.Add(Facet);
				}

				XmlSchemaSimpleType EnumType = new XmlSchemaSimpleType();
				EnumType.Content = Restriction;
				Element.SchemaType = EnumType;
			}
			else if(Type == typeof(string[]))
			{
				XmlSchemaElement ItemElement = new XmlSchemaElement();
				ItemElement.Name = "Item";
				ItemElement.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.String).QualifiedName;
				ItemElement.MinOccurs = 0;
				ItemElement.MaxOccursString = "unbounded";

				XmlSchemaSequence Sequence = new XmlSchemaSequence();
				Sequence.Items.Add(ItemElement);

				XmlSchemaComplexType ArrayType = new XmlSchemaComplexType();
				ArrayType.Particle = Sequence;
				Element.SchemaType = ArrayType;
			}
			else
			{
				throw new Exception("Unsupported field type for XmlConfigFile attribute");
			}
			return Element;
		}

		/// <summary>
		/// Writes a schema to the given location. Avoids writing it if the file is identical.
		/// </summary>
		/// <param name="Schema">The schema to be written</param>
		/// <param name="Location">Location to write to</param>
		static void WriteSchema(XmlSchema Schema, FileReference Location)
		{
			XmlWriterSettings Settings = new XmlWriterSettings();
			Settings.Indent = true;
			Settings.IndentChars = "\t";
			Settings.NewLineChars = Environment.NewLine;
			Settings.OmitXmlDeclaration = true;

			if(CachedSchemaSerializer == null)
			{
				CachedSchemaSerializer = XmlSerializer.FromTypes(new Type[] { typeof(XmlSchema) })[0];
			}

			StringBuilder Output = new StringBuilder();
			Output.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			using(XmlWriter Writer = XmlWriter.Create(Output, Settings))
			{
				XmlSerializerNamespaces Namespaces = new XmlSerializerNamespaces();
				Namespaces.Add("", "http://www.w3.org/2001/XMLSchema");
				CachedSchemaSerializer.Serialize(Writer, Schema, Namespaces);
			}

			string OutputText = Output.ToString();
			if(!FileReference.Exists(Location) || File.ReadAllText(Location.FullName) != OutputText)
			{
				DirectoryReference.CreateDirectory(Location.Directory);
				File.WriteAllText(Location.FullName, OutputText);
			}
		}

		/// <summary>
		/// Tests whether a type is a nullable struct, and extracts the inner type if it is
		/// </summary>
		static bool TryGetNullableStructType(Type Type, [NotNullWhen(true)] out Type? InnerType)
		{
			if (Type.IsGenericType && Type.GetGenericTypeDefinition() == typeof(Nullable<>))
			{
				InnerType = Type.GetGenericArguments()[0];
				return true;
			}
			else
			{
				InnerType = null;
				return false;
			}
		}

		/// <summary>
		/// Reads an XML config file and merges it to the given cache
		/// </summary>
		/// <param name="Location">Location to read from</param>
		/// <param name="CategoryToFields">Lookup for configurable fields by category</param>
		/// <param name="TypeToValues">Map of types to fields and their associated values</param>
		/// <param name="Schema">Schema to validate against</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>True if the file was read successfully</returns>
		static bool TryReadFile(FileReference Location, Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields,
			Dictionary<Type, Dictionary<XmlConfigData.TargetMember, XmlConfigData.ValueInfo>> TypeToValues, XmlSchema Schema, ILogger Logger)
		{
			// Read the XML file, and validate it against the schema
			XmlConfigFile? ConfigFile;
			if(!XmlConfigFile.TryRead(Location, Schema, Logger, out ConfigFile))
			{
				return false;
			}

			// Parse the document
			foreach(XmlElement CategoryElement in ConfigFile.DocumentElement!.ChildNodes.OfType<XmlElement>())
			{
				Dictionary<string, XmlConfigData.TargetMember>? NameToField;
				if(CategoryToFields.TryGetValue(CategoryElement.Name, out NameToField))
				{
					foreach(XmlElement KeyElement in CategoryElement.ChildNodes.OfType<XmlElement>())
					{
						XmlConfigData.TargetMember? Field;
						object Value;
						if(NameToField.TryGetValue(KeyElement.Name, out Field))
						{
							if (Field.Type == typeof(string[]))
							{
								Value = KeyElement.ChildNodes.OfType<XmlElement>().Where(x => x.Name == "Item").Select(x => x.InnerText).ToArray();
							}
							else if (TryGetNullableStructType(Field.Type, out Type? StructType))
							{
								Value = ParseValue(StructType, KeyElement.InnerText);
							}
							else
							{
								Value = ParseValue(Field.Type, KeyElement.InnerText);
							}

							// Add it to the set of values for the type containing this field
							Dictionary<XmlConfigData.TargetMember, XmlConfigData.ValueInfo>? FieldToValue;
							if(!TypeToValues.TryGetValue(Field.MemberInfo.DeclaringType!, out FieldToValue))
							{
								FieldToValue = new Dictionary<XmlConfigData.TargetMember, XmlConfigData.ValueInfo>();
								TypeToValues.Add(Field.MemberInfo.DeclaringType!, FieldToValue);
							}
							
							// Parse the corresponding value
							XmlConfigData.ValueInfo FieldValue = new XmlConfigData.ValueInfo(Field, Value, Location,
								Field.MemberInfo.GetCustomAttribute<XmlConfigFileAttribute>()!);
							
							FieldToValue[Field] = FieldValue;
						}
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Parse the value for a field from its text based representation in an XML file
		/// </summary>
		/// <param name="FieldType">The type of field being read</param>
		/// <param name="Text">Text to parse</param>
		/// <returns>The object that was parsed</returns>
		static object ParseValue(Type FieldType, string Text)
		{
			// ignore whitespace in all fields except for Strings which we leave unprocessed
			string TrimmedText = Text.Trim();
			if(FieldType == typeof(string))
			{
				return Text;
			}
			else if(FieldType == typeof(bool) || FieldType == typeof(bool?))
			{
				if (TrimmedText == "1" || TrimmedText.Equals("true", StringComparison.InvariantCultureIgnoreCase))
				{
					return true;
				}
				else if (TrimmedText == "0" || TrimmedText.Equals("false", StringComparison.InvariantCultureIgnoreCase))
				{
					return false;
				} 
				else 
				{
					throw new Exception(String.Format("Unable to convert '{0}' to boolean. 'true/false/0/1' are the supported formats.", Text));
				}
			}
			else if(FieldType == typeof(int))
			{
				return Int32.Parse(TrimmedText);
			}
			else if(FieldType == typeof(float))
			{
				return Single.Parse(TrimmedText, System.Globalization.CultureInfo.InvariantCulture);
			}
			else if(FieldType == typeof(double))
			{
				return Double.Parse(TrimmedText, System.Globalization.CultureInfo.InvariantCulture);
			}
			else if(FieldType.IsEnum)
			{
				return Enum.Parse(FieldType, TrimmedText);
			}
			else if (FieldType == typeof(FileReference))
			{
				return FileReference.FromString(Text);
			}
			else
			{
				throw new Exception(String.Format("Unsupported config type '{0}'", FieldType.Name));
			}
		}

		/// <summary>
		/// Checks that the given cache file exists and is newer than the given input files, and attempts to read it. Verifies that the resulting cache was created
		/// from the same input files in the same order.
		/// </summary>
		/// <param name="CacheFile">Path to the config cache file</param>
		/// <param name="InputFiles">The expected set of input files in the cache</param>
		/// <returns>True if the cache was valid and could be read, false otherwise.</returns>
		static bool IsCacheUpToDate(FileReference CacheFile, FileReference[] InputFiles)
		{
			// Always rebuild if the cache doesn't exist
			if(!FileReference.Exists(CacheFile))
			{
				return false;
			}

			// Get the timestamp for the cache
			DateTime CacheWriteTime = File.GetLastWriteTimeUtc(CacheFile.FullName);

			// Always rebuild if this executable is newer
			if(File.GetLastWriteTimeUtc(Assembly.GetExecutingAssembly().Location) > CacheWriteTime)
			{
				return false;
			}

			// Check if any of the input files are newer than the cache
			foreach(FileReference InputFile in InputFiles)
			{
				if(File.GetLastWriteTimeUtc(InputFile.FullName) > CacheWriteTime)
				{
					return false;
				}
			}

			// Otherwise, it's up to date
			return true;
		}

		/// <summary>
		/// Generates documentation files for the available settings, by merging the XML documentation from the compiler.
		/// </summary>
		/// <param name="OutputFile">The documentation file to write</param>
		/// <param name="Logger">Logger for output</param>
		public static void WriteDocumentation(FileReference OutputFile, ILogger Logger)
		{
			// Find all the configurable types
			List<Type> ConfigTypes = FindConfigurableTypes();

			// Find all the configurable fields from the given types
			Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields = new Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>>();
			FindConfigurableFields(ConfigTypes, CategoryToFields);
			CategoryToFields = CategoryToFields.Where(x => x.Value.Count > 0).ToDictionary(x => x.Key, x => x.Value);

			// Get the path to the XML documentation
			FileReference InputDocumentationFile = new FileReference(Assembly.GetExecutingAssembly().Location).ChangeExtension(".xml");
			if(!FileReference.Exists(InputDocumentationFile))
			{
				throw new BuildException("Generated assembly documentation not found at {0}.", InputDocumentationFile);
			}

			// Read the documentation
			XmlDocument InputDocumentation = new XmlDocument();
			InputDocumentation.Load(InputDocumentationFile.FullName);

			// Make sure we can write to the output file
			if(FileReference.Exists(OutputFile))
			{
				FileReference.MakeWriteable(OutputFile);
			}
			else
			{
				DirectoryReference.CreateDirectory(OutputFile.Directory);
			}

			// Generate the documentation file
			if(OutputFile.HasExtension(".udn"))
			{
				WriteDocumentationUDN(OutputFile, InputDocumentation, CategoryToFields, Logger);
			}
			else if(OutputFile.HasExtension(".html"))
			{
				WriteDocumentationHTML(OutputFile, InputDocumentation, CategoryToFields, Logger);
			}
			else
			{
				throw new BuildException("Unable to detect format from extension of output file ({0})", OutputFile);
			}

			// Success!
			Logger.LogInformation("Written documentation to {OutputFile}.", OutputFile);
		}

		/// <summary>
		/// Writes out documentation in UDN format
		/// </summary>
		/// <param name="OutputFile">The output file</param>
		/// <param name="InputDocumentation">The XML documentation for this assembly</param>
		/// <param name="CategoryToFields">Map of string to types to fields</param>
		/// <param name="Logger">Logger for output</param>
		private static void WriteDocumentationUDN(FileReference OutputFile, XmlDocument InputDocumentation, Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields, ILogger Logger)
		{
			using (StreamWriter Writer = new StreamWriter(OutputFile.FullName))
			{
				Writer.WriteLine("Availability: NoPublish");
				Writer.WriteLine("Title: Build Configuration Properties Page");
				Writer.WriteLine("Crumbs:");
				Writer.WriteLine("Description: This is a procedurally generated markdown page.");
				Writer.WriteLine("Version: {0}.{1}", ReadOnlyBuildVersion.Current.MajorVersion, ReadOnlyBuildVersion.Current.MinorVersion);
				Writer.WriteLine("");

				foreach (KeyValuePair<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryPair in CategoryToFields)
				{
					string CategoryName = CategoryPair.Key;
					Writer.WriteLine("### {0}", CategoryName);
					Writer.WriteLine();

					Dictionary<string, XmlConfigData.TargetMember> Fields = CategoryPair.Value;
					foreach (KeyValuePair<string, XmlConfigData.TargetMember> FieldPair in Fields)
					{
						// Get the XML comment for this field
						List<string>? Lines;
						if(!RulesDocumentation.TryGetXmlComment(InputDocumentation, FieldPair.Value.MemberInfo, Logger, out Lines) || Lines.Count == 0)
						{
							continue;
						}

						// Write the result to the .udn file
						Writer.WriteLine("$ {0} : {1}", FieldPair.Key, Lines[0]);
						for (int Idx = 1; Idx < Lines.Count; Idx++)
						{
							if (Lines[Idx].StartsWith("*") || Lines[Idx].StartsWith("-"))
							{
								Writer.WriteLine("        * {0}", Lines[Idx].Substring(1).TrimStart());
							}
							else
							{
								Writer.WriteLine("    * {0}", Lines[Idx]);
							}
						}
						Writer.WriteLine();
					}
				}
			}
		}

		/// <summary>
		/// Writes out documentation in HTML format
		/// </summary>
		/// <param name="OutputFile">The output file</param>
		/// <param name="InputDocumentation">The XML documentation for this assembly</param>
		/// <param name="CategoryToFields">Map of string to types to fields</param>
		/// <param name="Logger">Logger for output</param>
		private static void WriteDocumentationHTML(FileReference OutputFile, XmlDocument InputDocumentation, Dictionary<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryToFields, ILogger Logger)
		{
			using (StreamWriter Writer = new StreamWriter(OutputFile.FullName))
			{
				Writer.WriteLine("<html>");
				Writer.WriteLine("  <body>");
				Writer.WriteLine("  <h2>BuildConfiguration Properties</h2>");
				foreach (KeyValuePair<string, Dictionary<string, XmlConfigData.TargetMember>> CategoryPair in CategoryToFields)
				{
					string CategoryName = CategoryPair.Key;
					Writer.WriteLine("    <h3>{0}</h3>", CategoryName);
					Writer.WriteLine("    <dl>");

					Dictionary<string, XmlConfigData.TargetMember> Fields = CategoryPair.Value;
					foreach (KeyValuePair<string, XmlConfigData.TargetMember> FieldPair in Fields)
					{
						// Get the XML comment for this field
						List<string>? Lines;
						if (!RulesDocumentation.TryGetXmlComment(InputDocumentation, FieldPair.Value.MemberInfo, Logger, out Lines) || Lines.Count == 0)
						{
							continue;
						}

						// Write the result to the .udn file
						Writer.WriteLine("      <dt>{0}</dt>", FieldPair.Key);

						if (Lines.Count == 1)
						{
							Writer.WriteLine("      <dd>{0}</dd>", Lines[0]);
						}
						else
						{
							Writer.WriteLine("      <dd>");
							for (int Idx = 0; Idx < Lines.Count; Idx++)
							{
								if (Lines[Idx].StartsWith("*") || Lines[Idx].StartsWith("-"))
								{
									Writer.WriteLine("        <ul>");
									for (; Idx < Lines.Count && (Lines[Idx].StartsWith("*") || Lines[Idx].StartsWith("-")); Idx++)
									{
										Writer.WriteLine("          <li>{0}</li>", Lines[Idx].Substring(1).TrimStart());
									}
									Writer.WriteLine("        </ul>");
								}
								else
								{
									Writer.WriteLine("        {0}", Lines[Idx]);
								}
							}
							Writer.WriteLine("      </dd>");
						}
					}

					Writer.WriteLine("    </dl>");
				}
				Writer.WriteLine("  </body>");
				Writer.WriteLine("</html>");
			}
		}
	}
}

