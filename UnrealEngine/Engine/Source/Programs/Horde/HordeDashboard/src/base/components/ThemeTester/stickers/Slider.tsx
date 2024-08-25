import * as React from 'react';
import { Slider, IStackTokens, Stack, IStackStyles } from '@fluentui/react';

const stackStyles: Partial<IStackStyles> = { root: { maxWidth: 300 } };
const stackTokens: IStackTokens = { childrenGap: 20 };

export const SliderSticker: React.FunctionComponent = () => {
  return (
    <Stack tokens={stackTokens} styles={stackStyles}>
      <Slider aria-label="Basic example" />
      <Slider label="Disabled example" min={50} max={500} step={50} defaultValue={300} showValue disabled />
    </Stack>
  );
};
