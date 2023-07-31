import * as React from 'react';
import { HChartHoverCardBase } from './HHoverCard.base';
import { IHChartHoverCardStyles, IHChartHoverCardStyleProps, IHChartHoverCardProps } from './HHoverCard.types';
import { styled } from '@fluentui/react/lib/Utilities';
import { getChartHoverCardStyles } from './HHoverCard.styles';

//  Create a ChartHoverCard variant which uses these default styles and this styled subcomponent.
export const HChartHoverCard: React.FunctionComponent<IHChartHoverCardProps> = styled<
  IHChartHoverCardProps,
  IHChartHoverCardStyleProps,
  IHChartHoverCardStyles
>(HChartHoverCardBase, getChartHoverCardStyles);
