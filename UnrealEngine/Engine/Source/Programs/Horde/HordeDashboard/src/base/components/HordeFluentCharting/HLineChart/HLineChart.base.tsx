import * as React from 'react';
import { select as d3Select } from 'd3-selection';
import { classNamesFunction, getId, find } from '@fluentui/react/lib/Utilities';
import { DirectionalHint } from '@fluentui/react/lib/Callout';
import { IBasestate, IChildProps, IColorFillBarsProps, ILegend, ILineChartPoints, ILineChartStyleProps, ILineChartStyles, IRefArrayData, Legends } from '@fluentui/react-charting';
import { calloutData, ChartTypes, getXAxisType, NumericAxis, Points, pointTypes, tooltipOfXAxislabels, XAxisTypes } from '@fluentui/react-charting/lib/utilities/utilities';
import { IHCustomizedCalloutData, IHLineChartProps } from './HLineChart.types';
import { HCartesianChart } from '../HCartesianChart/HCartesianChart';
import { IMargins } from '@fluentui/react-charting/lib/LineChart';

const getClassNames = classNamesFunction<ILineChartStyleProps, ILineChartStyles>();

enum PointSize {
  hoverSize = 11,
  invisibleSize = 1,
  normalVisibleSize = 6,
}

/**
 *
 * @param x units from origin
 * @param y units from origin
 * @param w is the legnth of the each side of a shape
 * @param index index to get the shape path
 */
const _getPointPath = (x: number, y: number, w: number, index: number): string => {
  const allPointPaths = [
    // circle path
    `M${x - w / 2} ${y}
     A${w / 2} ${w / 2} 0 1 0 ${x + w / 2} ${y}
     M${x - w / 2} ${y}
     A ${w / 2} ${w / 2} 0 1 1 ${x + w / 2} ${y}
     `,
    //square
    `M${x - w / 2} ${y - w / 2}
     L${x + w / 2} ${y - w / 2}
     L${x + w / 2} ${y + w / 2}
     L${x - w / 2} ${y + w / 2}
     Z`,
    //triangle
    `M${x - w / 2} ${y - 0.2886 * w}
     H ${x + w / 2}
     L${x} ${y + 0.5774 * w} Z`,
    //diamond
    `M${x} ${y - w / 2}
     L${x + w / 2} ${y}
     L${x} ${y + w / 2}
     L${x - w / 2} ${y}
     Z`,
    //pyramid
    `M${x} ${y - 0.5774 * w}
     L${x + w / 2} ${y + 0.2886 * w}
     L${x - w / 2} ${y + 0.2886 * w} Z`,
    //hexagon
    `M${x - 0.5 * w} ${y - 0.866 * w}
     L${x + 0.5 * w} ${y - 0.866 * w}
     L${x + w} ${y}
     L${x + 0.5 * w} ${y + 0.866 * w}
     L${x - 0.5 * w} ${y + 0.866 * w}
     L${x - w} ${y}
     Z`,
    //pentagon
    `M${x} ${y - 0.851 * w}
     L${x + 0.6884 * w} ${y - 0.2633 * w}
     L${x + 0.5001 * w} ${y + 0.6884 * w}
     L${x - 0.5001 * w} ${y + 0.6884 * w}
     L${x - 0.6884 * w} ${y - 0.2633 * w}
     Z`,
    //octagon
    `M${x - 0.5001 * w} ${y - 1.207 * w}
     L${x + 0.5001 * w} ${y - 1.207 * w}
     L${x + 1.207 * w} ${y - 0.5001 * w}
     L${x + 1.207 * w} ${y + 0.5001 * w}
     L${x + 0.5001 * w} ${y + 1.207 * w}
     L${x - 0.5001 * w} ${y + 1.207 * w}
     L${x - 1.207 * w} ${y + 0.5001 * w}
     L${x - 1.207 * w} ${y - 0.5001 * w}
     Z`,
  ];
  return allPointPaths[index];
};

type LineChartDataWithIndex = ILineChartPoints & { index: number };

export interface IHLineChartState extends IBasestate {
  // This array contains data of selected legends for points
  selectedLegendPoints: LineChartDataWithIndex[];
  // This array contains data of selected legends for color bars
  selectedColorBarLegend: IColorFillBarsProps[];
  // This is a boolean value which is set to true
  // when at least one legend is selected
  isSelectedLegend: boolean;
  // This value will be used as customized callout props - point callout.
  dataPointCalloutProps?: IHCustomizedCalloutData;
  dataPoint2CalloutProps?: IHCustomizedCalloutData;
  // This value will be used as Customized callout props - For stack callout.
  stackCalloutProps?: IHCustomizedCalloutData;
  stack2CalloutProps?: IHCustomizedCalloutData;
  
