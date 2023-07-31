/*******************************************************************************

Copyright (c) 2011, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: Job.cs
 *
 * Author	: dbb
 *
 * Description	: Class used to abstract a job in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	//public class JobStatus { }

	/// <summary>
	/// A Job in the Repository.
	/// </summary>
	/// <remarks>
	/// A job specification has only one required field, "Job". If
	/// a Perforce installation uses a complex Job specification than
	/// the simple parsing and string formatting provided by the Form
	/// object cannot properly support the data, the Job object can 
	/// subclassed to provide custom parsing and form generation
	/// 
		/// <br/><b>p4 help job</b>
		/// <br/> 
		/// <br/>     job -- Create or edit a job (defect) specification
		/// <br/> 
		/// <br/>     p4 job [-f] [jobName]
		/// <br/>     p4 job -d jobName
		/// <br/>     p4 job -o [jobName]
		/// <br/>     p4 job -i [-f]
		/// <br/> 
		/// <br/> 	The 'p4 job' command creates and edits job specifications using an
		/// <br/> 	ASCII form. A job is a defect, enhancement, or other unit of
		/// <br/> 	intended work.The 'p4 fix' command associates changelists with jobs.
		/// <br/> 
		/// <br/> 	With no arguments, 'p4 job' creates an empty job specification
		/// <br/> 	and invokes the user's editor.  When the specification is saved,
		/// <br/> 	a job name of the form jobNNNNNN is assigned.  If the jobName
		/// <br/> 	parameter is specified on the command line, the job is created or
		/// <br/> 	opened for editing.
		/// <br/> 
		/// <br/> 	As jobs are entered or updated, all fields are indexed for searching
		/// <br/> 	Text fields are broken into individual alphanumeric words (punctuation
		/// <br/> 	and whitespace are ignored) and each word is case-folded and entered
		/// <br/> 	into the word index.  Date fields are converted to an internal
		/// <br/> 	representation (seconds since 1970/01/01 00:00:00) and entered
		/// <br/> 	into the date index.
		/// <br/> 
		/// <br/> 	The fields that compose a job are defined by the 'p4 jobspec' command.
		/// <br/> 	Perforce provides a default job specification that you can edit.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified job. You cannot delete a job if
		/// <br/> 	it has pending or submitted fixes associated with it.
		/// <br/> 
		/// <br/> 	The -o flag writes the job specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a job specification from the standard input. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag enables you set fields that are read-only by default.
		/// <br/> 	The -f flag requires 'admin' access, which is granted using the
		/// <br/> 	'p4 protect' command.
		/// <br/> 
		/// <br/> 
		/// </remarks>
	public class Job : FormBase
	{
		/// <summary>
		/// The job name
		/// </summary>
		public string Id 
		{
			get
            {
                object jobID;
                TryGetValue("Job", out jobID);
                if (jobID != null)
                {
                    return jobID.ToString();
                }
                return null;
            }
			set
			{
				// keep the underlying dictionary in sync
			    this["Job"] = value;
			}
		}

		/// <summary>
		/// Convert to a Job specification
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
            return base.ToString();
		}

		/// <summary>
		/// Parse the tagged output of a 'job' command
		/// </summary>
		/// <param name="obj"></param>
		public virtual void FromJobCmdTaggedOutput(TaggedObject obj)
		{
			SetValues(obj);

			if (obj.ContainsKey("Job"))
			{
				Id = obj["Job"];
			}
		}
		/// <summary>
		/// Parse a Job spec
		/// </summary>
		/// <param name="spec"></param>
		public override bool Parse(string spec)
		{
			base.Parse(spec);

			if (ContainsKey("Job"))
			{
				Id = (string) this["Job"];
			}
			return true;
		}
		/// <summary>
		/// Convert the Job to a Spec Def
		/// </summary>
		/// <param name="list"></param>
		/// <returns></returns>
		public static string[] ToStrings(IList<Job> list)
		{
			if (list == null)
				return null;

			string[] value = new string[list.Count];
			for (int idx = 0; idx < list.Count; idx++)
			{
				value[idx] = list[idx].Id;
			}
			return value;
		}
		/// <summary>
		/// Convert an array of Jobs to a list of JobIds to be passed as 
		/// parameters to a command
		/// </summary>
		/// <param name="list"></param>
		/// <returns></returns>

		public static string[] ToStrings(Job[] list)
		{
			if (list == null)
				return null;

			string[] value = new string[list.Length];
			for (int idx = 0; idx < list.Length; idx++)
			{
				value[idx] = list[idx].Id;
			}
			return value;
		}
	}
}
