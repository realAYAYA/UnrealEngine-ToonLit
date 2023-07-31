import { IRenderFunction, IStyleFunctionOrObject } from "@fluentui/react";
import { ICartesianChartProps, IChartProps, IColorFillBarsProps, ICustomizedCalloutDataPoint, IEventsAnnotationProps, ILineChartStyleProps, ILineChartStyles } from "@fluentui/react-charting";

/**
 * Used for custom callout data interface. As Area chart callout data will be prepared from given props.data,
 * Those required data passing to onRenderCalloutPerDataPoint and onRenderCalloutPerStack.
 */
 export interface IHCustomizedCalloutData {
    x: number | string | Date;
    values: ICustomizedCalloutDataPoint[];
    hoverXValue?: string | number | null | undefined;
    hoverYValue?: string | number | null | undefined;
}

export interface IHLineChartProps extends ICartesianChartProps {
    /**
     * Data to render in the chart.
     */
    data: IChartProps;
    /**
     * Call to provide customized styling that will layer on top of the variant rules.
     */
    styles?: IStyleFunctionOrObject<ILineChartStyleProps, ILineChartStyles>;
    /**
     * Show event annotation
     */
    eventAnnotationProps?: IEventsAnnotationProps;
    /**
     * Define a custom callout renderer for a data point
     */
    onRenderCalloutPerDataPoint?: IRenderFunction<(IHCustomizedCalloutData | undefined)[]>;
    /**
     * Define a custom callout renderer for a stack; default is to render per data point
     */
    onRenderCalloutPerStack?: IRenderFunction<(IHCustomizedCalloutData | undefined)[]>;
    colorFillBars?: IColorFillBarsProps[];
    /**
     * if this is set to true, then for each line there will be a unique shape assigned to the point,
     * there are total 8 shapes which are as follow circle, square, triangele, diamond, pyramid,
     *  hexagon, pentagon and octagon, which will get assigned as respectively, if there are more
     * than 8 lines in the line chart then it will again start from cicle to octagon.
     * setting this flag to true will also change the behavior of the points, like for a
     * line, last point shape and first point shape will be visible all the times, and all
     * other points will get enlarge only when hovered over them
     * if set to false default shape will be circle, with the existing behavior
     * @default false
     */
    allowMultipleShapesForPoints?: boolean;
}