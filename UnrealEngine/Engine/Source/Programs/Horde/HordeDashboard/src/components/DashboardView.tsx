// Copyright Epic Games, Inc. All Rights Reserved.

import { ColorPicker, DefaultButton, DetailsList, DetailsListLayoutMode, DialogFooter, IColorPickerStyles, IColumn, Label, List, Modal, PrimaryButton, SelectionMode, Stack, Text, Toggle } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useState } from 'react';
import { DashboardPreference } from '../backend/Api';
import dashboard, { StatusColor, WebBrowser } from '../backend/Dashboard';
import { useWindowSize } from '../base/utilities/hooks';
import { hordeClasses, modeColors } from '../styles/Styles';
import { Breadcrumbs } from './Breadcrumbs';
import { TopNav } from './TopNav';



const colorBlind1 = new Map<StatusColor, string>([
   [StatusColor.Success, "#37A862"],
   [StatusColor.Warnings, "#EECC17"],
   [StatusColor.Failure, "#FD827D"],
   [StatusColor.Running, "#7A6FF1"]
]);


const colorBlind2 = new Map<StatusColor, string>([
   [StatusColor.Success, "#208FA3"],
   [StatusColor.Warnings, "#FFB000"],
   [StatusColor.Failure, "#E8384F"],
   [StatusColor.Running, "#AA71FF"]
]);


const statusText = new Map<StatusColor, string>([
   [StatusColor.Success, "Success"],
   [StatusColor.Warnings, "Warnings"],
   [StatusColor.Failure, "Failure"],
   [StatusColor.Waiting, "Waiting"],
   [StatusColor.Ready, "Ready"],
   [StatusColor.Skipped, "Skipped"],
   [StatusColor.Aborted, "Aborted"],
   [StatusColor.Running, "Running"]
]);

const orderedStatus = [
   StatusColor.Success,
   StatusColor.Warnings,
   StatusColor.Failure,
   StatusColor.Running
];

const statusPrefs = new Map<StatusColor, DashboardPreference>([
   [StatusColor.Success, DashboardPreference.ColorSuccess],
   [StatusColor.Warnings, DashboardPreference.ColorWarning],
   [StatusColor.Failure, DashboardPreference.ColorError],
   [StatusColor.Running, DashboardPreference.ColorRunning]
]);


