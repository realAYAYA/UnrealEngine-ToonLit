// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.UHT.Types;

namespace EpicGames.UHT.Utils
{
	/// <summary>
	/// Interface used to collect all the objects referenced by a given type.
	/// Not all types such as UhtPackage and UhtHeaderFile support collecting
	/// references due to assorted reasons.
	/// </summary>
	public interface IUhtReferenceCollector
	{

		/// <summary>
		/// Add a cross module reference to a given object type.
		/// </summary>
		/// <param name="obj">Object type being referenced</param>
		/// <param name="registered">If true, the method being invoked must return the registered object.  This only applies to classes.</param>
		void AddCrossModuleReference(UhtObject? obj, bool registered);

		/// <summary>
		/// Add an object declaration
		/// </summary>
		/// <param name="obj">Object in question</param>
		/// <param name="registered">If true, the method being invoked must return the registered object.  This only applies to classes.</param>
		void AddDeclaration(UhtObject obj, bool registered);

		/// <summary>
		/// Add a field as a singleton for exporting
		/// </summary>
		/// <param name="field">Field to be added</param>
		void AddSingleton(UhtField field);

		/// <summary>
		/// Add a field as a type being exported
		/// </summary>
		/// <param name="field">Field to be added</param>
		void AddExportType(UhtField field);

		/// <summary>
		/// Add a forward declaration.  The string can contain multiple declarations but must only exist on one line.
		/// </summary>
		/// <param name="declaration">The declarations to add</param>
		void AddForwardDeclaration(string? declaration);
	}

	/// <summary>
	/// Delegate used to fetch the string associated with a reference
	/// </summary>
	/// <param name="objectIndex">Index of the referenced object</param>
	/// <param name="registered">If true return the registered string, otherwise the unregistered string.  Classes have an unregistered version.</param>
	/// <returns>The requested string</returns>
	public delegate string GetReferenceStringDelegate(int objectIndex, bool registered);

	/// <summary>
	/// Maintains a list of referenced object indices.
	/// </summary>
	public class UhtUniqueReferenceCollection
	{
		/// <summary>
		/// Collection use to quickly detect if a reference is already in the collection
		/// </summary>
		private HashSet<int> Uniques { get; } = new HashSet<int>();

		/// <summary>
		/// List of all unique reference keys.  Use UngetKey to get the object index and the flag.
		/// </summary>
		public List<int> References { get; } = new List<int>();

		/// <summary>
		/// Return an encoded key that represents the object and registered flag. 
		/// If the object has the alternate object set (i.e. native interfaces), then
		/// that object's index is used to generate the key.
		/// </summary>
		/// <param name="obj">Object being referenced</param>
		/// <param name="registered">If true, then the API that ensures the object is registered is returned.</param>
		/// <returns>Integer key value.</returns>
		public static int GetKey(UhtObject obj, bool registered)
		{
			return obj.AlternateObject != null ? GetKey(obj.AlternateObject, registered) : (obj.ObjectTypeIndex << 1) + (registered ? 1 : 0);
		}

		/// <summary>
		/// Given a key, return the object index and registered flag
		/// </summary>
		/// <param name="key">The key in question</param>
		/// <param name="objectIndex">Index of the referenced object</param>
		/// <param name="registered">True if referencing the registered API.</param>
		public static void UngetKey(int key, out int objectIndex, out bool registered)
		{
			objectIndex = key >> 1;
			registered = (key & 1) != 0;
		}

		/// <summary>
		/// Add the given object to the references
		/// </summary>
		/// <param name="obj">Object to be added</param>
		/// <param name="registered">True if the registered API is being returned.</param>
		public void Add(UhtObject? obj, bool registered)
		{
			if (obj != null)
			{
				int key = GetKey(obj, registered);
				if (Uniques.Add(key))
				{
					References.Add(key);
				}
			}
		}