  // active or hoverd point
  activePoint?: string;

  hoverX2Value?: string | null;
  activePoint2?: string;
  YValueHover2?: {
    legend?: string;
    y?: number;
    color?: string;
  }[];
}

export class HLineChartBase extends React.Component<IHLineChartProps, IHLineChartState> {
  private _points: LineChartDataWithIndex[];
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private _calloutPoints: any[];
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private _xAxisScale: any = '';
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private _yAxisScale: any = '';
  private _circleId: string;
  private _lineId: string;
  private _verticalLine: string;
  private _verticalLine2: string;
  private _colorFillBarPatternId: string;
  private _uniqueCallOutID: string | undefined;
  private _refArray: IRefArrayData[];
  private margins: IMargins | undefined;
  private eventLabelHeight: number = 36;
  private lines: JSX.Element[] | undefined;
  private _renderedColorFillBars: JSX.Element[] | undefined;
  private _colorFillBars: IColorFillBarsProps[];
  private _colorFillBarsOpacity: number;
  private _tooltipId: string;

  constructor(props: IHLineChartProps) {
    super(props);
    this.state = {
      hoverXValue: '',
      hoverX2Value: '',
      activeLegend: '',
      YValueHover: [],
      YValueHover2: [],
      refSelected: '',
      selectedLegend: '',
      isCalloutVisible: false,
      selectedLegendPoints: [],
      selectedColorBarLegend: [],
      isSelectedLegend: false,
      activePoint: '',
    };
    this._refArray = [];
    this._points = this._injectIndexPropertyInLineChartData(this.props.data.lineChartData);
    this._colorFillBars = [];
    this._colorFillBarsOpacity = 0.4;
    this._calloutPoints = calloutData(this._points) || [];
    this._circleId = getId('circle');
    this._lineId = getId('lineID');
    this._verticalLine = getId('verticalLine');
    this._verticalLine2 = getId('verticalLine2');
    this._colorFillBarPatternId = getId('colorFillBarPattern');
    this._tooltipId = getId('LineChartTooltipId_');
    props.eventAnnotationProps &&
      props.eventAnnotationProps.labelHeight &&
      (this.eventLabelHeight = props.eventAnnotationProps.labelHeight);
  }

  public componentDidUpdate(prevProps: IHLineChartProps): void {
    /** note that height and width are not used to resize or set as dimesions of the chart,
     * fitParentContainer is responisble for setting the height and width or resizing of the svg/chart
     */
    if (
      prevProps.height !== this.props.height ||
      prevProps.width !== this.props.width ||
      prevProps.data !== this.props.data
    ) {
      this._points = this._injectIndexPropertyInLineChartData(this.props.data.lineChartData);
      this._calloutPoints = calloutData(this._points) || [];
    }
  }

  public render(): JSX.Element {
    const { tickValues, tickFormat, legendProps, margins } = this.props;
    this._points = this._injectIndexPropertyInLineChartData(this.props.data.lineChartData);

    const isXAxisDateType = getXAxisType(this._points);
    let points = this._points;
    if (legendProps && !!legendProps.canSelectMultipleLegends) {
      points = this.state.selectedLegendPoints.length >= 1 ? this.state.selectedLegendPoints : this._points;
      this._calloutPoints = calloutData(points);
    }

    if(margins) {
      this.margins = margins;
    }

    const legendBars = this._createLegends(this._points!);
    const calloutProps = {
      isCalloutVisible: this.state.isCalloutVisible,
      directionalHint: DirectionalHint.topAutoEdge,
      YValueHover: this.state.YValueHover,
      hoverXValue: this.state.hoverXValue,
      id: `toolTip${this._uniqueCallOutID}`,
      target: this.state.refSelected,
      isBeakVisible: false,
      gapSpace: 15,
      onDismiss: this._closeCallout,
      preventDismissOnEvent: () => true,
      hidden: !(!this.props.hideTooltip && this.state.isCalloutVisible),
      ...this.props.calloutProps,
    };
    const tickParams = {
      tickValues: tickValues,
      tickFormat: tickFormat,
    };

    return (
      <HCartesianChart
        {...this.props}
        points={points}
        chartType={ChartTypes.LineChart}
        isCalloutForStack
        calloutProps={calloutProps}
        tickParams={tickParams}
        legendBars={legendBars}
        getmargins={this._getMargins}
        getGraphData={this._initializeLineChartData}
        xAxisType={isXAxisDateType ? XAxisTypes.DateAxis : XAxisTypes.NumericAxis}
        customizedCallout={this._getCustomizedCallout()}
        /* eslint-disable react/jsx-no-bind */
        // eslint-disable-next-line react/no-children-prop
        children={(props: IChildProps) => {
          this._xAxisScale = props.xScale!;
          this._yAxisScale = props.yScale!;
          return (
            <>
              <g>
                <line
                  x1={0}
                  y1={0}
                  x2={0}
                  y2={props.containerHeight}
                  stroke={'steelblue'}
                  id={this._verticalLine}
                  visibility={'hidden'}
                  strokeDasharray={'5,5'}
                />
                <line
                  x1={0}
                  y1={0}
                  x2={0}
                  y2={props.containerHeight}
                  stroke={'steelblue'}
                  id={this._verticalLine2}
                  visibility={'hidden'}
                  strokeDasharray={'5,5'}
                />
                <g>
                  {this._renderedColorFillBars}
                  {this.lines}
                </g>
                {/* {eventAnnotationProps && (
                  <EventsAnnotation
                    {...eventAnnotationProps}
                    scale={props.xScale!}
                    chartYTop={this.margins.top! + this.eventLabelHeight}
                    chartYBottom={props.containerHeight! - 35}
                  />
                )} */}
              </g>
            </>
          );
        }}
      />
    );
  }

