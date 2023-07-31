import React from 'react';
import { ICustomStackProperty, PropertyValue } from 'src/shared';


type Props = {
  widget: ICustomStackProperty;
  checked?: boolean;
  label?: React.ReactNode;

  onChange?: (widget: ICustomStackProperty, value?: PropertyValue) => void;
}

export class ToggleWidget extends React.Component<Props> {
  render() {
    const { widget, checked = false, label = '' } = this.props;

    let className = 'toggle ';
    if (checked)
      className += 'checked ';

    return (
      <div className={className}>
        <div className="title">{label}</div>
        <div className="switch" onClick={() => this.props.onChange?.(widget, !checked)} />
      </div>
    );
  }
};