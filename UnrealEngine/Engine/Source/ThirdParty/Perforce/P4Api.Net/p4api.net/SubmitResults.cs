using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Describes file submitted and file action performed.
	/// </summary>
	public class FileSubmitRecord
	{
		private StringEnum<FileAction> _action;
		public FileAction Action
		{
			get { return _action; }
			internal set { _action = value; }
		}

		public FileSpec File { get; private set; }

		public FileSubmitRecord(FileAction action, FileSpec file)
		{
			_action = action;
			File = file;
		}

		public FileSubmitRecord()
		{
			_action = FileAction.None;
			File = null;
		}
	}

	/// <summary>
	/// Describes the results for a submit operation on one or more files.
	/// </summary>
	public class SubmitResults
	{
		public int ChangeIdBeforeSubmit { get; internal set; }
		public int FilesLockedBySubmit { get; internal set; }
		public int ChangeIdAfterSubmit { get; internal set; }

		public IList<FileSubmitRecord> Files { get; internal set; }

		public SubmitResults(int changeIdBeforeSubmit, int filesLockedBySubmit, 
			int changeIdAfterSubmit, IList<FileSubmitRecord> files)
		{
			ChangeIdBeforeSubmit = changeIdBeforeSubmit;
			FilesLockedBySubmit = filesLockedBySubmit;
			ChangeIdAfterSubmit = changeIdAfterSubmit;
			Files = files;
		}

		public SubmitResults(int changeIdBeforeSubmit, int filesLockedBySubmit, 
			int changeIdAfterSubmit)
		{
			ChangeIdBeforeSubmit = changeIdBeforeSubmit;
			FilesLockedBySubmit = filesLockedBySubmit;
			ChangeIdAfterSubmit = changeIdAfterSubmit;
			Files = new List<FileSubmitRecord>();
		}

		public SubmitResults()
		{
			ChangeIdBeforeSubmit = -1;
			FilesLockedBySubmit = 0;
			ChangeIdAfterSubmit = -1;
			Files = new List<FileSubmitRecord>();
		}
	}
}