  private _injectIndexPropertyInLineChartData = (lineChartData?: ILineChartPoints[]): LineChartDataWithIndex[] | [] => {
    const { allowMultipleShapesForPoints = false } = this.props;
    return lineChartData
      ? lineChartData.map((item: ILineChartPoints, index: number) => ({
          ...item,
          index: allowMultipleShapesForPoints ? index : -1,
        }))
      : [];
  };

  private _getCustomizedCallout = () => {
    return this.props.onRenderCalloutPerStack
      ? this.props.onRenderCalloutPerStack(
          [this.state.stackCalloutProps, this.state.stack2CalloutProps],
        )
      : this.props.onRenderCalloutPerDataPoint
      ? this.props.onRenderCalloutPerDataPoint(
          [this.state.dataPointCalloutProps, this.state.dataPoint2CalloutProps]
        )
      : null;
  };

  private _getMargins = () => {
    return this.margins;
  };

  private _initializeLineChartData = (
    xScale: NumericAxis,
    yScale: NumericAxis,
    containerHeight: number,
    containerWidth: number,
    xElement: SVGElement | null,
  ) => {
    this._xAxisScale = xScale;
    this._yAxisScale = yScale;
    this._renderedColorFillBars = this.props.colorFillBars ? this._createColorFillBars(containerHeight) : [];
    this.lines = this._createLines(xElement!);
  };

  private _handleSingleLegendSelectionAction = (lineChartItem: LineChartDataWithIndex | IColorFillBarsProps) => {
    if (this.state.selectedLegend === lineChartItem.legend) {
      this.setState({ selectedLegend: '', activeLegend: lineChartItem.legend });
      this._handleLegendClick(lineChartItem, null);
    } else {
      this.setState({
        selectedLegend: lineChartItem.legend,
        activeLegend: lineChartItem.legend,
      });
      this._handleLegendClick(lineChartItem, lineChartItem.legend);
    }
  };

  private _onHoverCardHide = () => {
    this.setState({
      selectedLegendPoints: [],
      selectedColorBarLegend: [],
      isSelectedLegend: false,
    });
  };

