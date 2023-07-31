using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Defines triggers on Perforce events. 
	/// </summary>
	public class TriggerTable : List<Trigger>
	{
		public TriggerTable 
			(
			Trigger entry,
			FormSpec spec
			)
		{
			Entry = entry;
			Spec = spec;
		}
		public Trigger Entry {get; set;}
	    public FormSpec Spec {get; set;}

	}

	/// <summary>
	/// Describes an individual entry in the trigger table.
	/// </summary>
	public class Trigger
	{
		public Trigger
			(
			string name,
			int order,
			TriggerType type,
			string path,
			string command
			)
		{
			Name	= name;
			Order = order;
			Type = type;
			Path = path;
			Command= command;
		}
		public string Name { get; set; }
		public int Order { get; set; }
		public TriggerType Type { get; set; }
		public string Path { get; set; }
		public string Command { get; set; }
	}

	/// <summary>
	/// Defines whent he trigger is to execute.
	/// </summary>
	[Flags]
	public enum TriggerType
	{
		/// <summary>
		/// archive:
		///     Execute an archive trigger for the server to access
		///     any file with the +X filetype modifier.
		/// </summary>
		Archive = 0x0000,
		/// <summary>
		/// service-check:
		///     Execute an authentication check trigger to verify a
		///     user's password against an external password manager
		///     during login or when setting a new password.
		/// </summary>
		AuthServiceCheck = 0x0001,
		/// <summary>
		/// auth-check-sso:
		///     Facilitate a single sign-on user authentication. This
		///     configuration requires two programs or scripts to run;
		///     one on the client, the other on the server.
		/// </summary>
		AuthCheckSSO = 0x0002,
		/// <summary>
		/// auth-set:
		///     Execute an authentication set trigger to send a new
		///     password to an external password manager.
		/// </summary>
		AuthSet = 0x0004,
		/// <summary>
		/// change-submit:
		///     Execute pre-submit trigger after changelist has been
		///     created and files locked but prior to file transfer.
		/// </summary>
		ChangeSubmit = 0x0008,
		/// <summary>
		/// change-content:
		///     Execute mid-submit trigger after file transfer but prior
		///     to commit.  Files can be accessed by the 'p4 diff2',
		///     'p4 files', 'p4 fstat', and 'p4 print' commands using
		///     the revision specification '@=change', where 'change' is
		///     the pending changelist number passed as %changelist%.
		/// </summary>
		ChangeContent = 0x0010,
		/// <summary>
		/// change-commit:
		///     Execute post-submit trigger after changelist commit.
		/// </summary>
		ChangeCommit = 0x0020,
		/// <summary>
		/// fix-add:
		///     Execute fix trigger prior to adding a fix.  The special
		///     variable %jobs% is available for expansion and must be
		///     the last argument to the trigger as it expands to one
		///     argument for each job listed on the 'p4 fix' command.
		/// </summary>
		FixAdd = 0x0040,
		/// <summary>
		/// fix-delete:
		///     Execute fix trigger prior to deleting a fix.  The special
		///     variable %jobs% is available for expansion and must be
		///     the last argument to the trigger as it expands to one
		///     argument for each job listed on the 'p4 fix -d' command.
		/// </summary>
		FixDelete = 0x0080,
		/// <summary>
		/// form-out:
		///     Execute form trigger on generation of form.  Trigger may
		///     modify form.
		/// </summary>
		FormOut = 0x0100,
		/// <summary>
		/// form-in:
		///     Execute form trigger on input of form before its contents
		///     are parsed and validated.  Trigger may modify form.
		/// </summary>
		FormIn = 0x0200,
		/// <summary>
		/// form-save:
		///     Execute form trigger prior to save of form after its
		///     contents are parsed.
		/// </summary>
		FormSave = 0x0400,
		/// <summary>
		/// form-commit:
		///     Execute form trigger after it has been committed, allowing
		///     access to automatically generated fields (jobname, dates
		///     etc).  It cannot modify the form.  This trigger for job
		///     forms is run by 'p4 job' and 'p4 fix' (after the status
		///     is updated), 'p4 change' (if the job is added or deleted)
		///     and 'p4 submit' (if the job is associated with the change).
		///     The 'form-commit' trigger has access to the new job name
		///     created with 'p4 job', while the 'form-in' and 'form-save'
		///     triggers are run before the job name is created.  The
		///     special variable %action% is available on the job
		///     'form-commit' trigger command line, and is expanded when
		///     the job is modified by a fix.
		/// </summary>
		FormCommit = 0x0800,
		/// <summary>
		/// form-delete:
		///     Execute form trigger prior to delete of form after its
		///     contents are parsed.
		/// </summary>
		FormDelete = 0x1000,
		/// <summary>
		/// shelve-submit:
		///     Execute pre-shelve trigger after changelist has been
		///     created but prior to file transfer.
		/// </summary>
		ShelveSubmit = 0x2000,
		/// <summary>
		/// shelve-commit:
		///     Execute post-shelve trigger after files are shelved.
		/// </summary>
		ShelveCommit = 0x4000,
		/// <summary>
		/// shelve-delete:
		///     Execute shelve trigger prior to discarding shelved files.
		/// </summary>
		ShelveDelete = 0x8000
	}
}
