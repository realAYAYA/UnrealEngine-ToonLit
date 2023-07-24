using PerfSummaries;
using System.Collections.Generic;
using System.Linq;

namespace PerfReportTool
{
	// Filter out csv stat or metric columns below the specified threshold
	internal class StatThresholdColumnFilter : ISummaryTableColumnFilter
	{
		private float StatThreshold;
		public StatThresholdColumnFilter(float statThreshold)
		{
			StatThreshold = statThreshold;
		}

		public bool ShouldFilter(SummaryTableColumn column, SummaryTable table)
		{
			if (StatThreshold <= 0.0f)
			{
				return false;
			}

			if (!column.isNumeric ||
				(column.elementType != SummaryTableElement.Type.CsvStatAverage &&
				 column.elementType != SummaryTableElement.Type.SummaryTableMetric))
			{
				return false;
			}

			if (column.AreAllValuesOverThreshold((double)StatThreshold))
			{
				return false;
			}

			column.DebugMarkAsFiltered(GetType().ToString(), $"Not all column values are over thershold {StatThreshold}");
			return true;
		}
	}
}
