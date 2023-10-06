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
 * Name		: Repository.Job.cs
 *
 * Author	: dbb
 *
 * Description	: Job operations for the Repository.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace Perforce.P4
{
	public partial class Repository
	{
		/// <summary>
		/// Create a new job in the repository.
		/// </summary>
		/// <param name="job">Job specification for the new job</param>
		/// <param name="options">The '-i' flags is needed when creating a new job</param>
		/// <returns>The Job object if new job was created, null if creation failed</returns>
		/// <remarks> The '-i' flags is added if not specified by the caller
		/// <br/> 
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
        /// <example>
        ///		To create a new job with a name 'Myjob':
        ///		<code> 
        ///		
        ///			Job job = new Job();
        ///			job.Id = "Myjob";
        ///			job.Add("Status", "open");
        ///			job.Add("User", "admin");
        ///			job.Add("Description", "this is a test job");
        ///			Job job = _repository.CreateJob( job, null);
        ///			
        ///		</code>
        ///		To create a job with name of the form jobNNNNNN:
        ///		<code>
        ///        
        ///         Job job = new Job();
        ///			job.Id = "new";
        ///			job.Add("Status", "open");
        ///			job.Add("User", "admin");
        ///			job.Add("Description", "this is a test job");
        ///			Job job = _repository.CreateJob( job, JobCmdFlags.Input);
        ///		    
        ///		</code>
        /// </example>
        /// <seealso cref="JobCmdFlags"/>
		public Job CreateJob(Job job, Options options)
		{
			if (job == null)
			{
				throw new ArgumentNullException("job");

			}
			P4Command cmd = new P4Command(this, "job", true);

			cmd.DataSet = job.ToString();

			if (options == null)
			{
				options = new Options();
			}
			options["-i"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				// If this a new job that was saved, we need to parse out the new job Id
				if (job.Id == "new")
				{
					string[] words = results.InfoOutput[0].Message.Split(' ');

					string newId = words[1];
					Job newJob = GetJob(newId);
					return newJob;

				}
				else
				{
					Job newJob = GetJob(job.Id);
					return newJob;
				}
			}

			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
		/// <summary>
		/// Create a new job in the repository.
		/// </summary>
		/// <param name="job">Job specification for the new job</param>
		/// <returns>The Job object if new job was created, null if creation failed</returns>
        /// <example>
        ///		To create a new job name of the form jobNNNNNN:
        ///		<code> 
        ///		
        ///			Job job = new Job();
        ///			job.Id = new;
        ///			job.Add("Status", "open");
        ///			job.Add("User", "admin");
        ///			job.Add("Description", "this is a test job");
        ///			Job job = _repository.CreateJob( job );
        ///			
        ///		</code>
        /// </example>
		public Job CreateJob(Job job)
		{
			return CreateJob(job, null);
		}
		/// <summary>
		/// Update the record for a job in the repository
		/// </summary>
		/// <param name="job">Job specification for the job being updated</param>
		/// <returns>The Job object if new job was saved, null if creation failed</returns>
        /// <example>
        ///		To update the description of job 'job000001':
        ///		<code> 
        ///		
        ///			Job job = _repository.GetJob("job000001");
        ///			job["Description"] += "\n\Updated description";
        ///			_repository.UpdateJob("job000001");
        ///			
        ///		</code>
        /// </example>
		public Job UpdateJob(Job job)
		{
			return CreateJob(job, null);
		}
		/// <summary>
		/// Get the record for an existing job from the repository.
		/// </summary>
		/// <param name="job">Job name</param>
		/// <param name="options">There are no valid flags to use when fetching an existing job</param>
		/// <returns>The Job object if new job was found, null if the requested job does not exist</returns>
        /// <example>
        ///		Get the record for job 'job000001':
        ///		<code> 
        ///		
        ///			Job job = _repository.GetJob("job000001", null);
        ///			
        ///		</code>
        /// </example>
		public Job GetJob(string job, Options options)
		{
			if (job == null)
			{
				throw new ArgumentNullException("job");

			}
            // first confirm the job exists
            if (!string.IsNullOrEmpty(job))
            {
                JobsCmdOptions opts = new JobsCmdOptions(JobsCmdFlags.LongDescriptions, "Job=" + job, 1);
                if (GetJobs(opts, null) == null)
                    return null;
            }

            P4Command cmd = new P4Command(this, "job", true, job);

			if (options == null)
			{
				options = new Options();
			}
			options["-o"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				Job value = new Job();
				value.FromJobCmdTaggedOutput((results.TaggedOutput[0]));

				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
        /// <summary>
        /// Get the record for an existing job from the repository.
        /// </summary>
        /// <param name="job">Job name</param>
        /// <returns>The Job object if new job was found, null if the requested job does not exist</returns>
        /// <example>
        ///		Get the record for job 'job000001':
        ///		<code> 
        ///		
        ///			Job job = _repository.GetJob("job000001");
        ///			
        ///		</code>
        /// </example>
		public Job GetJob(string job)
		{
			return GetJob(job, null);
		}
        /// <summary>
        /// Get a list of jobs from the repository
        /// </summary>
        /// <param name="options">options for the jobs command<see cref="JobsCmdFlags"/></param>
        /// <param name="files">list of files to filter jobs by</param>
        /// <returns>A list containing the matching jobs</returns>
        /// <remarks>
        /// <br/><b>p4 help jobs</b>
        /// <br/> 
        /// <br/>     jobs -- Display list of jobs
        /// <br/> 
        /// <br/>     p4 jobs [-e jobview -i -l -m max -r] [file[revRange] ...]
        /// <br/>     p4 jobs -R
        /// <br/> 
        /// <br/> 	Lists jobs in the server. If a file specification is included, fixes
        /// <br/> 	for submitted changelists affecting the specified files are listed.
        /// <br/> 	The file specification can include wildcards and a revision range.
        /// <br/> 	 See 'p4 help revisions' for details about specifying revisions.
        /// <br/> 
        /// <br/> 	The -e flag lists jobs matching the expression specified in the
        /// <br/> 	jobview parameter. For a description of jobview syntax, see 'p4 help
        /// <br/> 	jobview'.
        /// <br/> 
        /// <br/> 	The -i flag includes any fixes made by changelists integrated into
        /// <br/> 	the specified files.
        /// <br/> 
        /// <br/> 	The -l flag produces long output with the full text of the job
        /// <br/> 	descriptions.
        /// <br/> 
        /// <br/> 	The -m max flag limits the output to the first 'max' jobs, ordered
        /// <br/> 	by their job name.
        /// <br/> 
        /// <br/> 	The -r flag sorts the jobs in reverse order (by job name).
        /// <br/> 
        /// <br/> 	The -R flag rebuilds the jobs table and reindexes each job, which
        /// <br/> 	is necessary after upgrading to 98.2.  'p4 jobs -R' requires that the
        /// <br/> 	user be an operator or have 'super' access granted by 'p4 protect'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		   
        ///		To get a list of 100 jobs that include files under a given filepath:
        ///		<code> 
        ///			         
        ///         FileSpec path = new FileSpec(new DepotPath(@"//depot/..."), null);
        ///         IList&#60;Job&#62; jobs = rep.GetJobs((new Options(JobsCmdFlags.LongDescriptions, null, 100)), path);
        ///			
        ///		</code>
        ///
        ///		To get a list of 100 jobs with the status "open":
        ///		<code>
        ///	                    
        ///         string jobView = "status=open";
        ///         IList&#60;Job&#62; jobs = rep.GetJobs((new Options(JobsCmdFlags.LongDescriptions, jobView, 100)), path);	
        ///         
        ///		</code>
        ///		To get a list of 10 jobs in reverse order:
        ///     <code>
        ///	                    
        ///        IList&#60;Job&#62; jobs = rep.GetJobs((new Options(JobsCmdFlags.ReverseSort, null, 10));	
        ///         
        ///		</code>
        ///		
        ///     To get a list of 10 jobs that include any fixes made by changelists integrated into
        ///  	the specified files:
        ///  	<code>
        ///  	
        ///         FileSpec path = new FileSpec(new DepotPath(@"//depot/..."), null);
        ///         IList&#60;Job&#62; jobs = rep.GetJobs((new Options(JobsCmdFlags.IncludeIntegratedFixes, null, 10)), path);
        ///  	
        ///     </code>
        ///     
        /// </example>
        /// <seealso cref="JobsCmdFlags"/>
        public IList<Job> GetJobs(Options options, params FileSpec[] files)
		{
			P4Command cmd = null;
            if ((files != null) && (files.Length > 0))
			{
                cmd = new P4Command(this, "jobs", true, FileSpec.ToStrings(files));
			}
			else
			{
				cmd = new P4Command(this, "jobs", true);
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<Job> value = new List<Job>();
				Dictionary<string, Job> map = new Dictionary<string, Job>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Job job = new Job();
					job.FromJobCmdTaggedOutput(obj);
					value.Add(job);
				}
				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
		/// <summary>
		/// Delete a job from the repository
		/// </summary>
		/// <param name="job">The job to be deleted</param>
		/// <param name="options">Only the '-f' flag is valid when deleting an existing job</param>
        /// <example>
        ///		To delete job000002 from the repository :
        ///		<code> 
        ///		
        ///			Job u = _repository.GetJob("job000002");
        ///			_repository.DeleteJob(u, null);
        ///			
        ///		</code>
        ///	</example>	
		public void DeleteJob(Job job, Options options)
		{
			if (job == null)
			{
				throw new ArgumentNullException("job");
			}
			P4Command cmd = new P4Command(this, "job", true, job.Id);

			if (options == null)
			{
				options = new Options();
			}
			options["-d"] = null;
			
			P4CommandResult results = cmd.Run(options);
			if (results.Success == false)
			{
				P4Exception.Throw(results.ErrorList);
			}
		}
	}
}