  private _createLegends(data: LineChartDataWithIndex[]): JSX.Element {
    const { legendProps, allowMultipleShapesForPoints = false } = this.props;
    const isLegendMultiSelectEnabled = !!(legendProps && !!legendProps.canSelectMultipleLegends);
    const legendDataItems = data.map((point: LineChartDataWithIndex) => {
      const color: string = point.color;
      // mapping data to the format Legends component needs
      const legend: ILegend = {
        title: point.legend!,
        color: color,
        action: () => {
          if (isLegendMultiSelectEnabled) {
            this._handleMultipleLineLegendSelectionAction(point);
          } else {
            this._handleSingleLegendSelectionAction(point);
          }
        },
        onMouseOutAction: () => {
          this.setState({ activeLegend: '' });
        },
        hoverAction: () => {
          this.setState({ activeLegend: point.legend });
        },
        ...(allowMultipleShapesForPoints && {
          shape: Points[point.index % Object.keys(pointTypes).length] as ILegend['shape'],
        }),
      };
      return legend;
    });

    const colorFillBarsLegendDataItems = this.props.colorFillBars
      ? this.props.colorFillBars.map((colorFillBar: IColorFillBarsProps, index: number) => {
          const title = colorFillBar.legend;
          const legend: ILegend = {
            title,
            color: colorFillBar.color,
            action: () => {
              if (isLegendMultiSelectEnabled) {
                this._handleMultipleColorFillBarLegendSelectionAction(colorFillBar);
              } else {
                this._handleSingleLegendSelectionAction(colorFillBar);
              }
            },
            onMouseOutAction: () => {
              this.setState({ activeLegend: '' });
            },
            hoverAction: () => {
              this.setState({ activeLegend: title });
            },
            opacity: this._colorFillBarsOpacity,
            stripePattern: colorFillBar.applyPattern,
          };
          return legend;
        })
      : [];

    const legends = (
      <Legends
        legends={[...legendDataItems, ...colorFillBarsLegendDataItems]}
        enabledWrapLines={this.props.enabledLegendsWrapLines}
        overflowProps={this.props.legendsOverflowProps}
        focusZonePropsInHoverCard={this.props.focusZonePropsForLegendsInHoverCard}
        overflowText={this.props.legendsOverflowText}
        {...(isLegendMultiSelectEnabled && { onLegendHoverCardLeave: this._onHoverCardHide })}
        {...this.props.legendProps}
      />
    );
    return legends;
  }

  private _closeCallout = () => {
    this.setState({
      isCalloutVisible: false,
    });
  };

  private _getBoxWidthOfShape = (pointId: string, pointIndex: number, isLastPoint: boolean) => {
    const { allowMultipleShapesForPoints = false } = this.props;
    const { activePoint, activePoint2 } = this.state;
    if (allowMultipleShapesForPoints) {
      if (activePoint === pointId || activePoint2 === pointId) {
        return PointSize.hoverSize;
      } else if (pointIndex === 1 || isLastPoint) {
        return PointSize.normalVisibleSize;
      } else {
        return PointSize.normalVisibleSize;
      }
    } else {
      if (activePoint === pointId || activePoint2 === pointId) {
        return PointSize.hoverSize;
      } else {
        return PointSize.normalVisibleSize;
      }
    }
  };

