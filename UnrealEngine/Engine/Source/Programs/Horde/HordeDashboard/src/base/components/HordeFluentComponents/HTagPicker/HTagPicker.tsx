import { IBasePickerStyleProps, IBasePickerStyles, initializeComponentRef, ITag, ITagItemProps, ITagPickerProps, styled, TagItem, TagItemSuggestion } from '@fluentui/react';
import * as React from 'react';
import { HBasePicker } from '../HBasePicker/HBasePicker';
import { getStyles } from '../HBasePicker/HBasePicker.styles';

/**
 * {@docCategory TagPicker}
 */
export class HTagPickerBase extends HBasePicker<ITag, ITagPickerProps> {
  public static defaultProps = {
    onRenderItem: (props: ITagItemProps) => <TagItem {...props}>{props.item.name}</TagItem>,
    onRenderSuggestionsItem: (props: ITag) => <TagItemSuggestion>{props.name}</TagItemSuggestion>,
  };

  constructor(props: ITagPickerProps) {
    super(props);
    initializeComponentRef(this);
  }
}

export const HTagPicker = styled<ITagPickerProps, IBasePickerStyleProps, IBasePickerStyles>(
  HTagPickerBase,
  getStyles,
  undefined,
  {
    scope: 'HTagPicker',
  },
);
