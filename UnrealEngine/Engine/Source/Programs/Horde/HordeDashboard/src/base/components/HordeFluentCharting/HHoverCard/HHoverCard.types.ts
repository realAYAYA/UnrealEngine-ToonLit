import { ITheme, IStyle, IStyleFunctionOrObject } from '@fluentui/react';
import { ICustomizedCalloutDataPoint } from '@fluentui/react-charting';
import { IHCustomizedCalloutData } from '../HLineChart/HLineChart.types';
export interface IHChartHoverCardProps {
  props1: IHCustomizedCalloutData;
  dataPoint1: ICustomizedCalloutDataPoint;

  props2: IHCustomizedCalloutData | undefined;
  dataPoint2: ICustomizedCalloutDataPoint | undefined;

  includeWait?: boolean;
  /**
   * ratio to show
   * first number is numerator
   * and second number is denominator
   */
  ratio?: [number, number];
  /**
   * Theme (provided through customization.)
   */
  theme?: ITheme;
  /**
   * description message in the callout
   */
  descriptionMessage?: string;
  /**
   * Call to provide customized styling that will layer on top of the variant rules.
   */
  styles?: IStyleFunctionOrObject<IHChartHoverCardStyleProps, IHChartHoverCardStyles>;
}

export interface IHChartHoverCardStyles {
  /**
   * styles for callout root-content
   */
  calloutContentRoot?: IStyle;

  /**
   * styles for callout Date time container
   */
  calloutDateTimeContainer?: IStyle;

  /**
   * styles for callout Date time container
   */
  calloutInfoContainer?: IStyle;

  /**
   * styles for callout Date time container
   */
  calloutBlockContainer?: IStyle;

  /**
   * styles for callout y-content
   */
  calloutlegendText?: IStyle;

  /**
   * styles for callout x-content
   */
  calloutContentX?: IStyle;
  /**
   * styles for callout y-content
   */
  calloutContentY?: IStyle;
  /**
   * styles for denomination
   */
  ratio?: IStyle;
  /**
   * styles for numerator
   */
  numerator?: IStyle;
  /**
   * styles for denominator
   */
  denominator?: IStyle;
  /**
   * styles for the description
   */
  descriptionMessage?: IStyle;
}

export interface IHChartHoverCardStyleProps {
  /**
   * Theme (provided through customization.)
   */
  theme: ITheme;

  /**
   * color for hover card
   */
  color?: string;

  /**
   * X  value for hover card
   */
  XValue?: string;

  /**
   * X2  value for hover card
   */
   X2Value?: string;

  /**
   * indicate if denomination is present
   */
  isRatioPresent?: boolean;
}
