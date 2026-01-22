#include "local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pspkernel.h>
#include <psputility.h>

static char s_phone_lang[8] = "xx"; // Язык PSP (по умолчанию английский)

// Кэш всех строк для предотвращения повторных загрузок файла
#define MAX_STRING_ID 32     // С запасом (видел до 20 в local.h)
#define MAX_STRING_LENGTH 1024   // С запасом для длинных инструкций
static char s_string_cache[MAX_STRING_ID][MAX_STRING_LENGTH];
static bool s_cache_initialized = false;


// Определение языка PSP в соответствии с файлами
static void detect_phone_language(void) {
    int language;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &language) != 0) {
        strcpy(s_phone_lang, "xx"); // Английский по умолчанию
        return;
    }

    static const struct {
        int lang_id;
        const char* code;
    } k_lang_map[] = {
        { PSP_SYSTEMPARAM_LANGUAGE_ENGLISH, "xx"    },
        { PSP_SYSTEMPARAM_LANGUAGE_GERMAN,  "de"    },
        { PSP_SYSTEMPARAM_LANGUAGE_RUSSIAN, "ru-RU" }
    };

    for (size_t i = 0; i < sizeof(k_lang_map) / sizeof(k_lang_map[0]); ++i) {
        if (k_lang_map[i].lang_id == language) {
            strcpy(s_phone_lang, k_lang_map[i].code);
            return;
        }
    }

    strcpy(s_phone_lang, "xx");
}

const char* local_get_lang(void) {
    if (strcmp(s_phone_lang, "xx") == 0) {
        detect_phone_language();
    }
    return s_phone_lang;
}

// Загрузка всех строк в кэш при первом обращении
static void initialize_string_cache(void) {
    if (s_cache_initialized) return;

    // Определяем язык если еще не определили
    if (strcmp(s_phone_lang, "xx") == 0) {
        detect_phone_language();
    }

    // Формируем имя файла
    char filename[64];
    snprintf(filename, sizeof(filename), "lang/lang.%s", s_phone_lang);

    // Пробуем открыть файл для нужного языка
    FILE* file = fopen(filename, "rb");
    if (!file) {
        // Если не найден, пробуем английский
        file = fopen("lang/lang.xx", "rb");
        if (!file) {
            // Заполняем ошибками и выходим
            for (int i = 0; i < MAX_STRING_ID; i++) {
                strcpy(s_string_cache[i], "NoLang");
            }
            s_cache_initialized = true;
            return;
        }
    }

    // Получаем размер файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Читаем весь файл
    unsigned char* buffer = malloc(file_size);
    if (!buffer) {
        fclose(file);
        for (int i = 0; i < MAX_STRING_ID; i++) {
            strcpy(s_string_cache[i], "Err");
        }
        s_cache_initialized = true;
        return;
    }

    fread(buffer, 1, file_size, file);
    fclose(file);

    // Сначала определяем сколько строк в файле и максимальную длину
    int actual_string_count = 0;
    int max_found_length = 0;

    // Проходим по файлу чтобы определить реальные размеры
    for (int string_id = 0; string_id < 100; string_id++) { // максимум 100 проверок
        size_t offset = string_id * 2;
        if (offset + 2 > (size_t)file_size) break;

        unsigned short string_offset = (buffer[offset] << 8) | buffer[offset + 1];
        if (string_offset + 2 > file_size) break;

        unsigned short length = (buffer[string_offset] << 8) | buffer[string_offset + 1];
        if (string_offset + 2 + length > file_size) break;

        actual_string_count = string_id + 1;
        if (length > max_found_length) {
            max_found_length = length;
        }
    }

    // Загружаем все строки в кэш
    for (int string_id = 0; string_id < MAX_STRING_ID && string_id < actual_string_count; string_id++) {
        // Реализуем алгоритм Java версии для каждой строки
        size_t offset = string_id * 2;

        if (offset + 2 > (size_t)file_size) {
            strcpy(s_string_cache[string_id], "Err");
            continue;
        }

        // Читаем short (offset к строке)
        unsigned short string_offset = (buffer[offset] << 8) | buffer[offset + 1];
        offset = string_offset;

        // Читаем длину строки (2 байта, big-endian)
        if (offset + 2 > (size_t)file_size) {
            strcpy(s_string_cache[string_id], "Err");
            continue;
        }

        unsigned short length = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;

        if (offset + length > (size_t)file_size) {
            strcpy(s_string_cache[string_id], "Err");
            continue;
        }

        // Копируем строку в кэш
        if (length > 0 && length < MAX_STRING_LENGTH - 1) {
            memcpy(s_string_cache[string_id], buffer + offset, length);
            s_string_cache[string_id][length] = '\0';
        } else if (length == 0) {
            s_string_cache[string_id][0] = '\0';  // Пустая строка
        } else {
            strcpy(s_string_cache[string_id], "TooLong");
        }
    }

    free(buffer);
    s_cache_initialized = true;
}

