// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, IColumn, SelectionZone, PrimaryButton, Selection, SelectionMode, Stack, Text, DefaultButton, Dialog, DialogFooter, DialogType, Modal, Spinner, SpinnerSize } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect, useState } from "react";
import backend from "../../backend";
import { GetPendingAgentResponse } from "../../backend/Api";
import { PollBase } from "../../backend/PollBase";
import { useWindowSize } from "../../base/utilities/hooks";
import { getHordeStyling } from "../../styles/Styles";
import { Breadcrumbs } from "../Breadcrumbs";
import { TopNav } from "../TopNav";
import ErrorHandler from "../ErrorHandler";

class AgentRequestsHandler extends PollBase {

   constructor(pollTime = 5000) {

      super(pollTime);

   }

   clear() {
      this.initial = true;
      this.requests = [];
      this.selectedAgents = [];
      this.selection = new Selection({ onSelectionChanged: () => { this.onSelectionChanged(this.selection.getSelection() as any) }, selectionMode: SelectionMode.multiple })
      super.stop();
   }

   async poll(): Promise<void> {

      try {

         const requests = await backend.getAgentRegistrationRequests();
         this.requests = requests.agents;
         this.initial = false;
         this.setUpdated();

      } catch (err) {

      }
   }

   onSelectionChanged(selection: GetPendingAgentResponse[] | undefined) {
      this.selectedAgents = selection ?? [];
      this.setUpdated();
   }

   selection = new Selection({ onSelectionChanged: () => { this.onSelectionChanged(this.selection.getSelection() as any) }, selectionMode: SelectionMode.multiple })

   selectedAgents: GetPendingAgentResponse[] = [];

   requests: GetPendingAgentResponse[] = [];

   initial = true;
}

const handler = new AgentRequestsHandler();

const AgentsPanel: React.FC = observer(() => {

   const [confirmRegister, setConfirmRegister] = useState(false);
   const [submitting, setSubmitting] = useState(false);

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   const { modeColors } = getHordeStyling();

   // subscribe
   if (handler.updated) { };

   const onRegister = async () => {

      const agents = handler.selectedAgents.map(a => { return { key: a.key } })

      try {
         setSubmitting(true);
         await backend.registerAgents({ agents: agents });
         setSubmitting(false);
         handler.stop();
         handler.clear();
         handler.start();
      } catch (reason) {
         console.error(reason);

         ErrorHandler.set({
            reason: reason,
            title: `Error Enrolling Agents`,
            message: `There was an error enrolling agents, reason: "${reason}"`

         }, true);

         setSubmitting(false);
      }
   }

   const columns = [
      { key: 'column_hostname', name: 'Hostname', fieldName: 'hostName', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column_description', name: 'Description', fieldName: 'description', minWidth: 120, maxWidth: 120, isResizable: false },
   ];

   let requests = [...handler.requests];

   requests = requests.sort((a, b) => a.hostName.localeCompare(b.hostName));

   const renderItem = (item: any, index?: number, column?: IColumn) => {
      if (!column?.fieldName) {
         return null;
      }
      return <Text style={{ color: modeColors.text }}>{item[column?.fieldName]}</Text>
   };

   const { hordeClasses } = getHordeStyling();

   return <Stack>
      {submitting && <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 400, hasBeenOpened: false, top: "120px", position: "absolute" } }} >
         <Stack style={{ paddingTop: 32 }}>
            <Stack tokens={{ childrenGap: 24 }} styles={{ root: { padding: 8 } }}>
               <Stack horizontalAlign="center">
                  <Text variant="mediumPlus">Please wait...</Text>
               </Stack>
               <Stack verticalAlign="center" style={{ paddingBottom: 32 }}>
                  <Spinner size={SpinnerSize.large} />
               </Stack>
            </Stack>
         </Stack>
      </Modal>}
      {confirmRegister &&
         <Dialog
            hidden={false}
            onDismiss={() => setConfirmRegister(false)}
            minWidth={612}
            dialogContentProps={{
               type: DialogType.normal,
               title: `Enroll Agents`,
               subText: `Confirm enrolling agents: ${handler.selectedAgents.map(a => a.hostName).join(", ")}`
            }}
            modalProps={{ isBlocking: true, topOffsetFixed: true, styles: { main: { padding: 8, width: 400, hasBeenOpened: false, top: "120px", position: "absolute" } } }} >
            <Stack style={{ height: "18px" }} />
            <DialogFooter>
               <PrimaryButton onClick={() => { setConfirmRegister(false); onRegister() }} text="Register" />
               <DefaultButton onClick={() => setConfirmRegister(false)} text="Cancel" />
            </DialogFooter>
         </Dialog>
      }
      {<Stack style={{ paddingBottom: 12 }}>
         <Stack verticalAlign="center">
            {!!requests.length && !handler.initial && <Stack horizontalAlign="end">
               <PrimaryButton disabled={!handler.selectedAgents.length} styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }} onClick={() => setConfirmRegister(true)}>Enroll Agents</PrimaryButton>
            </Stack>}
            {!requests.length && !handler.initial && <Stack horizontalAlign="center">
               <Text variant="mediumPlus">No Agent Enrollment Requests Found</Text>
            </Stack>}
         </Stack>
      </Stack>}

      {!!requests.length && <Stack className={hordeClasses.raised} >
         <Stack styles={{ root: { paddingLeft: 12, paddingRight: 12, paddingBottom: 12, width: "100%" } }} >
            <Stack>
               <SelectionZone selection={handler.selection}>
                  <DetailsList
                     setKey="set"
                     items={requests}
                     columns={columns}
                     layoutMode={DetailsListLayoutMode.justified}
                     compact={true}
                     selectionMode={SelectionMode.multiple}
                     selection={handler.selection}
                     selectionPreservedOnEmptyClick={true}
                     onRenderItemColumn={renderItem}
                     enableUpdateAnimations={false}
                     onShouldVirtualize={() => false}
                  />
               </SelectionZone>
            </Stack>
         </Stack>
      </Stack>}
   </Stack >
});


export const AgentRequestsView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
   const centerAlign = vw / 2 - 720;

   const { hordeClasses, modeColors } = getHordeStyling();

   const key = `windowsize_view_${windowSize.width}_${windowSize.height}`;

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Agent Enrollment' }]} />
      <Stack styles={{ root: { width: "100%", backgroundColor: modeColors.background } }}>
         <Stack style={{ width: "100%", backgroundColor: modeColors.background }}>
            <Stack style={{ position: "relative", width: "100%", height: 'calc(100vh - 148px)' }}>
               <div style={{ overflowX: "auto", overflowY: "visible" }}>
                  <Stack horizontal style={{ paddingTop: 30, paddingBottom: 48 }}>
                     <Stack key={`${key}`} style={{ paddingLeft: centerAlign }} />
                     <Stack style={{ width: 1440 }}>
                        <AgentsPanel />
                     </Stack>
                  </Stack>
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};

