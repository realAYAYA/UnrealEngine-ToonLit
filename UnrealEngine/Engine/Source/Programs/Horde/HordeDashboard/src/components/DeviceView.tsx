import { DetailsList, DetailsListLayoutMode, DetailsRow, Dropdown, FocusZone, FocusZoneDirection, getTheme, IColumn, Icon, IconButton, IDetailsListProps, IGroup, Label, Link as FluentLink, mergeStyleSets, Modal, Pivot, PivotItem, PrimaryButton, ScrollablePane, ScrollbarVisibility, SelectionMode, Spinner, SpinnerSize, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import moment from "moment";
import React, { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import backend from "../backend";
import { DevicePoolType, GetDeviceResponse } from "../backend/Api";
import dashboard from "../backend/Dashboard";
import { useWindowSize } from "../base/utilities/hooks";
import { getNiceTime } from "../base/utilities/timeUtils";
import { hordeClasses, modeColors } from "../styles/Styles";
import { Breadcrumbs } from "./Breadcrumbs";
import { DeviceEditor, DeviceHandler, DeviceStatus } from "./DeviceEditor";
import { DeviceInfoModal } from "./DeviceInfoView";
import { TopNav } from "./TopNav";

const handler = new DeviceHandler();

type DeviceItem = {
   device: GetDeviceResponse;
}

let groups: IGroup[] = [];

const theme = getTheme();

export const StatusColors = new Map<DeviceStatus, string>([
   [DeviceStatus.Available, "#52C705"],
   [DeviceStatus.Problem, "#DE4522"],
   [DeviceStatus.Reserved, theme.palette.blueLight],
   [DeviceStatus.Disabled, "#F3F2F1"],
   [DeviceStatus.Maintenance, theme.palette.blue]
]);

export const StatusNames = new Map<DeviceStatus, string>([
   [DeviceStatus.Available, "Available"],
   [DeviceStatus.Problem, "Problem"],
   [DeviceStatus.Reserved, "Reserved"],
   [DeviceStatus.Disabled, "Disabled"],
   [DeviceStatus.Maintenance, "Maintenance"]
]);

const dropDownStyle: any = () => {

   return {
      dropdown: {},
      callout: {
         selectors: {
            ".ms-Callout-main": {
               padding: "4px 4px 12px 12px",
               overflow: "hidden"
            }
         }
      },
      dropdownItemHeader: { fontSize: 12, color: modeColors.text},
      dropdownOptionText: { fontSize: 12 },
      dropdownItem: {
         minHeight: 28, lineHeight: 28
      },
      dropdownItemSelected: {
         minHeight: 28, lineHeight: 28, backgroundColor: "inherit"
      }
   }
}


const customStyles = mergeStyleSets({
   raised: {
      boxShadow: "0 1.6px 3.6px 0 rgba(0,0,0,0.132), 0 0.3px 0.9px 0 rgba(0,0,0,0.108)",
      padding: "25px 30px 25px 30px"
   },
   details: {
      selectors: {
         '.ms-GroupHeader,.ms-GroupHeader:hover': {
            background: "#DFDEDD",
         },
         '.ms-GroupHeader-title': {
            cursor: "default"
         },
         '.ms-GroupHeader-expand,.ms-GroupHeader-expand:hover': {
            cursor: "pointer",
            background: "#DFDEDD"
         },
         '.ms-DetailsRow': {
            animation: "none",
            background: "unset"
         },
         '.ms-DetailsRow:hover': {
            cursor: "pointer",
            background: "#F3F2F1"
         }
      },
   }

});

const pivotKeyAutomation = "pivot-key-automation";
const pivotKeyShared = "pivot-key-shared";
const checkoutDays = 7;

const DevicePanel: React.FC = observer(() => {

   const [editState, setEditState] = useState<{ shown?: boolean, infoShown?: boolean, editNote?: boolean, device?: DeviceItem | undefined }>({});
   const [pivotState, setPivotState] = useState<{ key: string, poolFilter: Set<string> }>({ key: pivotKeyShared, poolFilter: new Set() });
   const [checkoutState, setCheckoutState] = useState<{ checkoutId?: string, checkinId?: string, showConfirm?: "in" | "out" | "error" }>({});

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   if (handler.updated) { }

   const InfoModal: React.FC = () => {

      const checkout = !!checkoutState.checkoutId;


      return <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 840, hasBeenOpened: false, top: "80px", position: "absolute" } }} className={hordeClasses.modal}>
         <Stack tokens={{ childrenGap: 24 }} styles={{ root: { padding: 8 } }}>
            <Stack grow verticalAlign="center">
               <Text variant="mediumPlus" styles={{ root: { fontWeight: "unset", fontFamily: "Horde Open Sans SemiBold" } }}>{`Checking ${checkout ? "out" : "in"} Device`}</Text>
            </Stack>
            <Stack horizontalAlign="center">
               <Text variant="mediumPlus">{`The device is being checked ${checkout ? "out" : "in"}, one moment please.`}</Text>
            </Stack>
            <Stack verticalAlign="center" style={{ paddingBottom: 32 }}>
               <Spinner size={SpinnerSize.large} />
            </Stack>
         </Stack>
      </Modal>
   }

   const automationTab = pivotState.key === pivotKeyAutomation;

   const platforms = handler.platforms;

   const poolWidth = automationTab ? 80 : 140;
   const detailsWidth = automationTab ? 180 : 120;

   const columns: IColumn[] = [
      { key: 'column_platform', name: 'Platform', fieldName: 'platformId', minWidth: 80, maxWidth: 80, isResizable: false },
      { key: 'column_pool', name: 'Pool', fieldName: 'poolId', minWidth: poolWidth, maxWidth: poolWidth, isResizable: false },
      { key: 'column_model', name: 'Model', fieldName: 'modelId', minWidth: 80, maxWidth: 80, isResizable: false },
      { key: 'column_name', name: 'Name', fieldName: 'name', minWidth: 160, maxWidth: 160, isResizable: false },
      { key: 'column_address', name: 'Address', fieldName: 'address', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column_status', name: 'Status', fieldName: 'status', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column_details', name: 'Details', fieldName: 'details', minWidth: detailsWidth, maxWidth: detailsWidth, isResizable: false },
      { key: 'column_notes', name: 'Notes', fieldName: 'notes', minWidth: 312, maxWidth: 312, isResizable: false },
   ];

   columns.forEach(c => {
      c.onRender = ((item: DeviceItem, index, column) => {
         return <Stack verticalFill={true} verticalAlign="center">
            <Text>{(item.device as any)[column?.fieldName!]}</Text>
         </Stack>

      });
   });

   let column = columns.find(c => c.name === "Status")!
   column.onRender = ((item: DeviceItem, index, column) => {

      const write = handler.getDeviceWriteAccess(item.device);

      if (!write) {
         return null;
      }

      const device = item.device;
      const status = handler.getDeviceStatus(device);

      let color = status === DeviceStatus.Disabled ? "#999999" : "#ffffff";
      let backgroundColor = StatusColors.get(status)!
      let text = StatusNames.get(status)!;


      if (device.checkedOutByUserId) {
         if (status === DeviceStatus.Available) {
            text = "Checked Out";
            backgroundColor = StatusColors.get(DeviceStatus.Reserved)!;
         }
      }

      // @todo: support deleting reservations
      return <Stack verticalFill={true} verticalAlign="center">
         <PrimaryButton styles={{ root: { border: "0px", width: 84, height: 24, fontSize: 12, fontFamily: "Horde Open Sans SemiBold !important", color: color, backgroundColor: backgroundColor } }} text={text}
            onClick={(ev) => {
               ev.stopPropagation(); ev.preventDefault();
               setEditState({ shown: true, device: item })
            }}
         />
      </Stack>

   })

   column = columns.find(c => c.name === "Platform")!;
   column.onRender = ((item: DeviceItem, index, column) => {

      const platform = handler.platforms.get(item.device.platformId);

      return <Stack verticalFill={true} verticalAlign="center">
         <Text>{platform?.name ?? "???"}</Text>
      </Stack>

   })

   column = columns.find(c => c.name === "Pool")!;
   column.onRender = ((item: DeviceItem, index, column) => {

      const pool = handler.pools.get(item.device.poolId);

      return <Stack verticalFill={true} verticalAlign="center">
         <Text>{pool?.name ?? "???"}</Text>
      </Stack>

   })

   column = columns.find(c => c.name === "Model")!;
   column.onRender = ((item: DeviceItem, index, column) => {


      return <Stack verticalFill={true} verticalAlign="center">
         <Text>{item.device.modelId ?? "Base"}</Text>
      </Stack>

   })

   column = columns.find(c => c.name === "Details")!
   column.onRender = ((item: DeviceItem, index, column) => {

      const device = item.device;

      // Check whether shared kit
      if (!automationTab) {

         const status = handler.getDeviceStatus(item.device);

         const write = handler.getDeviceWriteAccess(item.device);

         const checkOutDisabled = !write || status !== DeviceStatus.Available;

         if (device.id === checkoutState.checkoutId) {
            return <Stack verticalFill={true} verticalAlign="center" horizontalAlign="start"><Spinner size={SpinnerSize.small} /></Stack>;
         }

         if (!device.checkedOutByUserId) {

            return <Stack verticalFill={true} verticalAlign="center">
               {!checkOutDisabled && <PrimaryButton styles={{ root: { border: "0px", width: 90, height: 24, fontSize: 12, fontFamily: "Horde Open Sans SemiBold !important" } }} text="Check Out"
                  onClick={async (ev) => {
                     ev.stopPropagation(); ev.preventDefault();
                     setCheckoutState({ checkoutId: device.id });
                     await backend.checkoutDevice(device.id, true).then(() => {
                        handler.updating = false;
                        handler.update().then(() => {
                           setCheckoutState({ showConfirm: "out" })
                        }).catch((reason) => {
                           console.error(reason);
                           setCheckoutState({ showConfirm: "error" });
                        })
                     });
                  }}
               />}
            </Stack>
         }

         const user = handler.users.get(device.checkedOutByUserId);

         if (!user) {
            return <Text>Error: Missing user {device.checkedOutByUserId} for checkout</Text>
         }

         if (user.id === dashboard.userId) {
            return <Stack verticalFill={true} verticalAlign="center">
               <PrimaryButton styles={{ root: { border: "0px", width: 90, height: 24, fontSize: 12, fontFamily: "Horde Open Sans SemiBold !important", backgroundColor: StatusColors.get(DeviceStatus.Reserved)! } }} text="Check In"
                  onClick={async (ev) => {
                     ev.stopPropagation(); ev.preventDefault();
                     setCheckoutState({ checkinId: device.id });
                     await backend.checkoutDevice(device.id, false).then(() => {
                        handler.updating = false;
                        handler.update().then(() => setCheckoutState(
                           { showConfirm: handler.getUserDeviceCheckouts(dashboard.userId).length > 0 ? "in" : undefined }))
                     }).catch((reason) => {
                        console.error(reason);
                        setCheckoutState({ showConfirm: "error" });
                     });
                  }}
               />
            </Stack>
         }

         return <Stack verticalFill={true} verticalAlign="center"><Text variant="tiny">{`${user.name}`}</Text></Stack>

      }

      // Automation Kit

      const r = handler.getReservation(device);

      if (!r) {
         return null;
      }

      let url = "";

      if (r.jobId) {
         url = `/job/${r.jobId}`;
         if (r.stepId) {
            url += `?step=${r.stepId}`;
         }
      }
      return <Stack verticalFill={true} verticalAlign="center">
         <Stack>
            {!!url && <Link to={url}><Text variant="small">Job Details</Text></Link>}
            {!url && !!r.reservationDetails && r.reservationDetails.startsWith("https://horde") && <FluentLink href={r.reservationDetails} target="_blank" underline={false}><Text variant="small" onClick={(ev) => { ev.stopPropagation(); }}>Job Details</Text></FluentLink>}
            <Text variant="tiny">Host: {r.hostname}</Text>
            <Text variant="tiny">Duration: {Math.floor(moment.duration(moment.utc().diff(moment(r.createTimeUtc))).asMinutes())}m</Text>
         </Stack>
      </Stack>

   })

   column = columns.find(c => c.name === "Notes")!
   column.onRender = ((item: DeviceItem, index, column) => {

      const device = item.device;
      let notes = device.notes ?? "";

      if (device.checkOutTime && device.checkedOutByUserId) {
         notes = `Checked out until ${getNiceTime(moment(new Date(device.checkOutTime)).add(checkoutDays, 'd').toDate())}.  ` + notes;
      }

      return <Stack verticalFill={true} verticalAlign="center">
         <div style={{ cursor: "pointer" }} onClick={(ev) => {
            ev.stopPropagation(); ev.preventDefault();
            setEditState({
               shown: true, device: item, editNote: true
            })
         }}>
            <Stack horizontal tokens={{ childrenGap: 12 }}>
               <Icon style={{ paddingTop: 2 }} iconName="Edit" />
               <Text variant="small" style={{ whiteSpace: "pre-wrap" }}>{notes}</Text>
            </Stack>
         </div>
      </Stack>
   })

   let poolDevices = handler.getDevices().filter((d) => {
      const pool = handler.pools.get(d.poolId);
      if (!pool) {
         return false;
      }

      if (automationTab && pool.poolType !== DevicePoolType.Automation) {
         return false;
      }

      if (!automationTab && pool.poolType === DevicePoolType.Automation) {
         return false;
      }

      if (pivotState.poolFilter.size && !pivotState.poolFilter.has(d.poolId)) {
         return false;
      }

      return true;

   });

   const devices = poolDevices.sort((a, b) => {

      if (a.platformId === b.platformId) {

         if (a.poolId === b.poolId) {

            /*
            if (a.status === b.status) {

                if (a.status === Status.Reserved) {

                    const ra = backend.reservations.get(a.name);
                    const rb = backend.reservations.get(b.name);

                    if (ra && rb) {
                        return parseInt(rb.duration) - parseInt(ra.duration);
                    }

                }

                return a.name < b.name ? -1 : 1;

            }
            */

            // see legacy backend for the status sort stuff, I added this here
            return a.name < b.name ? -1 : 1;;
            //return StatusSortPriority.get(a.status)! - StatusSortPriority.get(b.status)!;

         }

         return a.poolId < b.poolId ? -1 : 1;
      }

      return a.platformId < b.platformId ? -1 : 1;

   }).map(d => {
      return { device: d } as DeviceItem;
   });

   const newGroups: IGroup[] = [];

   let curPlatform: string | undefined;
   devices.forEach((item, index) => {
      const d = item.device;
      if (d.platformId !== curPlatform) {
         curPlatform = d.platformId;
         const key = `group_${d.platformId}`;
         newGroups.push({
            key: key,
            name: platforms.get(d.platformId)!.name,
            startIndex: index,
            count: devices.filter(cd => cd.device.platformId === d.platformId).length,
            level: 0,
            isCollapsed: groups.find(g => g.key === key)?.isCollapsed
         });
      }
   })

   groups = newGroups;

   const onRenderRow: IDetailsListProps['onRenderRow'] = (props) => {

      if (props) {

         return <div style={{ cursor: "pointer" }} onClick={(ev) => {
            ev.stopPropagation(); ev.preventDefault();
            setEditState({ infoShown: true, device: props.item })
         }}>
            <DetailsRow {...props} />
         </div>
      }

      return null;
   };

   const pivotItems: JSX.Element[] = [];

   pivotItems.push(<PivotItem headerText="Shared" itemKey={pivotKeyShared} key={pivotKeyShared} />);
   pivotItems.push(<PivotItem headerText="Automation" itemKey={pivotKeyAutomation} key={pivotKeyAutomation} />);


   const poolItems = Array.from(handler.pools.values()).filter(pool => {

      if (automationTab && pool.poolType !== DevicePoolType.Automation) {
         return false;
      }
      if (!automationTab && pool.poolType === DevicePoolType.Automation) {
         return false;
      }

      return true;

   }).sort((a, b) => {
      return a.name < b.name ? 1 : 0;
   }).map(pool => {
      const name = pool.name;
      return {
         text: name,
         key: `dropdown_${pool.id}_key`,
         poolId: pool.id
      }
   });

   const checkedOut = handler.getUserDeviceCheckouts(dashboard.userId);

   return (<Stack>
      {(!!checkoutState.checkinId || !!checkoutState.checkoutId) && <InfoModal />}
      {!!checkoutState.showConfirm && checkedOut.length > 0 && <CheckoutConfirmModal check={checkoutState.showConfirm} devices={checkedOut} onClose={() => setCheckoutState({})} />}
      {editState.infoShown && <DeviceInfoModal handler={handler} deviceIn={editState.device?.device} onEdit={(device) => { setEditState({ infoShown: true, shown: true, device: { device: device } }) }} onClose={() => { setEditState({}) }} />}
      {editState.shown && <DeviceEditor handler={handler} deviceIn={editState.device?.device} editNote={editState.editNote} onClose={() => { setEditState({ ...editState, shown: false }) }} />}
      <Stack styles={{ root: { paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack>

            <Stack horizontal>
               <Stack>
                  <Pivot className={hordeClasses.pivot}
                     selectedKey={pivotState.key}
                     linkSize="normal"
                     linkFormat="links"
                     onLinkClick={(item) => {
                        setPivotState({ key: item!.props.itemKey!, poolFilter: new Set() })
                     }}>
                     {pivotItems}
                  </Pivot>
               </Stack>
               {poolItems.length > 1 && <Stack style={{ paddingTop: 8, paddingLeft: 48 }}>
                  <Dropdown
                     placeholder="Filter Pools"
                     style={{ width: 200 }}
                     styles={dropDownStyle}
                     selectedKeys={Array.from(pivotState.poolFilter).map(id => `dropdown_${id}_key`)}
                     multiSelect
                     options={poolItems}
                     onChange={(event, option, index) => {

                        if (option) {

                           if (option.selected) {
                              pivotState.poolFilter.add((option as any).poolId);
                           } else {
                              pivotState.poolFilter.delete((option as any).poolId);
                           }

                           setPivotState({ ...pivotState })

                        }
                     }}
                  />
               </Stack>}

               <Stack grow />
               <Stack>
                  <PrimaryButton text="Add Device"
                     styles={{ root: { fontFamily: "Horde Open Sans SemiBold !important" } }}
                     onClick={(ev) => { ev.stopPropagation(); ev.preventDefault(); setEditState({ shown: true }) }}
                  />
               </Stack>

            </Stack>


         </Stack>


         <Stack tokens={{ childrenGap: 12 }}>
            <FocusZone direction={FocusZoneDirection.vertical}>
               <div className={customStyles.details} style={{ height: "calc(100vh - 270px)", position: 'relative' }} data-is-scrollable>
                  <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always} onScroll={() => { }}>
                     <DetailsList
                        styles={{ root: { overflowX: "hidden" } }}
                        items={devices}
                        groups={groups}
                        columns={columns}
                        selectionMode={SelectionMode.none}
                        layoutMode={DetailsListLayoutMode.justified}
                        compact={true}
                        onRenderRow={onRenderRow}
                     />
                  </ScrollablePane>
               </div>
            </FocusZone>
         </Stack>
      </Stack>

   </Stack>
   );
});

export const CheckoutConfirmModal: React.FC<{ check: "in" | "out" | "error", devices: GetDeviceResponse[], onClose: () => void }> = ({ check, devices, onClose }) => {

   type CheckoutItem = {
      device: GetDeviceResponse;
   }

   const close = () => {
      onClose();
   };

   const columns = [
      { key: 'column1', name: 'Name', fieldName: 'name', minWidth: 240, maxWidth: 240 },
      { key: 'column2', name: 'Date', fieldName: 'value', minWidth: 500, maxWidth: 500 },
   ];

   const deviceItems: CheckoutItem[] = devices.map(d => { return { device: d } });

   const onRenderItemColumn = (item: CheckoutItem, index?: number, columnIn?: IColumn) => {

      const column = columnIn!;

      const device = item.device;

      const platform = handler.platforms.get(device.platformId)?.name;

      // simple cases
      switch (column.name) {
         case 'Name':
            return <Text variant="medium" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{platform} : {device.name}</Text>
         case 'Date':
            if (device.checkOutTime) {
               return <Text variant="medium">{`Checked out until ${getNiceTime(moment(new Date(device.checkOutTime!)).add(checkoutDays, 'd').toDate())}`}</Text>
            }
            return null;
      }

      return null;
   }

   let text = check === "in" ? "Device checked in, you have the following devices checked out:" : "Device checked out, you have the following devices checked out:";
   if (check === "error") {
      text = "There was an error processing request, please try again.";
   }


   return <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 840, hasBeenOpened: false, top: "80px", position: "absolute" } }} className={hordeClasses.modal} onDismiss={() => { close() }}>
      <Stack tokens={{ childrenGap: 12 }}>
         <Stack horizontal styles={{ root: { paddingLeft: 12 } }}>
            <Stack grow style={{ paddingTop: 12, paddingLeft: 8 }}>
               <Label style={{ fontSize: 18, fontWeight: 400, fontFamily: "Horde Open Sans SemiBold" }}>Checked Out Devices</Label>
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
            <Stack style={{ paddingLeft: 18 }}>
               <Stack>
                  <Text variant="medium">{text}</Text>
               </Stack>
            </Stack>
            <Stack horizontal>
               <Stack style={{ minWidth: 700, paddingLeft: 18, paddingRight: 18 }}>
                  {check !== "error" && <DetailsList
                     items={deviceItems}
                     columns={columns}
                     setKey="set"
                     layoutMode={DetailsListLayoutMode.justified}
                     isHeaderVisible={false}
                     selectionMode={SelectionMode.none}
                     onRenderItemColumn={onRenderItemColumn}
                  />}
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


export const DeviceView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Devices' }]} />
      <Stack horizontal>
         <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 900, flexShrink: 0, backgroundColor: 'rgb(250, 249, 249)' }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: 'rgb(250, 249, 249)', width: "100%" } }}>
            <Stack style={{ maxWidth: 1800, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%", height: 'calc(100vh - 228px)' }} tokens={{ childrenGap: 18 }}>
                     <DevicePanel />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};


