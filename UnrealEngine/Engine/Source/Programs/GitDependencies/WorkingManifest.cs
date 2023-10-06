// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace GitDependencies
{
	public class WorkingFile
	{
		[XmlAttribute]
		public string Name = null!;

		[XmlAttribute]
		public string Hash = null!;

		[XmlAttribute]
		public string ExpectedHash = null!;

		[XmlAttribute]
		public long Timestamp;
	}

	public class WorkingManifest
	{
		[XmlArrayItem("File")]
		public List<WorkingFile> Files = new List<WorkingFile>();
	}
}
