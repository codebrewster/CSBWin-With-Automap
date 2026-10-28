
#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
/*                                                                            */
/*                          CONFIG FILE ROUTINES                              */
/*                                                                            */
/******************************************************************************/

#include "CSBTypes.h"

void csb_set_config_file(char *filename);

void csb_push_config_state(void);
void csb_pop_config_state(void);

char *csb_get_config_string(const char *section, const char *name, char *def);
int   csb_get_config_int(const char *section, const char *name, int def);
ui32 csb_get_config_hex(char *section, char *name, ui32 def);
int   csb_get_config_id(char *section, char *name, int def);
char *csb_get_config_text(char *msg);

void csb_set_config_string(const char *section, const char *name, char *val);
void csb_set_config_int(const char *section, const char *name, int val);
void csb_set_config_hex(char *section, char *name, int val);
void csb_set_config_8bit_hex(char *section, char *name, ui32 val);
void csb_set_config_16bit_hex(char *section, char *name, ui32 val);
void csb_set_config_24bit_hex(char *section, char *name, ui32 val);
void csb_set_config_32bit_hex(char *section, char *name, ui32 val);
void csb_set_config_id(char *section, char *name, int val);

void csb_clear_config_section(const char *section);
void csb_config_cleanup();

#define csb_translate_text(src) csb_get_config_text(src)

#ifdef __cplusplus
}
#endif
