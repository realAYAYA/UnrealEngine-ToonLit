using PerfSummaries;

namespace PerfReportTool
{
	internal interface ISummaryTableColumnFilter
	{
		bool ShouldFilter(SummaryTableColumn column, SummaryTable table);
	}
}