		/// <summary>
		/// Return the collection of references sorted by the API string returned by the delegate.
		/// </summary>
		/// <param name="referenceStringDelegate">Delegate to invoke to return the requested object API string</param>
		/// <returns>Read only memory region of all the string.</returns>
		public ReadOnlyMemory<string> GetSortedReferences(GetReferenceStringDelegate referenceStringDelegate)
		{
			// Collect the unsorted array
			string[] sorted = new string[References.Count];
			for (int index = 0; index < References.Count; ++index)
			{
				int key = References[index];
				UngetKey(key, out int objectIndex, out bool registered);
				sorted[index] = referenceStringDelegate(objectIndex, registered);
			}

			// Sort the array
			Array.Sort(sorted, StringComparerUE.OrdinalIgnoreCase);

			// Remove duplicates.  In some instances the different keys might return the same string.
			// This removes those duplicates
			if (References.Count > 1)
			{
				int priorOut = 0;
				for (int index = 1; index < sorted.Length; ++index)
				{
					if (sorted[index] != sorted[priorOut])
					{
						++priorOut;
						sorted[priorOut] = sorted[index];
					}
				}
				return sorted.AsMemory(0, priorOut + 1);
			}
			else
			{
				return sorted.AsMemory();
			}
		}
	}

	/// <summary>
	/// Standard implementation of the reference collector interface
	/// </summary>
	public class UhtReferenceCollector : IUhtReferenceCollector
	{

		/// <summary>
		/// Collection of unique cross module references
		/// </summary>
		public UhtUniqueReferenceCollection CrossModule { get; set; } = new UhtUniqueReferenceCollection();

		/// <summary>
		/// Collection of unique declarations
		/// </summary>
		public UhtUniqueReferenceCollection Declaration { get; set; } = new UhtUniqueReferenceCollection();

		/// <summary>
		/// Collection of singletons
		/// </summary>
		public List<UhtField> Singletons { get; } = new List<UhtField>();

		/// <summary>
		/// Collection of types to export
		/// </summary>
		public List<UhtField> ExportTypes { get; } = new List<UhtField>();

		/// <summary>
		/// Collection of forward declarations
		/// </summary>
		public HashSet<string> ForwardDeclarations { get; } = new HashSet<string>();

		/// <summary>
		/// Collection of referenced headers
		/// </summary>
		public HashSet<UhtHeaderFile> ReferencedHeaders { get; } = new HashSet<UhtHeaderFile>();

		/// <summary>
		/// Add a cross module reference
		/// </summary>
		/// <param name="obj">Object being referenced</param>
		/// <param name="registered">True if the object being referenced must be registered</param>
		public void AddCrossModuleReference(UhtObject? obj, bool registered)
		{
			CrossModule.Add(obj, registered);
			if (obj != null && obj is not UhtPackage && registered)
			{
				ReferencedHeaders.Add(obj.HeaderFile);
			}
		}

		/// <summary>
		/// Add a declaration
		/// </summary>
		/// <param name="obj">Object being declared</param>
		/// <param name="registered">True if the object being declared must be registered</param>
		public void AddDeclaration(UhtObject obj, bool registered)
		{
			Declaration.Add(obj, registered);
		}

		/// <summary>
		/// Add a singleton.  These are added as forward declared functions in the package file.
		/// </summary>
		/// <param name="field">Field being added</param>
		public void AddSingleton(UhtField field)
		{
			Singletons.Add(field);
		}

		/// <summary>
		/// Add a type to be exported.
		/// </summary>
		/// <param name="field">Type to be exported</param>
		public void AddExportType(UhtField field)
		{
			ExportTypes.Add(field);
		}

		/// <summary>
		/// Add a symbol that must be forward declared
		/// </summary>
		/// <param name="declaration">Symbol to be forward declared.</param>
		public void AddForwardDeclaration(string? declaration)
		{
			if (!String.IsNullOrEmpty(declaration))
			{
				ForwardDeclarations.Add(declaration);
			}
		}
	}
}