  private _getPath = (
    xPos: number,
    yPos: number,
    pointId: string,
    pointIndex: number,
    isLastPoint: boolean,
    pointOftheLine: number,
  ): string => {
    const { allowMultipleShapesForPoints = false } = this.props;
    let w = this._getBoxWidthOfShape(pointId, pointIndex, isLastPoint);
    const index: number = allowMultipleShapesForPoints ? pointOftheLine % Object.keys(pointTypes).length : 0;
    const widthRatio = pointTypes[index].widthRatio;
    w = widthRatio > 1 ? w / widthRatio : w;

    return _getPointPath(xPos, yPos, w, index);
  };
  private _getPointFill = (lineColor: string, pointId: string, pointIndex: number, isLastPoint: boolean) => {
    const { activePoint, activePoint2 } = this.state;
    const { theme, allowMultipleShapesForPoints = false } = this.props;
    if (allowMultipleShapesForPoints) {
      if (pointIndex === 1 || isLastPoint) {
        if (activePoint === pointId || activePoint2 === pointId) {
          return theme!.palette.white;
        } else {
          return lineColor;
        }
      } else {
        if (activePoint === pointId || activePoint2 === pointId) {
          return theme!.palette.white;
        } else {
          return lineColor;
        }
      }
    } else {
      if (activePoint === pointId || activePoint2 === pointId) {
        return theme!.palette.white;
      } else {
        return lineColor;
      }
    }
  };
  private _createLines(xElement: SVGElement): JSX.Element[] {
    const lines = [];
    if (this.state.isSelectedLegend) {
      this._points = this.state.selectedLegendPoints;
    } else {
      this._points = this._injectIndexPropertyInLineChartData(this.props.data.lineChartData);
    }
    for (let i = 0; i < this._points.length; i++) {
      const legendVal: string = this._points[i].legend;
      const lineColor: string = this._points[i].color;
      const { activePoint, /*activePoint2*/ } = this.state;
      const { theme } = this.props;
      if (this._points[i].data.length === 1) {
        const x1 = this._points[i].data[0].x;
        const y1 = this._points[i].data[0].y;
        const xAxisCalloutData = this._points[i].data[0].xAxisCalloutData;
        const circleId = `${this._circleId}${i}`;
        lines.push(
          <circle
            id={`${this._circleId}${i}`}
            key={`${this._circleId}${i}`}
            r={activePoint === circleId ? 5.5 : 3.5}
            cx={this._xAxisScale(x1)}
            cy={this._yAxisScale(y1)}
            fill={activePoint === circleId ? theme!.palette.white : lineColor}
            onMouseOver={this._handleHover.bind(this, x1, y1, xAxisCalloutData, circleId)}
            onMouseMove={this._handleHover.bind(this, x1, y1, xAxisCalloutData, circleId)}
            onMouseOut={this._handleMouseOut}
            strokeWidth={activePoint === circleId ? 2 : 0}
            stroke={activePoint === circleId ? lineColor : ''}
          />,
        );
      }
      for (let j = 1; j < this._points[i].data.length; j++) {
        const lineId = `${this._lineId}${i}_${j}`;
        const circleId = `${this._circleId}${i}_${j-1}`;
        let circleId2 = `${this._circleId}${i}_${j}`;
        if(j+1 === this._points[i].data.length) {
          circleId2 = `${this._circleId}${i}_${j-1}${j}L`;
        }
        const x1 = this._points[i].data[j - 1].x;
        const y1 = this._points[i].data[j - 1].y;
        const x2 = this._points[i].data[j].x;
        const y2 = this._points[i].data[j].y;
        const xAxisCalloutData = this._points[i].data[j - 1].xAxisCalloutData;
        const xAxisCalloutData2 = this._points[i].data[j].xAxisCalloutData;
        let path = this._getPath(this._xAxisScale(x1), this._yAxisScale(y1), circleId, j, false, this._points[i].index);
        if (this.state.activeLegend === legendVal || this.state.activeLegend === '' || this.state.isSelectedLegend) {
          lines.push(
            <line
              id={lineId}
              key={lineId}
              x1={this._xAxisScale(x1)}
              y1={this._yAxisScale(y1)}
              x2={this._xAxisScale(x2)}
              y2={this._yAxisScale(y2)}
              strokeWidth={this.props.strokeWidth || 4}
              ref={(e: SVGLineElement | null) => {
                this._refCallback(e!, lineId);
              }}
              onMouseOver={(ev) => this._handleLineHover(x1, y1, x2, y2, xAxisCalloutData, xAxisCalloutData2, circleId, circleId2, ev)}
              onMouseMove={(ev) => this._handleLineHover(x1, y1, x2, y2, xAxisCalloutData, xAxisCalloutData2, circleId, circleId2, ev)}
              onMouseOut={this._handleMouseOut}
              stroke={lineColor}
              strokeLinecap={'round'}
              opacity={1}
              onClick={this._onLineClick.bind(this, this._points[i].onLineClick!)}
            />,
          );
          lines.push(
            <path
              id={circleId}
              key={circleId}
              d={path}
              data-is-focusable={i === 0 ? true : false}
              onMouseOver={this._handleHover.bind(this, x1, y1, xAxisCalloutData, circleId)}
              onMouseMove={this._handleHover.bind(this, x1, y1, xAxisCalloutData, circleId)}
              onMouseOut={this._handleMouseOut}
              onFocus={() => this._handleFocus(lineId, x1, y1, xAxisCalloutData, circleId)}
              onBlur={this._handleMouseOut}
              onClick={this._onDataPointClick.bind(this, this._points[i].data[j - 1].onDataPointClick!)}
              opacity={1}
              fill={this._getPointFill(lineColor, circleId, j, false)}
              stroke={lineColor}
              strokeWidth={2}
            />,
          );
          if (j + 1 === this._points[i].data.length) {
            const lastCircleId = `${circleId}${j}L`;
            path = this._getPath(
              this._xAxisScale(x2),
              this._yAxisScale(y2),
              lastCircleId,
              j,
              true,
              this._points[i].index,
            );
            const lastCirlceXCallout = this._points[i].data[j].xAxisCalloutData;
            lines.push(
              <path
                id={lastCircleId}
                key={lastCircleId}
                d={path}
                data-is-focusable={i === 0 ? true : false}
                onMouseOver={this._handleHover.bind(this, x2, y2, lastCirlceXCallout, lastCircleId)}
                onMouseMove={this._handleHover.bind(this, x2, y2, lastCirlceXCallout, lastCircleId)}
                onMouseOut={this._handleMouseOut}
                onFocus={() => this._handleFocus(lineId, x2, y2, lastCirlceXCallout, lastCircleId)}
                onBlur={this._handleMouseOut}
                onClick={this._onDataPointClick.bind(this, this._points[i].data[j].onDataPointClick!)}
                opacity={1}
                fill={this._getPointFill(lineColor, lastCircleId, j, true)}
                stroke={lineColor}
                strokeWidth={2}
              />,
            );
            /* eslint-enable react/jsx-no-bind */
          }
        } else {
          lines.push(
            <line
              id={lineId}
              key={lineId}
              x1={this._xAxisScale(x1)}
              y1={this._yAxisScale(y1)}
              x2={this._xAxisScale(x2)}
              y2={this._yAxisScale(y2)}
              strokeWidth={this.props.strokeWidth || 4}
              stroke={lineColor}
              strokeLinecap={'round'}
              opacity={0.1}
            />,
          );
        }
      }
    }
    const classNames = getClassNames(this.props.styles!, {
      theme: this.props.theme!,
    });
    // Removing un wanted tooltip div from DOM, when prop not provided.
    if (!this.props.showXAxisLablesTooltip) {
      try {
        document.getElementById(this._tooltipId) && document.getElementById(this._tooltipId)!.remove();
        // eslint-disable-next-line no-empty
      } catch (e) {}
    }
    // Used to display tooltip at x axis labels.
    if (!this.props.wrapXAxisLables && this.props.showXAxisLablesTooltip) {
      const xAxisElement = d3Select(xElement).call(this._xAxisScale);
      try {
        document.getElementById(this._tooltipId) && document.getElementById(this._tooltipId)!.remove();
        // eslint-disable-next-line no-empty
      } catch (e) {}
      const tooltipProps = {
        tooltipCls: classNames.tooltip!,
        id: this._tooltipId,
        xAxis: xAxisElement,
      };
      xAxisElement && tooltipOfXAxislabels(tooltipProps);
    }
    return lines;
  }

