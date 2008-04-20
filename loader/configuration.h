/* Copyright (c) 2007 Mega Man */
#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

/** FIle where configuration is stored. */
#define CONFIG_FILE CONFIG_DIR "/config.txt"
/** Directory where configuration is stored. */
#define CONFIG_DIR "mc0:kloader"
/** Config file used for DVDs. */
#define DVD_CONFIG_FILE "cdfs:config.txt"

#ifdef __cplusplus
extern "C" {
#endif
int loadConfiguration(const char *configfile);
void saveConfiguration(const char *configfile);
#ifdef __cplusplus
}
#endif
void addConfigTextItem(const char *name, char *value, int maxlen);
void addConfigCheckItem(const char *name, int *value);

#endif
