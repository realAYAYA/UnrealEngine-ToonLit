// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;

namespace Gauntlet
{
	/// <summary>
	/// Interface for a generic Csv Import.
	/// CsvImportEntry should suit most needs, but if you need something custom you can implement this interface.
	/// </summary>
	public interface ICsvImport
	{
		/// <summary>
		/// The path to the csv file.
		/// </summary>
		public string CsvFilename { get; set; }
		
		/// <summary>
		/// The path to the log file.
		/// </summary>
		public string LogFilename { get; set; }
		
		/// <summary>
		/// Any additional metadata associated with this csv.
		/// </summary>
		public Dictionary<string, string> Metadata { get; }

		/// <summary>
		/// Any additional files that should be bundled with this csv.
		/// Key is the file type, value is the path to the file.
		/// </summary>
		public IReadOnlyDictionary<string, string> AdditionalFiles { get; }

		/// <summary>
		/// Adds an additional file to this import.
		/// </summary>
		/// <param name="FileType">The unique FileType.</param>
		/// <param name="Filename">The path to the file.</param>
		public void AddAdditionalFile(string FileType, string Filename);
	}

	/// <summary>
	/// Generic Csv import entry.
	/// </summary>
	public class CsvImportEntry : ICsvImport
	{
		public string CsvFilename { get; set; }
		public string LogFilename { get; set; }
		public Dictionary<string, string> Metadata { get; }
		public IReadOnlyDictionary<string, string> AdditionalFiles { get; private set; }

		public CsvImportEntry(string CsvFilename, string LogFilename = null, Dictionary<string, string> Metadata = null)
		{
			this.CsvFilename = CsvFilename;
			this.LogFilename = LogFilename;
			this.Metadata = Metadata ?? new Dictionary<string, string>();
		}

		public void AddAdditionalFile(string FileType, string Filename)
		{
			Dictionary<string, string> MutableAdditionalFiles = AdditionalFiles != null 
				? AdditionalFiles.ToDictionary(Kvp => Kvp.Key, Kvp => Kvp.Value)
				: new Dictionary<string, string>();
			
			if (MutableAdditionalFiles.ContainsKey(FileType))
			{
				throw new Exception("Filetype " + FileType + " already registered with CSV " + CsvFilename);
			}

			MutableAdditionalFiles[FileType] = Filename;
			AdditionalFiles = MutableAdditionalFiles;
		}
	}

	/// <summary>
	/// Csv Importer interface.
	/// </summary>
	public interface ICsvImporter
	{
		public void Import(IEnumerable<ICsvImport> Imports);
	}
}
