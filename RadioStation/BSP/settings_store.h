#ifndef FX9A_SETTINGS_STORE_H
#define FX9A_SETTINGS_STORE_H
#include <stdbool.h>
/* Existing record offsets remain compatible. Metadata is stored at 1152. */
typedef enum { SETTINGS_LEGACY, SETTINGS_VALID, SETTINGS_EMPTY,
               SETTINGS_CORRUPT, SETTINGS_IO_ERROR } SettingsResult;
SettingsResult Settings_Load(void);
bool Settings_Save(void);
#endif
