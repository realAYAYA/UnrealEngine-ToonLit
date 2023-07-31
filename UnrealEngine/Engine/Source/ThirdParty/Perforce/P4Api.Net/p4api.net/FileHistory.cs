using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Describes a Perforce file revision in detail, including the changelist
	/// number and associated description, action, user, etc. data.
	/// </summary>
	public class FileHistory
	{
		public FileHistory(int revision, int changelistid, FileAction action,
			DateTime date, string username, FileType filetype, string description,
			string digest, long filesize, PathSpec depotpath, string clientname,
			List<RevisionIntegrationSummary>
			integrationsummaries)
		{
			Revision = revision;
			ChangelistId = changelistid;
			Action = action;
			Date = date;
			UserName = username;
			FileType = filetype;
			Description = description;
			Digest = digest;
			FileSize = filesize;
			DepotPath = depotpath;
			ClientName = clientname;
			IntegrationSummaries = integrationsummaries;
		}
		public int Revision { get; set; }
		public int ChangelistId { get; set; }
		public FileAction Action { get; set; }
		public DateTime Date { get; set; }
		public string UserName { get; set; }
		public FileType FileType { get; set; }
		public string Description { get; set; }
		public string Digest { get; set; }
		public long FileSize { get; set; }
		public PathSpec DepotPath { get; set; }
		public string ClientName { get; set; }
		public IList<RevisionIntegrationSummary> IntegrationSummaries { get; set; }

		/// <summary>
		/// Convert to a string of the format ... #{rev} change {change} {action} on {date} {user}@{client} (type) '{desc}'
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return String.Format("... #{0} change {1} {2} {3}: {2}",
				Revision, Date.ToShortDateString(), ChangelistId, 
				Action.ToString("F"), Description.TrimEnd('\n', '\r'));
		}

		/// <summary>
		/// Convert to a string of the format ... #{rev} change {change} {action} on {date}[ {time}] {user}@{client} (type) '{desc}'
		/// </summary>
		/// <param name="includeTime">Include the time as well as the date</param>
		/// <returns></returns>
		public string ToString(bool includeTime)
		{
			String dateTime;
			if (includeTime)
				dateTime = String.Format("{0} {1}", Date.ToShortDateString(), Date.ToShortTimeString());
			else
				dateTime = Date.ToShortDateString();

			string desc = string.Empty;
			if (Description != null)
			{
				desc = Description.TrimEnd('\n', '\r');
			}

			return String.Format("... #{0} change {1} {2} {3}: {2}",
				Revision, dateTime, ChangelistId,
				Action.ToString("F"), desc);
}
	}

	/// <summary>
	/// Describes an integration, specifying the from file and how
	/// the integration was done.
	/// </summary>
	public class RevisionIntegrationSummary
	{
		public RevisionIntegrationSummary(FileSpec fromfile, string how)
		{
			FromFile = fromfile;
			How = how;
		}
		public FileSpec FromFile { get; set; }
		public string How { get; set; }
	}

}
