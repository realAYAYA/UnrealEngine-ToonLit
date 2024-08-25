import { IconButton, Modal, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React from "react";
import { getHordeStyling } from "../../styles/Styles";

export const BisectionModal: React.FC<{ taskId: string, onCloseExternal?: () => void }> = observer(({ onCloseExternal }) => {

   const { hordeClasses, modeColors } = getHordeStyling();

   return <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 1420, backgroundColor: modeColors.background, hasBeenOpened: false, top: "24px", position: "absolute", height: "95vh" } }} className={hordeClasses.modal} onDismiss={() => { if (onCloseExternal) { onCloseExternal() } else { /*onClose()*/ } }}>
      <Stack style={{ height: "93vh" }}>
         <Stack style={{ height: "100%" }}>
            <Stack style={{ flexBasis: "70px", flexShrink: 0 }}>
               <Stack horizontal styles={{ root: { padding: 8 } }} style={{ padding: 20, paddingBottom: 8 }}>
                  <Stack horizontal style={{ width: 1024 }} tokens={{ childrenGap: 24 }} verticalAlign="center" verticalFill={true}>
                     <Stack >
                        <Text styles={{ root: { fontWeight: "unset", maxWidth: 720, wordBreak: "break-word", fontFamily: "Horde Open Sans SemiBold", fontSize: "14px", color: "#087BC4", } }}>{"This is a bisection modal"}</Text>
                     </Stack>
                     <Stack grow />
                  </Stack>
                  <Stack>
                     <Text>Bisection Command Bar</Text>
                  </Stack>
                  <Stack grow />
                  <Stack horizontalAlign="end">
                     <IconButton
                        iconProps={{ iconName: 'Cancel' }}
                        ariaLabel="Close popup modal"
                        onClick={() => { if (onCloseExternal) { onCloseExternal() } else { /*onClose()*/ } }}
                     />
                  </Stack>

               </Stack>
            </Stack>
            <Stack style={{ flexGrow: 1, flexShrink: 0 }}>
               <Text> Add More Here</Text>
            </Stack>
         </Stack>
      </Stack>
   </Modal>

});