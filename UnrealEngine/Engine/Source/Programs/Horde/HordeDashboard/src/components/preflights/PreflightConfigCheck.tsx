import { FontIcon, IconButton, MaskedTextField, Modal, PrimaryButton, Spinner, SpinnerSize, Stack, Text } from "@fluentui/react";
import { useState } from "react";
import { useNavigate } from "react-router-dom";
import backend from "../../backend";
import dashboard, { StatusColor } from "../../backend/Dashboard";
import { getHordeStyling } from "../../styles/Styles";

const PreflightConfigPanel: React.FC = () => {
   
   const search = new URL(window.location.toString()).searchParams;
   const shelvedChange = search.get("preflightconfig") ? search.get("preflightconfig")! : undefined;

   const navigate = useNavigate();
   const [state, setState] = useState<{ initialCL?: string, submitting?: boolean, success?: boolean, message?: string }>({ initialCL: shelvedChange });

   const maskFormat: { [key: string]: RegExp } = {
      '*': /[0-9]/,
   };

   const checkPreflight = async (preflightCL?: string) => {

      if (!preflightCL) {

         if (!shelvedChange) {
            return;
         }

         preflightCL = shelvedChange;
      }

      setState({ submitting: true });

      try {
         const response = await backend.checkPreflightConfig(parseInt(preflightCL));
         setState({ submitting: false, success: response.result, message: response.message });
      } catch (error) {
         setState({ submitting: false, success: false, message: error as string });
      }

   }

   if (state.initialCL) {
      checkPreflight(state.initialCL);
      return null;
   }


   return (<Stack>
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 0, paddingRight: 0, width: "100%" } }} >
         <Stack tokens={{ childrenGap: 12 }} >
            <Stack style={{ width: 800, paddingLeft: 12 }}>
               <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 24 }}>
                  <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 18 }}>
                     <Text styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Shelved Change</Text>
                     <MaskedTextField placeholder="Shelved Change" mask="***********" maskFormat={maskFormat} maskChar="" value={shelvedChange} onChange={(ev, newValue) => {
                        ev.preventDefault();

                        if (!newValue) {
                           navigate("/index?preflightconfig", { replace: true });
                        }

                        if (!isNaN(parseInt(newValue!))) {
                           navigate(`/index?preflightconfig=${newValue}`, { replace: true });
                        }

                     }} />
                  </Stack>
                  <Stack grow />
                  {!!state.submitting && <Stack>
                     <Spinner size={SpinnerSize.large} />
                  </Stack>}

                  {state.success === true && <Stack>
                     <FontIcon style={{ color: dashboard.getStatusColors().get(StatusColor.Success)!, fontSize: 24 }} iconName="Tick" />
                  </Stack>}

                  {state.success === false && <Stack>
                     <FontIcon style={{ color: dashboard.getStatusColors().get(StatusColor.Failure)!, fontSize: 24 }} iconName="Cross" />
                  </Stack>}

                  <Stack>
                     <PrimaryButton disabled={!shelvedChange || state.submitting} styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }} onClick={async () => {
                        checkPreflight(shelvedChange);
                     }}>Check</PrimaryButton>
                  </Stack>
               </Stack>
               {!!state.message && <Stack style={{ paddingTop: 18, paddingRight: 2 }}>
                  <Stack style={{ paddingBottom: 12 }}>
                     <Text styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Error Message</Text>
                  </Stack>
                  <Stack style={{ border: "1px solid #CDCBC9" }}>
                     <Text style={{ whiteSpace: "pre-wrap", height: "472px", overflowY: "auto", padding: 18 }}> {state.message}</Text>
                  </Stack>
               </Stack>}
            </Stack>
         </Stack>
      </Stack>
   </Stack>)
}


export const PreflightConfigModal: React.FC<{ onClose: () => void }> = ({ onClose }) => {

   const { hordeClasses } = getHordeStyling();

   return <Stack>
      <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 880, height: 720, hasBeenOpened: false, top: "80px", position: "absolute" } }} onDismiss={() => onClose()} className={hordeClasses.modal}>
         <Stack className="horde-no-darktheme" styles={{ root: { paddingTop: 10, paddingRight: 0 } }}>
            <Stack style={{ paddingLeft: 24, paddingRight: 12 }}>
               <Stack tokens={{ childrenGap: 12 }} style={{ height: 700 }}>
                  <Stack horizontal verticalAlign="start">
                     <Stack style={{ paddingTop: 3 }}>
                        <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Preflight Configuration</Text>
                     </Stack>
                     <Stack grow />
                     <Stack horizontalAlign="end" >
                        <IconButton
                           iconProps={{ iconName: 'Cancel' }}
                           onClick={() => { onClose() }}
                        />
                     </Stack>
                  </Stack>
                  <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
                     <PreflightConfigPanel />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Modal>
   </Stack>;
};
