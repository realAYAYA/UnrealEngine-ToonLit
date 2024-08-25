// Copyright Epic Games, Inc. All Rights Reserved.
import { IconButton, Modal, PrimaryButton, Stack, Text, Label } from '@fluentui/react';
import React from 'react';
import dashboard from '../backend/Dashboard';
import { getHordeStyling } from '../styles/Styles';


export const HelpModal: React.FC<{ show: boolean, onClose: () => void }> = ({ show, onClose }) => {

   const { hordeClasses } = getHordeStyling();

   const close = () => {
      onClose();
   };

   const helpEmail = dashboard.helpEmail ?? "Not Configured";
   const helpSlack = dashboard.helpSlack ?? "Not Configured";

   return <Modal className={hordeClasses.modal} isOpen={show} styles={{ main: { padding: 4, width: 540 } }} onDismiss={() => { if (show) close() }}>
      <Stack tokens={{ childrenGap: 12 }}>
         <Stack horizontal styles={{ root: { paddingLeft: 12 } }}>
            <Stack grow style={{ paddingTop: 12 }}>
               <Label style={{ fontSize: 18, fontWeight: 400, fontFamily: "Horde Open Sans SemiBold" }}>Help and Feedback</Label>
            </Stack>
            <Stack style={{ paddingRight: 4, paddingTop: 4 }}>
               <IconButton
                  iconProps={{ iconName: 'Cancel' }}
                  ariaLabel="Close popup modal"
                  onClick={() => { close(); }}
               />
            </Stack>
         </Stack>


         <Stack tokens={{ childrenGap: 18 }} style={{ paddingLeft: 18 }}>
            <Text style={{ fontSize: 15 }}>Horde feedback and suggestions:</Text>
            <Stack tokens={{ childrenGap: 14 }} style={{ paddingLeft: 12, paddingTop: 2 }}>
               <Stack horizontal tokens={{ childrenGap: 12 }}><Text style={{ fontSize: 15 }}>Email:</Text><Text style={{ fontSize: 15, fontWeight: 400, fontFamily: "Horde Open Sans SemiBold" }}>{helpEmail}</Text></Stack>
               <Stack horizontal tokens={{ childrenGap: 12 }}><Text style={{ fontSize: 15 }}>Slack:</Text><Text style={{ fontSize: 15, fontWeight: 400, fontFamily: "Horde Open Sans SemiBold" }}>{helpSlack}</Text></Stack>
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