  private _createColorFillBars = (containerHeight: number) => {
    const colorFillBars: JSX.Element[] = [];
    if (this.state.isSelectedLegend) {
      this._colorFillBars = this.state.selectedColorBarLegend;
    } else {
      this._colorFillBars = this.props.colorFillBars!;
    }
    for (let i = 0; i < this._colorFillBars.length; i++) {
      const colorFillBar = this._colorFillBars[i];
      const colorFillBarId = getId(colorFillBar.legend.replace(/\W/g, ''));

      if (colorFillBar.applyPattern) {
        // Using a pattern element because CSS was unable to render diagonal stripes for rect elements
        colorFillBars.push(this._getStripePattern(colorFillBar.color, i));
      }

      for (let j = 0; j < colorFillBar.data.length; j++) {
        const startX = colorFillBar.data[j].startX;
        const endX = colorFillBar.data[j].endX;
        const opacity =
          this.state.activeLegend === colorFillBar.legend ||
          this.state.activeLegend === '' ||
          this.state.isSelectedLegend
            ? this._colorFillBarsOpacity
            : 0.1;
        colorFillBars.push(
          <rect
            fill={colorFillBar.applyPattern ? `url(#${this._colorFillBarPatternId}${i})` : colorFillBar.color}
            fillOpacity={opacity}
            x={this._xAxisScale(startX)}
            y={this._yAxisScale(0) - containerHeight}
            width={Math.abs(this._xAxisScale(endX) - this._xAxisScale(startX))}
            height={containerHeight}
            key={`${colorFillBarId}${j}`}
          />,
        );
      }
    }
    return colorFillBars;
  };

  private _getStripePattern = (color: string, id: number) => {
    // This describes a tile pattern that resembles diagonal stripes
    // For more information: https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/d
    const stripePath = 'M-4,4 l8,-8 M0,16 l16,-16 M12,20 l8,-8';
    return (
      <pattern
        id={`${this._colorFillBarPatternId}${id}`}
        width={16}
        height={16}
        key={`${this._colorFillBarPatternId}${id}`}
        patternUnits={'userSpaceOnUse'}
      >
        <path d={stripePath} stroke={color} strokeWidth={2} />
      </pattern>
    );
  };

  private _refCallback(element: SVGGElement, legendTitle: string): void {
    this._refArray.push({ index: legendTitle, refElement: element });
  }

