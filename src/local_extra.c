#include "local_extra.h"
#include "local.h"
#include <string.h>

static int lang_is_prefix(const char* prefix) {
    const char* lang = local_get_lang();
    size_t len = strlen(prefix);
    return (strncmp(lang, prefix, len) == 0);
}

const char* local_text_select_level(void) {
    if (lang_is_prefix("ru")) {
        return "Выбор уровня";
    }
    if (lang_is_prefix("de")) {
        return "Levelauswahl";
    }
    return "Select level";
}

const char* local_text_settings(void) {
    if (lang_is_prefix("ru")) {
        return "Настройки";
    }
    return "Settings";
}

const char* local_text_press_start(void) {
    if (lang_is_prefix("ru")) {
        return "Нажмите START";
    }
    return "Press START";
}
