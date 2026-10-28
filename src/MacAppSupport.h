#ifndef MAC_APP_SUPPORT_H
#define MAC_APP_SUPPORT_H

// When running from a macOS .app bundle, makes ~/Library/Application Support/CSB/<bundle name>
// the working directory and fills it with the bundled game files it does not have yet.
// Does nothing for a plain executable, which keeps using the directory it was started from.
void UseApplicationSupportFolderWhenBundled(void);

#endif