const ColorPreferenceDialog: React.FC<{ shown: boolean, statusIn: StatusColor, onClose: () => void }> = ({ shown, statusIn, onClose }) => {

   let [state, setState] = useState<{ status: StatusColor, colors: Map<StatusColor, string> }>({ status: statusIn, colors: new Map<StatusColor, string>() });

   if (!shown) {
      return null;
   }

   const defaultStatusColors = dashboard.getDefaultStatusColors();

   let anyMissing = false;

   statusPrefs.forEach((value, key) => {

      if (!state.colors.has(key)) {

         anyMissing = true;
      }

   });

   if (anyMissing) {

      statusPrefs.forEach((value, key) => {

         if (state.colors.has(key)) {
            return;
         }

         const pref = dashboard.getPreference(value);
         const color = pref ? pref : defaultStatusColors.get(key)!;

         state.colors.set(key, color);

      })

      setState({ status: state.status, colors: state.colors });
      return null;
   }

   const status = state.status;
   let color = state.colors.get(status)!;

   const setColor = (c: string) => {

      state.colors.set(status, c);
      setState({ status: state.status, colors: state.colors });
   }

   const setStatus = (status: StatusColor) => {

      setState({ status: status, colors: state.colors });
   }

   // tol: https://personal.sron.nl/~pault/
   const tolColors = [
      defaultStatusColors.get(status)!,
      "#332288",
      "#117733",
      "#44AA99",
      "#88CCEE",
      "#DDCC77",
      "#CC6677",
      "#AA4499",
      "#882255",
      "#648FFF",
      "#785EF0",
      "#DC267F",
      "#FE6100",
      "#FFB000"
   ];

   const suggestions = tolColors.map(c => {
      return <DefaultButton key={`color_${c}`} style={{ backgroundColor: c, width: 80 }} text={statusText.get(status)} onClick={() => { setColor(c) }} />
   })

   const colorPicker = () => {
      const colorPickerStyles: Partial<IColorPickerStyles> = {
         panel: { padding: 12 },
         root: {
            maxWidth: 352,
            minWidth: 352,
         },
         colorRectangle: { height: 268 },
      };


      return <Stack className="horde-no-darktheme">
         <ColorPicker
            color={color}
            onChange={(ev, color) => { setColor(`#${color.hex}`) }}
            alphaType="none"
            showPreview={true}
            styles={colorPickerStyles}
         />
      </Stack >;

   }

   const BadgeColors: React.FC = () => {

      const buttons = orderedStatus.map(status => {

         return <div className="horde-no-darktheme">
            <Stack className={hordeClasses.badgeNoIcon} key={`badge_${status}`}> <DefaultButton style={{ backgroundColor: state.colors.get(status) }} text={statusText.get(status)} onClick={() => { setStatus(status) }} />
            </Stack>
         </div>
      });

      return <Stack horizontal tokens={{ childrenGap: 12 }}>{buttons}</Stack>;

   }


   return <Modal isOpen={shown} isBlocking={true} styles={{ main: { padding: 8, width: 570, } }} className={hordeClasses.modal} onDismiss={() => { onClose() }}>
      <Stack style={{ paddingLeft: 12, paddingRight: 12 }}>
         <Stack style={{ padding: 12 }}>
            <Stack style={{ paddingBottom: 24 }}>
               <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{`Set Status Colors`}</Text>
            </Stack>

            <Stack tokens={{ childrenGap: 12 }}>
               <Stack horizontalAlign="center">
                  <BadgeColors />
               </Stack>
               <Stack>
                  <Stack style={{ paddingBottom: 12 }}>
                     <Text styles={{ root: { fontSize: "15px", fontFamily: "Horde Open Sans SemiBold" } }}>{`Profiles`}</Text>
                  </Stack>
                  <Stack style={{ paddingLeft: 12 }} horizontalAlign="center">
                     <Stack horizontal tokens={{ childrenGap: 12 }}>
                        <DefaultButton style={{ height: 24 }} text="Default" onClick={() => {
                           setState({ status: state.status, colors: new Map(defaultStatusColors) });
                        }} />
                        <DefaultButton style={{ height: 24 }} text="Color Blind 1" onClick={() => {
                           setState({ status: state.status, colors: new Map(colorBlind1) });
                        }} />
                        <DefaultButton style={{ height: 24 }} text="Color Blind 2" onClick={() => {
                           setState({ status: state.status, colors: new Map(colorBlind2) });
                        }} />
                     </Stack>
                  </Stack>

               </Stack>
               <Stack>
                  <Stack style={{ paddingTop: 8 }}>
                     <Text styles={{ root: { fontSize: "15px", fontFamily: "Horde Open Sans SemiBold" } }}>{`Custom Color`}</Text>
                  </Stack>
                  <Stack horizontal tokens={{ childrenGap: 48 }}>
                     {colorPicker()}
                     <Stack className="horde-no-darktheme" style={{ paddingTop: 8 }}>
                        <Stack style={{ paddingTop: 2 }} className={hordeClasses.badgeNoIcon} tokens={{ childrenGap: 6 }}>
                           {suggestions}
                        </Stack>
                     </Stack>
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
         <Stack style={{ paddingBottom: 18, marginRight: 12 }} >
            <DialogFooter>
               <PrimaryButton text="Set" onClick={() => {
                  state.colors.forEach((value, key) => {
                     const pref = statusPrefs.get(key)!;
                     dashboard.setStatusColor(pref, value);
                  })
                  onClose();
               }} />
               <DefaultButton text="Cancel" onClick={() => onClose()} />
            </DialogFooter>

         </Stack>
      </Stack>
   </Modal>

}

