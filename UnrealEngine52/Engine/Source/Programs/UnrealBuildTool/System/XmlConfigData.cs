// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildTool;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores parsed values from XML config files which can be applied to a configurable type. Can be serialized to disk in binary form as a cache.
	/// </summary>
	class XmlConfigData
	{
		/// <summary>
		/// The current cache serialization version
		/// </summary>
		const int SerializationVersion = 2;

		/// <summary>
		/// List of input files. Stored to allow checking cache validity.
		/// </summary>
		public FileReference[] InputFiles;

		/// <summary>
		/// Abstract description of a target data member.
		/// </summary>
		public abstract class TargetMember
		{
			/// <summary>
			/// Returns Reflection.MemberInfo describing the target class member.
			/// </summary>
			public abstract MemberInfo               MemberInfo { get; }

			/// <summary>
			/// Returns Reflection.Type of the target class member.
			/// </summary>
			public abstract Type                     Type       { get; }

			/// <summary>
			/// Indicates whether the target class member is static or not.
			/// </summary>
			public abstract bool                     IsStatic   { get; }

			/// <summary>
			/// Returns the value setter of the target class member.
			/// </summary>
			public abstract Action<object?, object?> SetValue   { get; }

			/// <summary>
			/// Returns the value getter of the target class member.
			/// </summary>
			public abstract Func<object?, object?>   GetValue   { get; }
		}

		/// <summary>
		/// Description of a field member.
		/// </summary>
		public class TargetField : TargetMember
		{
			public override MemberInfo               MemberInfo => FieldInfo;
			public override Type                     Type       => FieldInfo.FieldType;
			public override bool                     IsStatic   => FieldInfo.IsStatic;
			public override Action<object?, object?> SetValue   => FieldInfo.SetValue;
			public override Func<object?, object?>   GetValue   => FieldInfo.GetValue;

			private FieldInfo FieldInfo;

			public TargetField(FieldInfo FieldInfo)
			{
				this.FieldInfo = FieldInfo;
			}
		}

		/// <summary>
		/// Description of a property member.
		/// </summary>
		public class TargetProperty : TargetMember
		{
			public override MemberInfo               MemberInfo => PropertyInfo;
			public override Type                     Type       => PropertyInfo.PropertyType;
			public override bool                     IsStatic   => PropertyInfo.GetGetMethod()!.IsStatic;
			public override Action<object?, object?> SetValue   => PropertyInfo.SetValue;
			public override Func<object?, object?>   GetValue   => PropertyInfo.GetValue;

			private PropertyInfo PropertyInfo;

			public TargetProperty(PropertyInfo PropertyInfo)
			{
				this.PropertyInfo = PropertyInfo;
			}
		}

		public class ValueInfo
		{
			public TargetMember Target;
			public object Value;
			public FileReference SourceFile;
			public XmlConfigFileAttribute XmlConfigAttribute;

			public ValueInfo(FieldInfo FieldInfo, object Value, FileReference SourceFile, XmlConfigFileAttribute XmlConfigAttribute)
			{
				this.Target = new TargetField(FieldInfo);
				this.Value = Value;
				this.SourceFile = SourceFile;
				this.XmlConfigAttribute = XmlConfigAttribute;
			}

			public ValueInfo(PropertyInfo PropertyInfo, object Value, FileReference SourceFile, XmlConfigFileAttribute XmlConfigAttribute)
			{
				this.Target = new TargetProperty(PropertyInfo);
				this.Value = Value;
				this.SourceFile = SourceFile;
				this.XmlConfigAttribute = XmlConfigAttribute;
			}

			public ValueInfo(TargetMember Target, object Value, FileReference SourceFile, XmlConfigFileAttribute XmlConfigAttribute)
			{
				this.Target = Target;
				this.Value = Value;
				this.SourceFile = SourceFile;
				this.XmlConfigAttribute = XmlConfigAttribute;
			}
		}
		
		/// <summary>
		/// Stores a mapping from type -> member -> value, with all the config values for configurable fields.
		/// </summary>
		public Dictionary<Type, ValueInfo[]> TypeToValues;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InputFiles"></param>
		/// <param name="TypeToValues"></param>
		public XmlConfigData(FileReference[] InputFiles, Dictionary<Type, ValueInfo[]> TypeToValues)
		{
			this.InputFiles = InputFiles;
			this.TypeToValues = TypeToValues;
		}

		/// <summary>
		/// Attempts to read a previous block of config values from disk
		/// </summary>
		/// <param name="Location">The file to read from</param>
		/// <param name="Types">Array of valid types. Used to resolve serialized type names to concrete types.</param>
		/// <param name="Data">On success, receives the parsed data</param>
		/// <returns>True if the data was read and is valid</returns>
		public static bool TryRead(FileReference Location, IEnumerable<Type> Types, [NotNullWhen(true)] out XmlConfigData? Data)
		{
			// Check the file exists first
			if (!FileReference.Exists(Location))
			{
				Data = null;
				return false;
			}

			// Read the cache from disk
			using (BinaryReader Reader = new BinaryReader(File.Open(Location.FullName, FileMode.Open, FileAccess.Read, FileShare.Read)))
			{
				// Check the serialization version matches
				if(Reader.ReadInt32() != SerializationVersion)
				{
					Data = null;
					return false;
				}

				// Read the input files
				FileReference[] InputFiles = Reader.ReadArray(() => Reader.ReadFileReference())!;

				// Read the types
				int NumTypes = Reader.ReadInt32();
				Dictionary<Type, ValueInfo[]> TypeToValues = new Dictionary<Type, ValueInfo[]>(NumTypes);
				for(int TypeIdx = 0; TypeIdx < NumTypes; TypeIdx++)
				{
					// Read the type name
					string TypeName = Reader.ReadString();

					// Try to find it in the list of configurable types
					Type? Type = Types.FirstOrDefault(x => x.Name == TypeName);
					if(Type == null)
					{
						Data = null;
						return false;
					}

					// Read all the values
					ValueInfo[] Values = new ValueInfo[Reader.ReadInt32()];
					for (int ValueIdx = 0; ValueIdx < Values.Length; ValueIdx++)
					{
						string MemberName = Reader.ReadString();

						XmlConfigData.TargetMember? TargetMember = GetTargetMemberWithAttribute<XmlConfigFileAttribute>(Type, MemberName);

						if (TargetMember != null)
						{
							// If TargetMember is not null, we know it has our attribute.
							XmlConfigFileAttribute XmlConfigAttribute = TargetMember!.MemberInfo.GetCustomAttribute<XmlConfigFileAttribute>()!;

							// Try to parse the value and add it to the output array
							object Value = Reader.ReadObject(TargetMember.Type)!;

							// Read the path of the config file that provided this setting
							FileReference SourceFile = Reader.ReadFileReference();

							Values[ValueIdx] = new ValueInfo(TargetMember, Value, SourceFile, XmlConfigAttribute);
						}
						else
						{
							Data = null;
							return false;
						}
					}

					// Add it to the type map
					TypeToValues.Add(Type, Values);
				}

				// Return the parsed data
				Data = new XmlConfigData(InputFiles.ToArray(), TypeToValues);
				return true;
			}
		}

		/// <summary>
		/// Find a data member (field or property) with the given name and attribute and returns TargetMember wrapper created for it.
		/// </summary>
		/// <typeparam name="T">Attribute a member has to have to be considered.</typeparam>
		/// <param name="Type">Type which members are to be searched</param>
		/// <param name="MemberName">Name of a member (field or property) to find.</param>
		/// <returns>TargetMember wrapper or null if no member has been found.</returns>
		private static XmlConfigData.TargetMember? GetTargetMemberWithAttribute<T>(Type Type, string MemberName)
			where T : System.Attribute
		{
			T? XmlConfigAttribute = null;

			FieldInfo? Field = Type.GetField(MemberName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static);
			XmlConfigAttribute = Field?.GetCustomAttribute<T>();

			if (Field != null && XmlConfigAttribute != null)
			{
				return new XmlConfigData.TargetField(Field);
			}

			PropertyInfo? Property = Type.GetProperty(MemberName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static);
			XmlConfigAttribute = Property?.GetCustomAttribute<T>();

			if (Property != null && XmlConfigAttribute != null)
			{
				return new XmlConfigData.TargetProperty(Property);
			}

			return null;
		}

		/// <summary>
		/// Writes the coalesced config hierarchy to disk
		/// </summary>
		/// <param name="Location">File to write to</param>
		public void Write(FileReference Location)
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using (BinaryWriter Writer = new BinaryWriter(File.Open(Location.FullName, FileMode.Create, FileAccess.Write, FileShare.Read)))
			{
				Writer.Write(SerializationVersion);

				// Save all the input files. The cache will not be valid if these change.
				Writer.Write(InputFiles, Item => Writer.Write(Item));

				// Write all the categories
				Writer.Write(TypeToValues.Count);
				foreach(KeyValuePair<Type, ValueInfo[]> TypePair in TypeToValues)
				{
					Writer.Write(TypePair.Key.Name);
					Writer.Write(TypePair.Value.Length);
					foreach(ValueInfo MemberPair in TypePair.Value)
					{
						Writer.Write(MemberPair.Target.MemberInfo.Name);
						Writer.Write(MemberPair.Target.Type, MemberPair.Value);
						Writer.Write(MemberPair.SourceFile);
					}
				}
			}
		}
	}
}
