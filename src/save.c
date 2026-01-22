#include "types.h"
#include <pspdisplay.h>
#include <psputility.h>
#include <psputility_savedata.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Параметры системного сохранения PSP
#define SAVE_MAGIC 0x424F554E  // "BOUN" в hex
#define SAVE_GAME_NAME "BZERO0001"
#define SAVE_NAME "0000"
#define SAVE_FILE_NAME "DATA.BIN"
#define SAVE_DATA_SIZE 16u
#define SAVE_IO_BUFFER_SIZE 256u
#define SAVE_ERR_RW_NO_DATA 0x80110327
#define SAVE_ICON0_PATH "icons/ICON0.PNG"

// Глобальные данные сохранений
static SaveData g_save_data = {0};
static bool g_save_initialized = false;
static bool g_save_dirty = false;
static int g_save_last_load_result = 0;
static int g_save_last_save_result = 0;
static uint8_t g_save_io_buf[SAVE_IO_BUFFER_SIZE] __attribute__((aligned(64)));
static void* g_save_icon0_buf = NULL;
static size_t g_save_icon0_size = 0;

// Forward declaration
static void save_store_data(void);
static bool save_load_data(void* data_buf, size_t data_size);
static bool save_store_data_buffer(const void* data_buf, size_t data_size);
static void save_pack_data(uint8_t* data_buf, size_t data_size);
static bool save_unpack_data(const uint8_t* data_buf, size_t data_size);
static void save_init_dialog(SceUtilitySavedataParam* dialog, int mode, void* data_buf, size_t data_size);
static bool save_do_utility(int mode, void* data_buf, size_t data_size);
static void save_load_icon0(void);

void save_init(void) {
    if (g_save_initialized) return;
    
    // Попытка загрузить существующее сохранение
    if (save_load_data(g_save_io_buf, sizeof(g_save_io_buf))) {
        // Проверка валидности файла
        if (save_unpack_data(g_save_io_buf, SAVE_DATA_SIZE) && g_save_data.magic == SAVE_MAGIC) {
            // Файл корректен - данные загружены
        } else {
            // Файл поврежден - сброс к значениям по умолчанию
            memset(&g_save_data, 0, sizeof(SaveData));
            g_save_data.magic = SAVE_MAGIC;
            g_save_data.best_level = 1;  // По умолчанию доступен уровень 1
        }
        g_save_initialized = true;
    } else if (g_save_last_load_result == (int)SAVE_ERR_RW_NO_DATA) {
        // Файл не существует - первый запуск
        memset(&g_save_data, 0, sizeof(SaveData));
        g_save_data.magic = SAVE_MAGIC;
        g_save_data.best_level = 1;  // По умолчанию доступен уровень 1

        // Сохранить начальные данные
        g_save_initialized = true; // Должно быть установлено до вызова save_store_data()
        save_store_data();
    } else {
        // Ошибка доступа/чтения - не инициализируем и не пишем
        g_save_initialized = false;
        return;
    }
}

void save_shutdown(void) {
    if (!g_save_initialized) return;
    g_save_initialized = false;
    if (g_save_icon0_buf) {
        free(g_save_icon0_buf);
        g_save_icon0_buf = NULL;
        g_save_icon0_size = 0;
    }
}

void save_flush(void) {
    if (!g_save_initialized) return;
    save_store_data();
}

static void save_store_data(void) {
    if (!g_save_initialized) return;
    if (!g_save_dirty) return;

    g_save_data.magic = SAVE_MAGIC;
    save_pack_data(g_save_io_buf, SAVE_DATA_SIZE);
    if (!save_store_data_buffer(g_save_io_buf, SAVE_DATA_SIZE)) {
        // Ошибка записи - данные могут быть потеряны
        // В будущем можно добавить логирование или уведомление пользователя
    } else {
        g_save_dirty = false;
    }
}

static bool save_load_data(void* data_buf, size_t data_size) {
    return save_do_utility(SCE_UTILITY_SAVEDATA_READDATA, data_buf, data_size);
}

static bool save_store_data_buffer(const void* data_buf, size_t data_size) {
    bool ok = save_do_utility(SCE_UTILITY_SAVEDATA_WRITEDATA, (void*)data_buf, data_size);
    if (!ok && g_save_last_save_result == (int)SAVE_ERR_RW_NO_DATA) {
        // Первый запуск: создать слот и записать данные через MAKEDATA
        ok = save_do_utility(SCE_UTILITY_SAVEDATA_MAKEDATA, (void*)data_buf, data_size);
    }
    return ok;
}

static void save_pack_data(uint8_t* data_buf, size_t data_size) {
    uint32_t magic = (uint32_t)g_save_data.magic;
    int32_t best_level = (int32_t)g_save_data.best_level;
    int32_t best_score = (int32_t)g_save_data.best_score;

    if (!data_buf || data_size < SAVE_DATA_SIZE) return;

    memset(data_buf, 0, data_size);
    memcpy(data_buf + 0, &magic, sizeof(magic));
    memcpy(data_buf + 4, &best_level, sizeof(best_level));
    memcpy(data_buf + 8, &best_score, sizeof(best_score));
}

