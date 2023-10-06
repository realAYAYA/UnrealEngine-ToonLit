// Copyright Epic Games, Inc. All Rights Reserved.

import { mergeStyleSets, MessageBar, MessageBarType, Separator, Spinner, SpinnerSize, Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useState } from 'react';
import { Link } from 'react-router-dom';
import dashboard from '../backend/Dashboard';
import notices from '../backend/Notices';
import { modeColors } from '../styles/Styles';

export type BreadcrumbItem = {
   text: string;
   link?: string;
}

export const classes = mergeStyleSets({
   crumb: {
      marginLeft: '0px !important',
      marginRight: '0px !important',
      fontFamily: "Horde Open Sans Light",
      color: modeColors.text,
      flexShrink: 1,
      selectors: {
         ':active': {
            textDecoration: 'none'
         },
         ':hover': {
            textDecoration: 'none'
         }
      }
   }
});



export const Breadcrumbs: React.FC<{ items: BreadcrumbItem[], title?: string, suppressHome?: boolean, spinner?: boolean }> = observer((({ items: itemsIn, title, suppressHome, spinner }) => {

   const [hideAlert, setHideAlert] = useState(false);

   if (notices.updated) { }

   // hack fix to stop the entire stack item acting as a link
   function onLinkClick(ev: any, toLink: string | undefined) {
      if (!toLink || toLink === "") {
         ev.preventDefault();
         return false;
      }
   }

   let items: BreadcrumbItem[] = [];
   if (itemsIn?.length) {
      items = [...itemsIn];
   }

   const last = items.pop();

   if (!last) {
      return null;
   }

   if (!suppressHome) {
      if (last.text !== "Home") {
         items.unshift({ text: "Home", link: "/index" });
      }
   }

   document.title = title ?? `Horde - ${last.text}`;

   const topElements = items.map(item => {
      return <Link key={items.indexOf(item)} className={classes.crumb} to={item.link ?? ""} onClick={(ev: any) => { return onLinkClick(ev, item.link); }} style={{ cursor: item.link ? "" : "default" }}>
         <Text variant="medium" className={classes.crumb}>{item.text}</Text>
         <Text variant="medium" styles={{ root: { marginLeft: 7, marginRight: 7 } }} className={classes.crumb}>{"\u203A"}</Text>
      </Link>;
   });

   const bottomLength = last.text?.length ?? 0;
   let bottomFontSize = 28;

   if (bottomLength > 32) {
      bottomFontSize = 24;
   }
   if (bottomLength > 48) {
      bottomFontSize = 22;
   }
   if (bottomLength > 64) {
      bottomFontSize = 20;
   }

   const bottomElement = <Text className={classes.crumb} styles={{ root: { color: modeColors.text, fontSize: bottomFontSize } }}>{last.text}</Text>;

   const alert = notices.alertText;

   return (<Stack>
      <Separator styles={{ root: { fontSize: 0, padding: 0 } }} />
      <Stack>
         <Stack styles={{ root: { userInput: 'all' } }}>
            <Stack tokens={{ childrenGap: 4 }} styles={{ root: { height: 88, padding: 0, paddingLeft: 24, paddingBottom: 8, paddingTop: 8, backgroundColor: modeColors.crumbs, userSelect: 'text' } }}>
               <Stack tokens={{ childrenGap: 0 }} disableShrink={true} styles={{ root: { margin: "auto", width: "100%", maxWidth: 1464 } }}>
                  <Stack horizontal tokens={{ childrenGap: 8, padding: 0 }}>{topElements}</Stack>
                  {
                     <Stack horizontal tokens={{ childrenGap: 18 }}>
                        <Stack>{bottomElement}</Stack>
                        {!!spinner && <Spinner size={SpinnerSize.large} />}
                     </Stack>
                  }
               </Stack>
            </Stack>
         </Stack>
         {!alert && !!dashboard.preview && <Stack horizontalAlign="center" disableShrink={true} style={{ position: "absolute", width: "100%", pointerEvents: "none" }} >
            <Stack style={{ paddingTop: 22 }}>
               <Text variant="large" style={{ fontFamily: "Horde Open Sans Light", opacity: 0.33 }}>Horde Preview</Text>
            </Stack>
         </Stack>}
         {!alert && !!dashboard.development && <Stack horizontalAlign="center" disableShrink={true} style={{ position: "absolute", width: "100%", pointerEvents: "none" }} >
            <Stack style={{ paddingTop: 22 }}>
               <Text variant="large" style={{ fontFamily: "Horde Open Sans Light", opacity: 0.33 }}>Horde Development</Text>
            </Stack>
         </Stack>}

         {!!alert && dashboard.alertSquelch !== alert && <Stack horizontalAlign="center" disableShrink={true} style={{ position: "absolute", width: "100%" }} >
            <Stack horizontal>
               <Stack grow />
               <Stack>
                  <MessageBar onDismiss={() => { dashboard.alertSquelch = alert; setHideAlert(true) }}
                     messageBarType={MessageBarType.severeWarning} isMultiline={false} >
                     <Text variant={"small"} style={{ fontFamily: "Horde Open Sans Bold" }}>{alert}</Text>
                  </MessageBar>
               </Stack>
               <Stack grow />
            </Stack>
         </Stack>}
      </Stack>
   </Stack>
   );
}));

/*
*/