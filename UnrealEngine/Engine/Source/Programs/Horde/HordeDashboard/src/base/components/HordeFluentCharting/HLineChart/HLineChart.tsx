import * as React from 'react';
import { styled } from '@fluentui/react/lib/Utilities';
import { IHLineChartProps } from './HLineChart.types';
import { HLineChartBase } from './HLineChart.base';
import { ILineChartStyleProps, ILineChartStyles } from '@fluentui/react-charting';

export const getStyles = (props: ILineChartStyleProps): ILineChartStyles => {
  return {
    tooltip: {
      ...props.theme!.fonts.medium,
      display: 'flex',
      flexDirection: 'column',
      padding: '8px',
      position: 'absolute',
      textAlign: 'center',
      top: '0px',
      background: props.theme!.semanticColors.bodyBackground,
      borderRadius: '2px',
      pointerEvents: 'none',
    },
  };
};


// Create a LineChart variant which uses these default styles and this styled subcomponent.
export const HLineChart: React.FunctionComponent<IHLineChartProps> = styled<
  IHLineChartProps,
  ILineChartStyleProps,
  ILineChartStyles
>(HLineChartBase, getStyles);