static bool save_unpack_data(const uint8_t* data_buf, size_t data_size) {
    uint32_t magic = 0;
    int32_t best_level = 0;
    int32_t best_score = 0;

    if (!data_buf || data_size < SAVE_DATA_SIZE) return false;

    memcpy(&magic, data_buf + 0, sizeof(magic));
    memcpy(&best_level, data_buf + 4, sizeof(best_level));
    memcpy(&best_score, data_buf + 8, sizeof(best_score));

    g_save_data.magic = (int)magic;
    g_save_data.best_level = (int)best_level;
    g_save_data.best_score = (int)best_score;
    return true;
}

static void save_init_dialog(SceUtilitySavedataParam* dialog, int mode, void* data_buf, size_t data_size) {
    int language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;

    memset(dialog, 0, sizeof(*dialog));
    dialog->base.size = sizeof(*dialog);
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &language) == 0) {
        dialog->base.language = language;
    } else {
        dialog->base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    }
    dialog->base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    dialog->base.graphicsThread = 0x11;
    dialog->base.accessThread = 0x13;
    dialog->base.fontThread = 0x12;
    dialog->base.soundThread = 0x10;

    dialog->mode = mode;
    dialog->overwrite = 1;

    strncpy(dialog->gameName, SAVE_GAME_NAME, sizeof(dialog->gameName) - 1);
    strncpy(dialog->saveName, SAVE_NAME, sizeof(dialog->saveName) - 1);
    strncpy(dialog->fileName, SAVE_FILE_NAME, sizeof(dialog->fileName) - 1);

    dialog->dataBuf = data_buf;
    dialog->dataBufSize = (SceSize)data_size;
    if (mode == SCE_UTILITY_SAVEDATA_READDATA) {
        dialog->dataSize = 0;
    } else {
        dialog->dataSize = (SceSize)data_size;
    }

    strncpy(dialog->sfoParam.title, "Bounce Zero", sizeof(dialog->sfoParam.title) - 1);
    strncpy(dialog->sfoParam.savedataTitle, "Bounce Zero", sizeof(dialog->sfoParam.savedataTitle) - 1);
    strncpy(dialog->sfoParam.detail, "Saved progress and records.", sizeof(dialog->sfoParam.detail) - 1);
    dialog->sfoParam.parentalLevel = 1;

    save_load_icon0();
    if (g_save_icon0_buf && g_save_icon0_size > 0) {
        dialog->icon0FileData.buf = g_save_icon0_buf;
        dialog->icon0FileData.bufSize = (SceSize)g_save_icon0_size;
        dialog->icon0FileData.size = (SceSize)g_save_icon0_size;
    }

}

static bool save_do_utility(int mode, void* data_buf, size_t data_size) {
    SceUtilitySavedataParam dialog;
    int init_rc = 0;

    if (!data_buf || data_size == 0) return false;

    save_init_dialog(&dialog, mode, data_buf, data_size);
    init_rc = sceUtilitySavedataInitStart(&dialog);
    if (init_rc < 0) return false;

    for (;;) {
        int status = sceUtilitySavedataGetStatus();
        if (status == PSP_UTILITY_DIALOG_INIT || status == PSP_UTILITY_DIALOG_VISIBLE) {
            sceUtilitySavedataUpdate(1);
        } else if (status == PSP_UTILITY_DIALOG_FINISHED || status == PSP_UTILITY_DIALOG_QUIT) {
            sceUtilitySavedataShutdownStart();
        } else if (status == PSP_UTILITY_DIALOG_NONE) {
            break;
        }

        sceDisplayWaitVblankStart();
    }

    if (mode == SCE_UTILITY_SAVEDATA_READDATA) {
        g_save_last_load_result = dialog.base.result;
    } else if (mode == SCE_UTILITY_SAVEDATA_WRITEDATA || mode == SCE_UTILITY_SAVEDATA_MAKEDATA) {
        g_save_last_save_result = dialog.base.result;
    }
    return dialog.base.result == 0;
}


static void save_load_icon0(void) {
    FILE* file = NULL;
    long size = 0;

    if (g_save_icon0_buf) return;

    file = util_open_file(SAVE_ICON0_PATH, "rb");
    if (!file) return;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return;
    }
    size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return;
    }

    g_save_icon0_buf = malloc((size_t)size);
    if (!g_save_icon0_buf) {
        fclose(file);
        return;
    }

    if (fread(g_save_icon0_buf, 1, (size_t)size, file) != (size_t)size) {
        free(g_save_icon0_buf);
        g_save_icon0_buf = NULL;
        fclose(file);
        return;
    }

    g_save_icon0_size = (size_t)size;
    fclose(file);
}

void save_update_records(int level, int score) {
    if (!g_save_initialized) return;
    
    int updated = 0;
    
    // Обновить максимальный уровень (как в оригинале BounceCanvas:179-180)
    if (level > g_save_data.best_level) {
        g_save_data.best_level = level;
        updated = 1;
    }
    
    // Обновить лучший счёт (как в оригинале BounceCanvas:184-187)
    if (score > g_save_data.best_score) {
        g_save_data.best_score = score;
        g_game.new_best_score = true;  // Установить флаг (как mNewBestScore в Java)
        updated = 1;
    }
    
    // Отложить запись до подходящего момента
    if (updated) {
        g_save_dirty = true;
    }
}

SaveData* save_get_data(void) {
    if (!g_save_initialized) {
        save_init();  // Автоматическая инициализация если забыли
    }
    return &g_save_data;
}