  private _handleFocus = (
    lineId: string,
    x: number | Date,
    y: number,
    xAxisCalloutData: string | undefined,
    circleId: string,
  ) => {
    this._uniqueCallOutID = circleId;
    const formattedData = x instanceof Date ? x.toLocaleDateString() : x;
    const xVal = x instanceof Date ? x.getTime() : x;
    const found = find(this._calloutPoints, (element: { x: string | number }) => element.x === xVal) as any;
    const _this = this;
    d3Select('#' + circleId).attr('aria-labelledby', `toolTip${this._uniqueCallOutID}`);
    d3Select(`#${this._verticalLine}`)
      .attr('transform', () => `translate(${_this._xAxisScale(x)}, 0)`)
      .attr('visibility', 'visibility');
    this._refArray.forEach((obj: IRefArrayData) => {
      if (obj.index === lineId) {
        let hoverXValue = xAxisCalloutData ? xAxisCalloutData : '' + formattedData;
        found.hoverXValue = hoverXValue;
        found.hoverYValue = y;
        this.setState({
          isCalloutVisible: true,
          refSelected: obj.refElement,
          hoverXValue: hoverXValue,
          hoverX2Value: undefined,
          hoverYValue: y,
          YValueHover: found.values,
          YValueHover2: undefined,
          stackCalloutProps: found!,
          stack2CalloutProps: undefined,
          dataPointCalloutProps: found!,
          dataPoint2CalloutProps: undefined,
          activePoint: circleId,
        });
      }
    });
  };

  private _handleHover = (
    x: number | Date,
    y: number,
    xAxisCalloutData: string | undefined,
    circleId: string,
    mouseEvent: React.MouseEvent<SVGElement>,
  ) => {
    mouseEvent.persist();
    this._uniqueCallOutID = circleId;
    const formattedData = x instanceof Date ? x.toLocaleDateString() : x;
    const xVal = x instanceof Date ? x.getTime() : x;
    const _this = this;
    d3Select(`#${this._verticalLine}`)
      .attr('transform', () => `translate(${_this._xAxisScale(x)}, 0)`)
      .attr('visibility', 'visibility');
    const found = find(this._calloutPoints, (element: { x: string | number }) => element.x === xVal) as any;
    let hoverXValue = xAxisCalloutData ? xAxisCalloutData : '' + formattedData;
    found.hoverXValue = hoverXValue;
    found.hoverYValue = y;
    this.setState({
      isCalloutVisible: true,
      refSelected: mouseEvent,
      hoverXValue: hoverXValue,
      hoverX2Value: undefined,
      hoverYValue: y,
      YValueHover: found.values,
      YValueHover2: undefined,
      stackCalloutProps: found!,
      stack2CalloutProps: undefined,
      dataPointCalloutProps: found!,
      dataPoint2CalloutProps: undefined,
      activePoint: circleId,
    });
  };

  private _handleLineHover = (
    x1: number | Date,
    y1: number,
    x2: number | Date,
    y2: number,
    xAxisCalloutData: string | undefined,
    xAxisCalloutData2: string | undefined,
    circleId1: string,
    circleId2: string,
    mouseEvent: React.MouseEvent<SVGElement>,
  ) => {
    mouseEvent.persist();
    this._uniqueCallOutID = `${circleId1}-${circleId2}`;
    const formattedX1 = x1 instanceof Date ? x1.toLocaleDateString() : x1;
    const x1Val = x1 instanceof Date ? x1.getTime() : x1;
    const formattedX2 = x2 instanceof Date ? x2.toLocaleDateString() : x2;
    const x2Val = x2 instanceof Date ? x2.getTime() : x2;

    const _this = this;
    d3Select(`#${this._verticalLine}`)
      .attr('transform', () => `translate(${_this._xAxisScale(x1)}, 0)`)
      .attr('visibility', 'visibility');
    d3Select(`#${this._verticalLine2}`)
      .attr('transform', () => `translate(${_this._xAxisScale(x2)}, 0)`)
      .attr('visibility', 'visibility');
    const found1 = find(this._calloutPoints, (element: { x: string | number }) => element.x === x1Val) as any;
    const found2 = find(this._calloutPoints, (element: { x: string | number }) => element.x === x2Val) as any;
    
    let hoverXValue = xAxisCalloutData ? xAxisCalloutData : '' + formattedX1;
    let hoverX2Value = xAxisCalloutData2 ? xAxisCalloutData2 : '' + formattedX2;
    found1.hoverXValue = hoverXValue;
    found1.hoverYValue = y1;
    

    found2.hoverXValue = hoverX2Value;
    found2.hoverYValue = y2;

    this.setState({
      isCalloutVisible: true,
      refSelected: mouseEvent,
      hoverXValue: hoverXValue,
      hoverX2Value: hoverX2Value,
      hoverYValue: y1,
      YValueHover: found1.values,
      YValueHover2: found2.values,
      stackCalloutProps: found1!,
      dataPointCalloutProps: found1!,
      stack2CalloutProps: found2!,
      dataPoint2CalloutProps: found2!,
      activePoint: circleId1,
      activePoint2: circleId2,
    });
  };

  private _onLineClick = (func: () => void) => {
    if (func) {
      func();
    }
  };

  private _onDataPointClick = (func: () => void) => {
    if (func) {
      func();
    }
  };

