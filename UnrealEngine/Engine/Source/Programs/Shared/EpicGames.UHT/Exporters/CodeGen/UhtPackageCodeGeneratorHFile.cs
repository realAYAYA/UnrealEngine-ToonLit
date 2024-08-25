// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text;
using EpicGames.Core;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	internal class UhtPackageCodeGeneratorHFile : UhtPackageCodeGenerator
	{
		/// <summary>
		/// Construct an instance of this generator object
		/// </summary>
		/// <param name="codeGenerator">The base code generator</param>
		/// <param name="package">Package being generated</param>
		public UhtPackageCodeGeneratorHFile(UhtCodeGenerator codeGenerator, UhtPackage package)
			: base(codeGenerator, package)
		{
		}

		/// <summary>
		/// For a given UE header file, generated the generated H file
		/// </summary>
		/// <param name="factory">Requesting factory</param>
		/// <param name="packageSortedHeaders">Sorted list of headers by name of all headers in the package</param>
		public void Generate(IUhtExportFactory factory, List<UhtHeaderFile> packageSortedHeaders)
		{
			{
				using BorrowStringBuilder borrower = new(StringBuilderCache.Big);
				StringBuilder builder = borrower.StringBuilder;

				builder.Append(HeaderCopyright);
				builder.Append("#pragma once\r\n");
				builder.Append("\r\n");
				builder.Append("\r\n");

				List<UhtHeaderFile> headerFiles = new(Package.Children.Count * 2);
				headerFiles.AddRange(packageSortedHeaders);

				foreach (UhtType type in Package.Children)
				{
					if (type is UhtHeaderFile headerFile)
					{
						if (headerFile.HeaderFileType == UhtHeaderFileType.Classes)
						{
							headerFiles.Add(headerFile);
						}
					}
				}

				List<UhtHeaderFile> sortedHeaderFiles = new(headerFiles.Distinct());
				sortedHeaderFiles.Sort((x, y) => StringComparerUE.OrdinalIgnoreCase.Compare(x.FilePath, y.FilePath));

				foreach (UhtHeaderFile headerFile in sortedHeaderFiles)
				{
					if (headerFile.HeaderFileType == UhtHeaderFileType.Classes)
					{
						builder.Append("#include \"").Append(HeaderInfos[headerFile.HeaderFileTypeIndex].IncludePath).Append("\"\r\n");
					}
				}

				builder.Append("\r\n");

				if (SaveExportedHeaders)
				{
					string headerFilePath = factory.MakePath(Package, "Classes.h");
					factory.CommitOutput(headerFilePath, builder);
				}
			}
		}
	}
}