// Основная функция получения текста (аналог Java getText)
const char* local_get_text(int string_id) {
    // Инициализируем кэш при первом обращении
    initialize_string_cache();

    // Проверяем границы
    if (string_id < 0 || string_id >= MAX_STRING_ID) {
        return "BadID";
    }

    // Возвращаем указатель на кэшированную строку
    return s_string_cache[string_id];
}

// Вспомогательная функция замены подстроки (как replace() в Java Local.java:34-38)
static char* str_replace(char* dest, size_t dest_size, const char* src,
                        const char* search, const char* replace) {
    const char* pos = strstr(src, search);

    if (pos == NULL) {
        // Подстрока не найдена - копируем как есть
        size_t src_len = strlen(src);
        if (src_len >= dest_size) {
            src_len = dest_size - 1;
        }
        memcpy(dest, src, src_len);
        dest[src_len] = '\0';
        return dest;
    }

    // Вычисляем длины
    size_t prefix_len = pos - src;
    size_t search_len = strlen(search);
    size_t replace_len = strlen(replace);
    size_t suffix_len = strlen(pos + search_len);

    // Проверяем, поместится ли результат
    if (prefix_len + replace_len + suffix_len >= dest_size) {
        size_t src_len = strlen(src);
        if (src_len >= dest_size) {
            src_len = dest_size - 1;
        }
        memcpy(dest, src, src_len);
        dest[src_len] = '\0';
        return dest;
    }

    // Собираем результат: prefix + replace + suffix
    memcpy(dest, src, prefix_len);
    memcpy(dest + prefix_len, replace, replace_len);
    strcpy(dest + prefix_len + replace_len, pos + search_len);

    return dest;
}

// Функция получения текста с параметрами (аналог Java getText(int, String[]))
const char* local_get_text_with_params(int string_id, const char** params, int param_count) {
    // Получаем базовую строку
    const char* base_str = local_get_text(string_id);

    if (param_count <= 0 || params == NULL) {
        return base_str;
    }

    // Статический буфер для результата (используем тот же принцип, что и в кэше)
    static char result_buffer[MAX_STRING_LENGTH];
    static char temp_buffer[MAX_STRING_LENGTH];

    // Копируем базовую строку в рабочий буфер
    strncpy(result_buffer, base_str, MAX_STRING_LENGTH - 1);
    result_buffer[MAX_STRING_LENGTH - 1] = '\0';

    // Логика замены как в Java Local.java:74-81
    if (param_count == 1) {
        // Один параметр: заменяем %U на params[0]
        str_replace(temp_buffer, MAX_STRING_LENGTH, result_buffer, "%U", params[0]);
        strcpy(result_buffer, temp_buffer);
    } else {
        // Несколько параметров: заменяем %0U, %1U, %2U и т.д.
        for (int i = 0; i < param_count && i < 99; i++) {  // Ограничение до 99 параметров
            char placeholder[16];  // Увеличен размер буфера
            snprintf(placeholder, sizeof(placeholder), "%%%dU", i);
            str_replace(temp_buffer, MAX_STRING_LENGTH, result_buffer, placeholder, params[i]);
            strcpy(result_buffer, temp_buffer);
        }
    }

    return result_buffer;
}

// Дополнительные строки вынесены в local_extra.c


