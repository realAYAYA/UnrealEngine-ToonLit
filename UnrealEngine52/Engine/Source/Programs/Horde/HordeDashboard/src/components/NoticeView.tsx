// Copyright Epic Games, Inc. All Rights Reserved.

import { DefaultButton, DetailsList, DetailsListLayoutMode, Dialog, DialogFooter, DialogType, IColumn, Icon, Modal, PrimaryButton, SelectionMode, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import moment from "moment-timezone";
import React, { useState } from "react";
import backend from "../backend";
import { GetNoticeResponse } from "../backend/Api";
import dashboard from "../backend/Dashboard";
import notices from "../backend/Notices";
import { useWindowSize } from "../base/utilities/hooks";
import { displayTimeZone } from "../base/utilities/timeUtils";
import { hordeClasses, modeColors } from "../styles/Styles";
import { Breadcrumbs } from "./Breadcrumbs";
import { TopNav } from "./TopNav";


const NoticePanel: React.FC = observer(() => {

   const [state, setState] = useState<{ showEditor?: boolean, editNotice?: GetNoticeResponse }>({});

   // subscribe
   if (notices.updated) { };

   const columns = [
      { key: 'column_message', name: 'Message', minWidth: 1024, maxWidth: 1024, isResizable: false },
      { key: 'column_starttime', name: 'Start Time', minWidth: 160, maxWidth: 160, isResizable: false },
      { key: 'column_finishtime', name: 'Finish Time', minWidth: 160, maxWidth: 160, isResizable: false },
      { key: 'column_createdby', name: 'Created By', minWidth: 240, maxWidth: 240, isResizable: false },
   ];

   let allNotices = [...notices.allNotices];

   const userNotice = allNotices.find(n => n.active && n.createdByUser);

   const renderItem = (item: GetNoticeResponse, index?: number, column?: IColumn) => {

      if (column?.name === "Message") {

         if (item.createdByUser) {
            return <div onClick={() => { setState({ showEditor: true, editNotice: item }) }} style={{ "cursor": "pointer" }}>
               <Stack horizontal tokens={{ childrenGap: 24 }}>
                  <Icon style={{ paddingTop: 2 }} iconName="Edit" />
                  <Text>{item.message}</Text>
               </Stack>
            </div>
         }

         return <Stack horizontal tokens={{ childrenGap: 24 }}>
            <Icon style={{ paddingTop: 2 }} iconName="Build" />
            <Text>{item.scheduledDowntime ? "Horde Scheduled Downtime" : item.message ?? "???"}</Text>
         </Stack>
      }
      function getDisplayTime(date?: Date) {
         return date ? moment(date).tz(displayTimeZone()).format(dashboard.display24HourClock ? "MMM Do, HH:mm z" : "MMM Do, h:mma z") : "";
      }

      if (column?.name === "Start Time") {
         if (!item.startTime) {
            return null;
         }
         return <Text>{getDisplayTime(item.startTime as Date)}</Text>
      }

      if (column?.name === "Finish Time") {
         if (!item.finishTime) {
            return null;
         }
         return <Text>{getDisplayTime(item.finishTime as Date)}</Text>
      }

      if (column?.name === "Created By") {
         return <Text>{item.createdByUser?.name ?? "Horde Configuration"}</Text>
      }

      return null;
   };

   return (<Stack>
      {!!state.showEditor && <NoticeEditor noticeIn={state.editNotice} onClose={() => {
         setState({})
      }} />}
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack tokens={{ childrenGap: 12 }} >
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Notices</Text>
               </Stack>
               <Stack grow />
               <Stack>
                  <PrimaryButton styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }} onClick={() => setState({ showEditor: true, editNotice: userNotice })}>Set Notice</PrimaryButton>
               </Stack>
            </Stack>
            <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "calc(100vh - 312px)" }} data-is-scrollable={true}>
               <Stack>
                  <DetailsList
                     items={allNotices}
                     columns={columns}
                     selectionMode={SelectionMode.none}
                     layoutMode={DetailsListLayoutMode.justified}
                     compact={true}
                     onRenderItemColumn={renderItem}
                  />
               </Stack>
            </div>
         </Stack>
      </Stack>
   </Stack>);
});


export const NoticeView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Server Notices' }]} />
      <Stack horizontal>
         <div key={`windowsize_noticeview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 896, flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1778, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%", height: 'calc(100vh - 228px)' }} tokens={{ childrenGap: 18 }}>
                     <NoticePanel />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};

const NoticeEditor: React.FC<{ noticeIn?: GetNoticeResponse, onClose: () => void }> = ({ noticeIn, onClose }) => {

   const [submitting, setSubmitting] = useState(false);
   const [confirmDelete, setConfirmDelete] = useState(false);

   let notice = noticeIn ? { ...noticeIn } : { id: undefined, message: undefined };

   const onSave = async () => {

      try {

         setSubmitting(true);
         if (notice.id) {
            await backend.updateNotice({ id: notice.id, message: notice.message });
         } else {
            await backend.createNotice({ message: notice.message! })
         }
         setSubmitting(false);
         onClose();
         notices.poll();

      } catch (reason) {
         console.error(reason);
         setSubmitting(false);
      }

   }

   const onDelete = async () => {

      try {
         setSubmitting(true);
         await backend.deleteNotice(notice.id!);
         setSubmitting(false);
         onClose();
         notices.poll();
      } catch (reason) {
         console.error(reason);
         setSubmitting(false);
      }
   }

   if (submitting) {
      return <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 400, hasBeenOpened: false, top: "120px", position: "absolute" } }} >
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
      </Modal>
   }

   if (confirmDelete) {
      return <Dialog
         hidden={false}
         onDismiss={() => setConfirmDelete(false)}
         minWidth={400}
         dialogContentProps={{
            type: DialogType.normal,
            title: `Confirm Deletion`,
         }}
         modalProps={{ isBlocking: true, topOffsetFixed: true, styles: { main: { padding: 8, width: 400, hasBeenOpened: false, top: "120px", position: "absolute" } } }} >
         <DialogFooter>
            <PrimaryButton onClick={() => { setConfirmDelete(false); onDelete() }} text="Delete" />
            <DefaultButton onClick={() => setConfirmDelete(false)} text="Cancel" />
         </DialogFooter>
      </Dialog>
   }


   return <Modal className={hordeClasses.modal} isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 800, hasBeenOpened: false, top: "120px", position: "absolute" } }} >

      <Stack style={{ padding: 8 }}>
         <Stack style={{ paddingBottom: 16 }}>
            <Text variant="mediumPlus" style={{ fontFamily: "Horde Open Sans SemiBold" }}>{noticeIn ? "Edit Notice" : "New Notice"}</Text>
         </Stack>
         <Stack tokens={{ childrenGap: 8 }} style={{ padding: 8 }}>
            <TextField label="Message" autoComplete="off" placeholder="Please enter the notice message" defaultValue={notice.message ? notice.message : ""} onChange={(ev, value) => { notice.message = value ?? "" }} />
         </Stack>

         <Stack horizontal style={{ paddingTop: 64 }}>
            {!!noticeIn && <PrimaryButton style={{ backgroundColor: "red", border: 0 }} onClick={() => setConfirmDelete(true)} text="Delete Notice" />}
            <Stack grow />
            <Stack horizontal tokens={{ childrenGap: 28 }}>
               <PrimaryButton onClick={() => onSave()} text="Save" />
               <DefaultButton onClick={() => onClose()} text="Cancel" />
            </Stack>
         </Stack>
      </Stack>
   </Modal>

}