import * as React from 'react';
import { classNamesFunction, IStyleFunctionOrObject, styled, IStyleFunction } from '@fluentui/react/lib/Utilities';
import { ITheme, IStyle } from '@fluentui/react/lib/Styling';

/**
 * The component props.
 */
export interface IMarkdownHeaderProps {
  as?: keyof React.ReactHTML;
  children?: React.ReactNode;
  className?: string;
  id?: string;

  theme?: ITheme;
  styles?: IStyleFunctionOrObject<IMarkdownHeaderStyleProps, IMarkdownHeaderStyles>;
}

/**
 * The getStyles props contract.
 */
export type IMarkdownHeaderStyleProps = {
  as: string;
  className?: string;
};

/**
 * The styles produced by getStyles.
 */
export interface IMarkdownHeaderStyles {
  root: IStyle;
}

const getStyles: IStyleFunction<IMarkdownHeaderStyleProps, IMarkdownHeaderStyles> = props => {
  const { className } = props;
  return {
    root: [
      {
        fontSize: "16",
        fontWeight: 600,
        padding: 0,
        margin: 0,
        marginTop: '32px',
        marginBottom: '12px',

        selectors: {
          '&:first-child': {
            marginTop: 0
          }
        }
      },

      props.as === 'h1' && [
        {
          fontSize: "32",
          marginBottom: '28px'
        }
      ],

      props.as === 'h2' && [
        {
          fontSize: "24",
          marginBottom: '20px'
        }
      ],

      props.as === 'h3' && [
        {
          fontSize: "20",
          marginBottom: '8px'
        }
      ],
      className
    ]
  };
};

const getClassNames = classNamesFunction<IMarkdownHeaderStyleProps, IMarkdownHeaderStyles>();

const MarkdownHeaderBase: React.StatelessComponent<IMarkdownHeaderProps> = props => {
  const { as: RootType = 'h1', children, id, styles, className } = props;

  const classNames = getClassNames(styles, { as: RootType, className });
  return (
    <RootType className={classNames.root} id={id}>
      {children}
    </RootType>
  );
};

export const MarkdownHeader: React.StatelessComponent<IMarkdownHeaderProps> = styled<
  IMarkdownHeaderProps,
  IMarkdownHeaderStyleProps,
  IMarkdownHeaderStyles
>(MarkdownHeaderBase, getStyles);
