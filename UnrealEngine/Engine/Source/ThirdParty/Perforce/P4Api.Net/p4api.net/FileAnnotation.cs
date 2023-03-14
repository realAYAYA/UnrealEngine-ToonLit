using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Correlates file lines with revision information. 
	/// </summary>
	public class FileAnnotation
	{
		public FileSpec File { get; set; }
		public string Line { get; set; }

		public FileAnnotation() { }

		public FileAnnotation( FileSpec file, string line)
		{
			File = file;
			Line = line;
		}
	}
}
