import * as React from 'react';
import { IHChartHoverCardStyles, IHChartHoverCardStyleProps, IHChartHoverCardProps } from './HHoverCard.types';
import { classNamesFunction, IProcessedStyleSet } from '@fluentui/react';

const getClassNames = classNamesFunction<IHChartHoverCardStyleProps, IHChartHoverCardStyles>();
export class HChartHoverCardBase extends React.Component<IHChartHoverCardProps, {}> {
  private _classNames: IProcessedStyleSet<IHChartHoverCardStyles> | undefined;
  public render(): React.ReactNode {
    const { props1, dataPoint1, props2, dataPoint2, styles, theme, ratio, descriptionMessage } = this.props;
    this._classNames = getClassNames(styles!, {
      theme: theme!,
      color: dataPoint1.color,
      XValue: dataPoint1.xAxisCalloutData,
	  X2Value: dataPoint2?.xAxisCalloutData,
      isRatioPresent: !!ratio,
    });

	let calloutIsObject = typeof dataPoint1.yAxisCalloutData !== 'string';
	let xValue = `${props1.hoverXValue!}${props2 ? ` => ${props2.hoverXValue}` : ''}`;

	let legendObjects: any[] = [];
	if(!calloutIsObject) {
		legendObjects.push(
			<div className={this._classNames.calloutBlockContainer}>
            	<div className={this._classNames.calloutlegendText}>
					{dataPoint1.legend}
				</div>
            	<div className={this._classNames.calloutContentY}>
					{dataPoint1.yAxisCalloutData}
					{!!props2 && !!dataPoint2 && ` => ${dataPoint2.yAxisCalloutData}`}
				</div>
          	</div>
		);
	}
	else {
		for(const [key, val] of Object.entries(dataPoint1.yAxisCalloutData as { [id: string] : number })) {
			let yAxisCalloutPoint2 = undefined;
			if(props2 && dataPoint2) {
				yAxisCalloutPoint2 = (dataPoint2.yAxisCalloutData as { [id: string] : number })[key];
			}
			legendObjects.push(
				<div className={this._classNames.calloutBlockContainer}>
					<div className={this._classNames.calloutlegendText}>
						{key}
					</div>
					<div className={this._classNames.calloutContentY}>
						{val}
						{!!yAxisCalloutPoint2 && ` => ${yAxisCalloutPoint2}`}
					</div>
				  </div>
			);
		}
	}

	return (
		<div className={this._classNames.calloutContentRoot}>
			<div className={this._classNames.calloutDateTimeContainer}>
				<div className={this._classNames.calloutContentX}>
					{xValue}
				</div>
          	</div>
        <div className={this._classNames.calloutInfoContainer}>
			{legendObjects.map((object, index) => {
				return object; 
			})}
          {!!ratio && (
            <div className={this._classNames.ratio}>
              <>
                <span className={this._classNames.numerator}>{ratio[0]}</span>/
                <span className={this._classNames.denominator}>{ratio[1]}</span>
              </>
            </div>
          )}
        </div>
		{!!descriptionMessage && 
			<div className={this._classNames.descriptionMessage}>
				{descriptionMessage}
			</div>
		}
      </div>
    );
  }
}
