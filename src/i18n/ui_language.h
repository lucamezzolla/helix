#ifndef HELIX_UI_LANGUAGE_H
#define HELIX_UI_LANGUAGE_H

typedef enum {
    HELIX_LANG_IT = 0,
    HELIX_LANG_EN = 1,
    HELIX_LANG_ES = 2,
    HELIX_LANG_PT = 3,
    HELIX_LANG_FR = 4
} HelixLanguage;

const char *helix_language_code(HelixLanguage language);
const char *helix_language_display_name(HelixLanguage language);
HelixLanguage helix_language_from_code(const char *code);
const char *helix_tr(HelixLanguage language, const char *key);

#endif
