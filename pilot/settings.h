#if !defined(settings_h)
#define settings_h

/* Simple library to deal with settings files.
 * MIT license, Copyright 2017 Jon Watte.
 * This library will use perror() to print errors 
 * to stderr when failing.
 */

/* Load/merge settings from a file based on app name. Return number of settings, or 0 for failure. */
int load_settings(char const *appname);
/* save all settings to a file based on app name. Returns number of settings, or 0 for failure. */
int save_settings(char const *appname);

/* Get a particular setting; when not found, return the default */
char const *get_setting(char const *name, char const *dflt);
/* Get a particular setting cast to long; return the default if not present or not a number */
long get_setting_int(char const *name, int dflt);
/* Get a particular setting cast to double; return the default if not present or not a number */
double get_setting_float(char const *name, double dflt);
/* Set a setting to a particular value. Return 0 if the name or value have invalid characters.
 * Names must not start with '#' or contain spaces or '='. Names and values may not contain newlines/returns.
 */
int set_setting(char const *name, char const *value);
int set_setting_long(char const *name, long value);
int set_setting_float(char const *name, double value);
/* Remove a setting by name. Return 0 on failure and 1 on succes.  */
int remove_setting(char const *name);
/* Test whether a given setting exists. Return 1 on exists, 0 on not-exists. */
int has_setting(char const *name);
/* Iterate through potentially all settings. Your function will be called back once
 * for each setting that is loaded. If you return 0, the iteration will be broken; 
 * if you return 1, you will be called back with the next setting. The cookie is 
 * passed through to each invocation of your callback.
 */
void iterate_settings(int (*func)(char const *name, char const *value, void *cookie), void *cookie);


#endif  //  settings_h

