// Copyright Epic Games, Inc. All Rights Reserved.
import { DetailsList, DetailsListLayoutMode, IColumn, IconButton, Label, Modal, PrimaryButton, SelectionMode, Stack, Text } from '@fluentui/react';
import React, { useState } from 'react';
import backend from '../backend';
import { GetAgentSoftwareChannelResponse, GetServerInfoResponse } from '../backend/Api';
import dashboard from '../backend/Dashboard';
import { hordeClasses } from '../styles/Styles';


export const VersionModal: React.FC<{ show: boolean, onClose: () => void }> = ({ show, onClose }) => {

   const [version, setVersion] = useState<{ serverInfo?: GetServerInfoResponse, agentInfo?: GetAgentSoftwareChannelResponse, querying?: boolean }>({});

   type GeneralItem = {
      name: string;
      value?: string;
   }

   const close = () => {
      onClose();
   };

   if (!version.serverInfo && !version.querying) {

      setVersion({ querying: true });

      (async () => {
         let agentInfo: GetAgentSoftwareChannelResponse | undefined;

         const serverInfo = await backend.getServerInfo();

         try {
            if (dashboard.hordeAdmin) {
               agentInfo = await backend.getAgentSoftwareChannel();
            }
         } catch (error) {
            console.error(`Unable to get agent version: ${error}`);
         }

         setVersion({ serverInfo: serverInfo, agentInfo: agentInfo })
      })()

   }

   const versionItems: GeneralItem[] = [];

   const columns = [
      { key: 'column1', name: 'Name', fieldName: 'name', minWidth: 100, maxWidth: 100 },
      { key: 'column2', name: 'Value', fieldName: 'value', minWidth: 100, maxWidth: 200 },
   ];

   const dashboardVersion: string | undefined = process?.env?.REACT_APP_VERSION_INFO;
   if (dashboardVersion) {
      versionItems.push({ name: "Dashboard", value: dashboardVersion });
   }

   versionItems.push({ name: "Server", value: version.serverInfo?.serverVersion ?? "Querying" });

   if (dashboard.hordeAdmin) {
      versionItems.push({ name: "Agent", value: version.agentInfo?.version ?? "Querying" });
   }

   const onRenderItemColumn = (item: GeneralItem, index?: number, columnIn?: IColumn) => {

      const column = columnIn!;

      // simple cases
      switch (column.name) {
         case 'Name':
            return <Text variant="medium" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{item.name}:</Text>
         case 'Value':
            if (item.value) {
               return <Text variant="medium" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{item.value}</Text>
            }
      }

      return null;
   }


   return <Modal className={hordeClasses.modal} isOpen={show} styles={{ main: { padding: 4, width: 640 } }} onDismiss={() => { if (show) close() }}>
      <Stack tokens={{ childrenGap: 12 }}>
         <Stack horizontal styles={{ root: { paddingLeft: 12 } }}>
            <Stack grow style={{ paddingTop: 12, paddingLeft: 8 }}>
               <Label style={{ fontSize: 18, fontWeight: 400, fontFamily: "Horde Open Sans SemiBold" }}>Horde Versions</Label>
            </Stack>
            <Stack style={{ paddingRight: 4, paddingTop: 4 }}>
               <IconButton
                  iconProps={{ iconName: 'Cancel' }}
                  ariaLabel="Close popup modal"
                  onClick={() => { close(); }}
               />
            </Stack>
         </Stack>

         <Stack tokens={{ childrenGap: 32 }} style={{ paddingLeft: 8 }}>

            <Stack horizontal>
               <Stack style={{ minWidth: 600 }}>
                  <DetailsList
                     items={versionItems}
                     columns={columns}
                     setKey="set"
                     layoutMode={DetailsListLayoutMode.justified}
                     isHeaderVisible={false}
                     selectionMode={SelectionMode.none}
                     onRenderItemColumn={onRenderItemColumn}
                  />
               </Stack>
            </Stack>

         </Stack>

         <Stack styles={{ root: { padding: 12 } }}>
            <Stack horizontal tokens={{ childrenGap: 16 }} styles={{ root: { paddingTop: 12, paddingLeft: 8, paddingBottom: 8 } }}>
               <Stack grow />
               <PrimaryButton text="Ok" disabled={false} onClick={() => { close(); }} />
            </Stack>
         </Stack>
      </Stack>
   </Modal>;

};
