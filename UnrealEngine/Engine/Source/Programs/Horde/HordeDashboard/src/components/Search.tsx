// Copyright Epic Games, Inc. All Rights Reserved.

import { Callout, DirectionalHint, FontWeights, Link, List, mergeStyleSets, SearchBox, Stack } from '@fluentui/react';
import React, { useState } from 'react';
import { getHordeTheme } from '../styles/theme';

// https://github.com/OfficeDev/@fluentui/react/tree/0afa4fab0b43f3449398c63862370d55f70cb760/packages/@fluentui/react/src/components/Callout


let _styles: any;

const getStyles = () => {

   const theme = getHordeTheme();

   const styles = _styles ?? mergeStyleSets({

      buttonArea: {
          verticalAlign: 'top',
          display: 'inline-block',
          textAlign: 'center',
          margin: '0 100px',
          minWidth: 130,
          height: 32
      },
      container: {
          overflow: 'auto',
          maxHeight: 500
      },
      callout: {
          maxWidth: 300
      },
      header: {
          padding: '18px 24px 12px'
      },
      title: [
          theme.fonts.xLarge,
          {
              margin: 0,
              fontWeight: FontWeights.semilight
          }
      ],
      inner: {
          height: '100%',
          padding: '0 24px 20px'
      },
      actions: {
          position: 'relative',
          marginTop: 20,
          width: '100%',
          whiteSpace: 'nowrap'
      },
      subtext: [
          theme.fonts.small,
          {
              margin: 0,
              fontWeight: FontWeights.semilight
          }
      ],
      link: [
          theme.fonts.medium,
          {
              color: theme.palette.neutralPrimary
          }
      ]
   });
   
   _styles = styles;
   return styles;
}



type SearchItem = {
    text: string;

}


const onRenderCell = (item?: SearchItem, index?: number, isScrolling?: boolean): JSX.Element => {

    return (<Link onClick={() => console.log("Click!")}>Test</Link>);
};

const SearchList: React.FC = () => {

    const searchItems: SearchItem[] = [];

    searchItems.push({
        text: "test"
    });

    return (

        <List
            items={searchItems}
            onRenderCell={onRenderCell}
            data-is-focusable={true}
        />
    );
};


const searchRef = React.createRef<HTMLDivElement>();

export const Search: React.FC = () => {

   const [visible, setVisible] = useState(false);
   
   const styles = getStyles();

    return (
        <Stack>
            <div className={styles.buttonArea} ref={searchRef}>

                <SearchBox placeholder="Search" styles={{ iconContainer: { fontSize: 12 }, root: { width: 200, height: 24, fontSize: 12 } }}
                    onEscape={ev => {
                        console.log('Custom onEscape Called');
                    }}
                    onClear={ev => {
                        console.log('Custom onClear Called');
                    }}
                    onChange={(_, newValue) => console.log('SearchBox onChange fired: ' + newValue)}
                    onSearch={newValue => console.log('SearchBox onSearch fired: ' + newValue)}
                    onFocus={() => { setVisible(true); console.log('onFocus called'); }}
                    onBlur={() => { setVisible(false); console.log('onBlur called'); }}
                />

            </div>
            <div className={styles.container} data-is-scrollable={true}>
                <Callout isBeakVisible={false}
                    onDismiss={(ev) => console.log(ev)}
                    hidden={!visible}
                    target={searchRef?.current}
                    className={styles.callout}
                    role="alertdialog"
                    gapSpace={0}
                    setInitialFocus={false}
                    preventDismissOnLostFocus={true}
                    directionalHint={DirectionalHint.bottomLeftEdge}>
                    <div className={styles.inner}>

                        <SearchList />
                    </div>

                </Callout>
            </div>
        </Stack>
    );
};