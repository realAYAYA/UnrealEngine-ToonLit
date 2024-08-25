// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Serialization
{
	/// <summary>
	/// Marks a class as supporting serialization to compact-binary, even if it does not have exposed fields. This suppresses errors
	/// when base class objects are empty.
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class CbObjectAttribute : Attribute
	{
	}

	/// <summary>
	/// Attribute used to mark a property that should be serialized to compact binary
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class CbFieldAttribute : Attribute
	{
		/// <summary>
		/// Name of the serialized field
		/// </summary>
		public string? Name { get; }

		/// <summary>
		/// Default constructor
		/// </summary>
		public CbFieldAttribute()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name"></param>
		public CbFieldAttribute(string name)
		{
			Name = name;
		}
	}

	/// <summary>
	/// Attribute used to mark that a property should not be serialized to compact binary
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class CbIgnoreAttribute : Attribute
	{
	}

	/// <summary>
	/// Attribute used to indicate that this object is the base for a class hierarchy. Each derived class must have a [CbDiscriminator] attribute.
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class CbPolymorphicAttribute : Attribute
	{
	}

	/// <summary>
	/// Sets the name used for discriminating between derived classes during serialization
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class CbDiscriminatorAttribute : Attribute
	{
		/// <summary>
		/// Name used to identify this class
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name used to identify this class</param>
		public CbDiscriminatorAttribute(string name) => Name = name;
	}

	/// <summary>
	/// Exception thrown when serializing cb objects
	/// </summary>
	public class CbException : Exception
	{
		/// <inheritdoc cref="Exception(String?)"/>
		public CbException(string message) : base(message)
		{
		}

		/// <inheritdoc cref="Exception(String?, Exception)"/>
		public CbException(string message, Exception inner) : base(message, inner)
		{
		}
	}

	/// <summary>
	/// Exception indicating that a class does not have any fields to serialize
	/// </summary>
	public sealed class CbEmptyClassException : CbException
	{
		/// <summary>
		/// Type with missing field annotations
		/// </summary>
		public Type ClassType { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public CbEmptyClassException(Type classType)
			: base($"{classType.Name} does not have any fields marked with a [CbField] attribute. If this is intended, explicitly mark the class with a [CbObject] attribute.")
		{
			ClassType = classType;
		}
	}

	/// <summary>
	/// Attribute-driven compact binary serializer
	/// </summary>
	public static class CbSerializer
	{
		/// <summary>
		/// Serialize an object
		/// </summary>
		/// <param name="type">Type of the object to serialize</param>
		/// <param name="value"></param>
		/// <returns></returns>
		public static CbObject Serialize(Type type, object value)
		{
			CbWriter writer = new CbWriter();
			CbConverter.GetConverter(type).WriteObject(writer, value);
			return writer.ToObject();
		}

		/// <summary>
		/// Serialize an object
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="value"></param>
		/// <returns></returns>
		public static CbObject Serialize<T>(T value)
		{
			CbWriter writer = new CbWriter();
			CbConverter.GetConverter<T>().Write(writer, value);
			return writer.ToObject();
		}

		/// <summary>
		/// Serialize an object
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="value"></param>
		/// <returns></returns>
		public static byte[] SerializeToByteArray<T>(T value)
		{
			CbWriter writer = new CbWriter();
			CbConverter.GetConverter<T>().Write(writer, value);
			return writer.ToByteArray();
		}

		/// <summary>
		/// Serialize a property to a given writer
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="writer"></param>
		/// <param name="value"></param>
		public static void Serialize<T>(CbWriter writer, T value)
		{
			CbConverter.GetConverter<T>().Write(writer, value);
		}

		/// <summary>
		/// Serialize a named property to the given writer
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="writer"></param>
		/// <param name="name"></param>
		/// <param name="value"></param>
		public static void Serialize<T>(CbWriter writer, CbFieldName name, T value)
		{
			CbConverter.GetConverter<T>().WriteNamed(writer, name, value);
		}

		/// <summary>
		/// Deserialize an object from a <see cref="CbObject"/>
		/// </summary>
		/// <param name="field"></param>
		/// <param name="type">Type of the object to read</param>
		/// <returns></returns>
		public static object? Deserialize(CbField field, Type type)
		{
			return CbConverter.GetConverter(type).ReadObject(field);
		}

		/// <summary>
		/// Deserialize an object from a <see cref="CbField"/>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="field"></param>
		/// <returns></returns>
		public static T Deserialize<T>(CbField field)
		{
			return CbConverter.GetConverter<T>().Read(field);
		}

		/// <summary>
		/// Deserialize an object from a <see cref="CbObject"/>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="obj"></param>
		/// <returns></returns>
		public static T Deserialize<T>(CbObject obj) => Deserialize<T>(obj.AsField());

		/// <summary>
		/// Deserialize an object from a block of memory
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="data"></param>
		/// <returns></returns>
		public static T Deserialize<T>(ReadOnlyMemory<byte> data) => Deserialize<T>(new CbField(data));
	}
}
