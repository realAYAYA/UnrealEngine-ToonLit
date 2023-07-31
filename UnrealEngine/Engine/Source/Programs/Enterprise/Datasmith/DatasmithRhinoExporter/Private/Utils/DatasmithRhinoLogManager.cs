// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace DatasmithRhino.Utils
{
	public enum DatasmithRhinoLogType
	{
		Info,
		Warning,
		Error,
	}

	public class DatasmithRhinoLogManager
	{
		public delegate void OnLogsChangedDelegate(DatasmithRhinoLogManager LogManager);

		public event OnLogsChangedDelegate OnLogsChanged;

		private Dictionary<DatasmithRhinoLogType, StringBuilder> Logs = new Dictionary<DatasmithRhinoLogType, StringBuilder>();

		/// <summary>
		/// Default capacity the StringBuilders are initialized with. The value 16 was taken from the default constructor.
		/// </summary>
		private const int DefaultCapacity = 16;

		/// <summary>
		/// Max capacity the StringBuilders are initialized with. We are explicitly setting it to ensure we get the maximum possible.
		/// </summary>
		private const int MaxCapacity = int.MaxValue;

		public DatasmithRhinoLogManager()
		{
			foreach (DatasmithRhinoLogType LogType in Enum.GetValues(typeof(DatasmithRhinoLogType)))
			{
				Logs.Add(LogType, new StringBuilder(DefaultCapacity, MaxCapacity));
			}
		}

		public void AddLog(DatasmithRhinoLogType LogType, string Message)
		{
			string FormatedMessage = string.Format("{0} : {1}", LogType, Message);

			if (Logs.TryGetValue(LogType, out StringBuilder LogString))
			{
				if (LogString.Length + FormatedMessage.Length < LogString.MaxCapacity)
				{
					LogString.AppendLine(FormatedMessage);

					OnLogsChanged?.Invoke(this);
				}
			}
		}

		public string GetLogs(DatasmithRhinoLogType LogType)
		{
			if (Logs.TryGetValue(LogType, out StringBuilder LogString))
			{
				return LogString.ToString();
			}

			return "";
		}

		public void ClearLogs()
		{
			foreach (StringBuilder LogString in Logs.Values)
			{
				LogString.Clear();
			}

			OnLogsChanged?.Invoke(this);
		}
	}
}