  private _handleMouseOut = () => {
    d3Select(`#${this._verticalLine}`).attr('visibility', 'hidden');
    d3Select(`#${this._verticalLine2}`).attr('visibility', 'hidden');
    this.setState({
      isCalloutVisible: false,
      activePoint: '',
      activePoint2: '',
    });
  };

  private _handleLegendClick = (
    lineChartItem: LineChartDataWithIndex | IColorFillBarsProps,
    selectedLegend: string | null | string[],
  ): void => {
    if (lineChartItem.onLegendClick) {
      lineChartItem.onLegendClick(selectedLegend);
    }
  };

  private _handleMultipleLineLegendSelectionAction = (selectedLine: LineChartDataWithIndex) => {
    const selectedLineIndex = this.state.selectedLegendPoints.reduce((acc, line, index) => {
      if (acc > -1 || line.legend !== selectedLine.legend) {
        return acc;
      } else {
        return index;
      }
    }, -1);

    let selectedLines: LineChartDataWithIndex[];
    if (selectedLineIndex === -1) {
      selectedLines = [...this.state.selectedLegendPoints, selectedLine];
    } else {
      selectedLines = this.state.selectedLegendPoints
        .slice(0, selectedLineIndex)
        .concat(this.state.selectedLegendPoints.slice(selectedLineIndex + 1));
    }

    const areAllLineLegendsSelected = this.props.data && selectedLines.length === this.props.data.lineChartData!.length;

    if (
      areAllLineLegendsSelected &&
      ((this.props.colorFillBars && this.props.colorFillBars.length === this.state.selectedColorBarLegend.length) ||
        !this.props.colorFillBars)
    ) {
      // Clear all legends if all legends including color fill bar legends are selected
      // Or clear all legends if all legends are selected and there are no color fill bars
      this._clearMultipleLegendSelections();
    } else if (!selectedLines.length && !this.state.selectedColorBarLegend.length) {
      // Clear all legends if no legends including color fill bar legends are selected
      this._clearMultipleLegendSelections();
    } else {
      // Otherwise, set state when one or more legends are selected, including color fill bar legends
      this.setState({
        selectedLegendPoints: selectedLines,
        isSelectedLegend: true,
      });
    }

    const selectedLegendTitlesToPass = selectedLines.map((line: LineChartDataWithIndex) => line.legend);
    this._handleLegendClick(selectedLine, selectedLegendTitlesToPass);
  };

  private _handleMultipleColorFillBarLegendSelectionAction = (selectedColorFillBar: IColorFillBarsProps) => {
    const selectedColorFillBarIndex = this.state.selectedColorBarLegend.reduce((acc, colorFillBar, index) => {
      if (acc > -1 || colorFillBar.legend !== selectedColorFillBar.legend) {
        return acc;
      } else {
        return index;
      }
    }, -1);

    let selectedColorFillBars: IColorFillBarsProps[];
    if (selectedColorFillBarIndex === -1) {
      selectedColorFillBars = [...this.state.selectedColorBarLegend, selectedColorFillBar];
    } else {
      selectedColorFillBars = this.state.selectedColorBarLegend
        .slice(0, selectedColorFillBarIndex)
        .concat(this.state.selectedColorBarLegend.slice(selectedColorFillBarIndex + 1));
    }

    const areAllColorFillBarLegendsSelected =
      selectedColorFillBars.length === (this.props.colorFillBars && this.props.colorFillBars!.length);

    if (
      areAllColorFillBarLegendsSelected &&
      ((this.props.data && this.props.data.lineChartData!.length === this.state.selectedLegendPoints.length) ||
        !this.props.data)
    ) {
      // Clear all legends if all legends, including line legends, are selected
      // Or clear all legends if all legends are selected and there is no line data
      this._clearMultipleLegendSelections();
    } else if (!selectedColorFillBars.length && !this.state.selectedLegendPoints.length) {
      // Clear all legends if no legends are selected, including line legends
      this._clearMultipleLegendSelections();
    } else {
      // set state when one or more legends are selected, including line legends
      this.setState({
        selectedColorBarLegend: selectedColorFillBars,
        isSelectedLegend: true,
      });
    }

    const selectedLegendTitlesToPass = selectedColorFillBars.map(
      (colorFillBar: IColorFillBarsProps) => colorFillBar.legend,
    );
    this._handleLegendClick(selectedColorFillBar, selectedLegendTitlesToPass);
  };

  private _clearMultipleLegendSelections = () => {
    this.setState({
      selectedColorBarLegend: [],
      selectedLegendPoints: [],
      isSelectedLegend: false,
    });
  };
}
