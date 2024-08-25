
import dashboard from "../backend/Dashboard";
import { darkTheme } from "./darkTheme";
import { lightTheme } from "./lightTheme";

/** Please note, backend must be initialized before calling this method, so don't use in module scope */
export function getHordeTheme() {  
    
    if (!dashboard.available) {
        throw new Error("getHordeStyling must not be called before dashboard is available");
     }
      
    return dashboard.darktheme ? darkTheme : lightTheme;
}
