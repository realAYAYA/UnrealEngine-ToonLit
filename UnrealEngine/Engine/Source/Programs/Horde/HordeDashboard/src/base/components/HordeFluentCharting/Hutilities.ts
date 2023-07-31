import { IXAxisParams, IYAxisParams, prepareDatapoints } from "@fluentui/react-charting/lib/utilities/utilities";
import { axisBottom as d3AxisBottom, axisRight as d3AxisRight, axisLeft as d3AxisLeft } from 'd3-axis';
import { scaleLinear as d3ScaleLinear } from 'd3-scale';
import { select as d3Select } from 'd3-selection';
import { format as d3Format } from 'd3-format';

export interface IHTickParams {
	tickValues?: Date[] | number[];
	tickFormat?: any;
  }

  /**
 * Create Numeric X axis
 * @export
 * @param {IXAxisParams} xAxisParams
 */
export function hCreateNumericXAxis(xAxisParams: IXAxisParams, tickParams?: IHTickParams) {
	const {
	  domainNRangeValues,
	  showRoundOffXTickValues = false,
	  xAxistickSize = 6,
	  tickPadding = 10,
	  xAxisCount = 6,
	  xAxisElement,
	} = xAxisParams;
	const xAxisScale = d3ScaleLinear()
	  .domain([domainNRangeValues.dStartValue, domainNRangeValues.dEndValue])
	  .range([domainNRangeValues.rStartValue, domainNRangeValues.rEndValue]);
	showRoundOffXTickValues && xAxisScale.nice();
  
	const xAxis = d3AxisBottom(xAxisScale)
	  .tickSize(xAxistickSize)
	  .tickPadding(tickPadding)
	  .ticks(xAxisCount)
	  .tickFormat(tickParams?.tickFormat || null)
	  .tickSizeOuter(0);
	if (xAxisElement) {
	  d3Select(xAxisElement)
		.call(xAxis as any)
		.selectAll('text')
		.attr('aria-hidden', 'true');
	}
	return xAxisScale;
  }

  export function hCreateYAxis(yAxisParams: IYAxisParams, isRtl: boolean) {
	const {
	  yMinMaxValues = { startValue: 0, endValue: 0 },
	  yAxisElement = null,
	  yMaxValue = 0,
	  yMinValue = 0,
	  containerHeight,
	  containerWidth,
	  margins,
	  tickPadding = 12,
	  maxOfYVal = 0,
	  yAxisTickFormat,
	  yAxisTickCount = 4,
	  eventAnnotationProps,
	  eventLabelHeight,
	} = yAxisParams;
  
	// maxOfYVal coming from only area chart and Grouped vertical bar chart(Calculation done at base file)
	const tempVal = maxOfYVal || yMinMaxValues.endValue;
	const finalYmax = tempVal > yMaxValue ? tempVal : yMaxValue!;
	const finalYmin = yMinMaxValues.startValue < yMinValue ? 0 : yMinValue!;
	const domainValues = prepareDatapoints(finalYmax, finalYmin, yAxisTickCount);
	const yAxisScale = d3ScaleLinear()
	  .domain([finalYmin, domainValues[domainValues.length - 1]])
	  .range([containerHeight - margins.bottom!, margins.top! + (eventAnnotationProps! ? eventLabelHeight! : 0)]);
	const axis = isRtl ? d3AxisRight(yAxisScale) : d3AxisLeft(yAxisScale);
	const yAxis = axis
	  .tickPadding(tickPadding)
	  .tickValues(domainValues)
	  .tickSizeInner(-(containerWidth - margins.left! - margins.right!));
	yAxisTickFormat ? yAxis.tickFormat(yAxisTickFormat) : yAxis.tickFormat(d3Format('.2~s'));
	// eslint-disable-next-line @typescript-eslint/no-unused-expressions
	yAxisElement
	  ? d3Select(yAxisElement)
		  .call(yAxis as any)
		  .selectAll('text')
		  .attr('aria-hidden', 'true')
	  : '';
	return yAxisScale;
  }