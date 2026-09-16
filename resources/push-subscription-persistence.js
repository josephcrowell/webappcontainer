// Qt WebEngine 6.11 removes PushManager registrations during clean profile
// teardown. Remember options from successful site-initiated subscriptions and
// recreate a missing subscription when that site next queries it.
(() => {
    if (typeof PushManager === "undefined" ||
            PushManager.prototype.__webAppContainerPersistenceInstalled)
        return;

    const storageKey = "__webappcontainer_push_subscription_options_v1";
    const originalSubscribe = PushManager.prototype.subscribe;
    const originalGetSubscription = PushManager.prototype.getSubscription;

    function encodeApplicationServerKey(key) {
        if (key === undefined || key === null)
            return null;
        if (typeof key === "string")
            return {type: "string", value: key};
        let bytes;
        if (ArrayBuffer.isView(key))
            bytes = new Uint8Array(key.buffer, key.byteOffset, key.byteLength);
        else if (key instanceof ArrayBuffer)
            bytes = new Uint8Array(key);
        else
            return null;
        let binary = "";
        for (const byte of bytes)
            binary += String.fromCharCode(byte);
        return {type: "bytes", value: btoa(binary)};
    }

    function decodeApplicationServerKey(encoded) {
        if (!encoded)
            return undefined;
        if (encoded.type === "string")
            return encoded.value;
        if (encoded.type !== "bytes")
            return undefined;
        const binary = atob(encoded.value);
        const bytes = new Uint8Array(binary.length);
        for (let index = 0; index < binary.length; ++index)
            bytes[index] = binary.charCodeAt(index);
        return bytes;
    }

    PushManager.prototype.subscribe = async function(options = {}) {
        const subscription = await originalSubscribe.call(this, options);
        try {
            localStorage.setItem(storageKey, JSON.stringify({
                userVisibleOnly: options.userVisibleOnly === true,
                applicationServerKey: encodeApplicationServerKey(
                    options.applicationServerKey)
            }));
        } catch (_) {
            // Storage can be unavailable for opaque or restricted origins.
        }
        return subscription;
    };

    PushManager.prototype.getSubscription = async function() {
        const existing = await originalGetSubscription.call(this);
        if (existing)
            return existing;
        try {
            const saved = JSON.parse(localStorage.getItem(storageKey));
            if (!saved)
                return null;
            return await originalSubscribe.call(this, {
                userVisibleOnly: saved.userVisibleOnly,
                applicationServerKey: decodeApplicationServerKey(
                    saved.applicationServerKey)
            });
        } catch (_) {
            return null;
        }
    };

    Object.defineProperty(PushManager.prototype,
                          "__webAppContainerPersistenceInstalled",
                          {value: true});
})();
