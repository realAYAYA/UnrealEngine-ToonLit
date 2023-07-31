import React from 'react';

type Props = {
  value?: string;
  onChange?: (value: string) => void;
};

export class Text extends React.Component<Props> {

  onChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    this.props.onChange?.(e.target.value);
  }

  render() {
    const { value } = this.props;

    return (
      <div className="widget-text-wrapper">
        <input className="widget-text" type="textbox" value={value ?? ''} onChange={this.onChange} />
      </div>
    );
  }
}
