// Usage: {{ num | numStr }}
export function numStr(v: number): string {
    if(v == 0 || v > 8) {
        return "";
    }

    const strLookup: Array<string> = [
        "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight"
    ];

    return strLookup[v - 1] + (v > 1 ? "Params" : "Param");
}
