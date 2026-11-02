# Popups

One file per known popup, `popups/<id>.json`, handled by the global popup handler (`core/include/maba/popup.h`) before every look at the screen. Tasks never handle popups themselves. Format: `docs/PAGE_FORMAT.md`, "Popups".

No popup is defined yet: their templates must be captured from the real client (login notices, attendance rewards, event banners, the quit dialog). Add each one here as it is found, with a `notes` entry saying where it appeared.