const GeneralPanel: React.FC = observer(() => {

   let [colorState, setColorState] = useState<{ status?: StatusColor }>({});

   const defaultStatusColors = dashboard.getDefaultStatusColors();

   type GeneralItem = {
      name: string;
      value?: string;
   }

   // subscribe
   if (dashboard.updated) { }

   const columns = [
      { key: 'column1', name: 'Name', fieldName: 'name', minWidth: 100, maxWidth: 100 },
      { key: 'column2', name: 'Value', fieldName: 'value', minWidth: 100, maxWidth: 200 },
   ];


   const items: GeneralItem[] = [];

   items.push({
      name: "Roles"
   });


   const p4user = dashboard.p4user;

   if (p4user) {

      items.push({
         name: "Perforce",
         value: p4user
      });

   }

   const BadgeColors: React.FC = () => {

      const buttons = orderedStatus.map(status => {

         const dashboardPref = statusPrefs.get(status)!;
         const pref = dashboard.getPreference(dashboardPref);
         const color = pref ? pref : defaultStatusColors.get(status)!;

         return <Stack className={hordeClasses.badgeNoIcon} key={`badge_${status}`}> <DefaultButton style={{ backgroundColor: color }} text={statusText.get(status)} onClick={() => { setColorState({ status: status }) }} />
         </Stack>
      });

      return <Stack horizontal tokens={{ childrenGap: 12 }}>{buttons}</Stack>;

   }


   const onRenderItemColumn = (item: GeneralItem, index?: number, columnIn?: IColumn) => {

      const column = columnIn!;

      // simple cases
      switch (column.name) {
         case 'Name':
            return <Text styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{item.name}:</Text>
         case 'Value':
            if (item.value)
               return <Text >{item.value}</Text>
            break;
      }

      // roles list
      if (item.name === "Roles") {
         const roles = dashboard.roles.map(c => { return { name: c.value } });

         return <Stack styles={{
            root: {
               selectors: {
                  '.ms-List-cell': {
                     color: modeColors.text,
                     height: 12,
                     lineHeight: 12,
                     paddingTop: 2,
                     paddingBottom: 12,
                     minHeight: "unset",
                     fontFamily: "Horde Open Sans SemiBold"
                  }
               }
            }
         }}><List items={roles} /></Stack>;
      }

      return null;
   }

   return (<Stack>
      {colorState.status !== undefined && <ColorPreferenceDialog shown={true} statusIn={colorState.status} onClose={() => setColorState({})} />}
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack tokens={{ childrenGap: 12 }} style={{ height: 'calc(100vh - 200px)' }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{dashboard.email}</Text>

            <Stack style={{ padding: 12 }}>
               <Stack horizontal tokens={{ childrenGap: 96 }}>
                  <Stack style={{ minWidth: 600 }}>
                     <Stack style={{ paddingBottom: 12 }}>
                        <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Account</Text>
                     </Stack>
                     <DetailsList
                        items={items}
                        columns={columns}
                        setKey="set"
                        layoutMode={DetailsListLayoutMode.justified}
                        isHeaderVisible={false}
                        selectionMode={SelectionMode.none}
                        onRenderItemColumn={onRenderItemColumn}
                     />
                  </Stack>
                  <Stack style={{ paddingLeft: 12 }} tokens={{ childrenGap: 12 }}>
                     <Stack style={{ paddingBottom: 4 }}>
                        <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Settings</Text>
                     </Stack>
                     <Stack style={{ paddingLeft: 12 }}>
                        <Toggle label="Dark Mode" inlineLabel={true} defaultChecked={dashboard.darktheme} onChange={(ev, checked) => {
                           dashboard.setDarkTheme(checked ? true : false);
                           setColorState({ ...colorState });
                        }} />
                        <Toggle label="Display UTC Times" inlineLabel={true} defaultChecked={dashboard.displayUTC} onChange={(ev, checked) => {
                           dashboard.setDisplayUTC(checked ? true : false);
                        }} />
                        <Toggle label="24 Hour Clock" inlineLabel={true} defaultChecked={dashboard.display24HourClock} onChange={(ev, checked) => {
                           dashboard.setDisplay24HourClock(checked ? true : false);
                        }} />
                        <Toggle label="Prefer Compact Views" inlineLabel={true} defaultChecked={dashboard.compactViews} onChange={(ev, checked) => {
                           dashboard.setCompactViews(checked ? true : false);
                        }} />
                        <Toggle label="Left Align Log View" inlineLabel={true} defaultChecked={dashboard.leftAlignLog} onChange={(ev, checked) => {
                           dashboard.setLeftAlignLog(checked ? true : false);
                        }} />
                     </Stack>

                     <Stack className="horde-no-darktheme" tokens={{ childrenGap: 12 }}>
                        <Label>Status Colors</Label>
                        <BadgeColors />
                     </Stack>

                     <Stack style={{ paddingBottom: 4, paddingTop: 12 }}>
                        <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Experimental</Text>
                     </Stack>

                     <Stack style={{ paddingLeft: 12 }}>

                        <Toggle label="General Features" inlineLabel={true} defaultChecked={dashboard.experimentalFeatures} onChange={(ev, checked) => {
                           dashboard.experimentalFeatures = checked ? true : false;
                        }} />
                        {dashboard.browser !== WebBrowser.Chromium && <Toggle label="Data Caching (Page Reload Required)" inlineLabel={true} defaultChecked={dashboard.localCache} onChange={(ev, checked) => {
                           dashboard.setLocalCache(checked ? true : false);
                        }} />}
                     </Stack>


                  </Stack>
               </Stack>

            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});

export const DashboardView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Preferences' }]} />
      <Stack horizontal>
         <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 900, flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1800, paddingTop: 16, marginLeft: 4 }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%" }} tokens={{ childrenGap: 18 }}>
                     {dashboard.available && <GeneralPanel />}
                     {!dashboard.available && <Text>User settings unavailable, try logging out and back in</Text>}
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};