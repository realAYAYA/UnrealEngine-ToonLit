

export const copyToClipboard = (value: string | undefined) => {

    if (!value) {
        return;
    }

    const el = document.createElement('textarea');
    el.value = value;
    document.body.appendChild(el);
    el.select();
    document.execCommand('copy');
    document.body.removeChild(el);
}
