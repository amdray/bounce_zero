#include "sound.h"
#include "level.h"  // Для SOUND_*_NAME констант
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pspaudiolib.h>
#include <pspaudio.h>
#include <pspintrman.h>  // Для отключения прерываний

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Fixed-point wavetable для оптимизации производительности
#define WAVETABLE_SIZE 1024
#define PHASE_BITS 32
#define PHASE_MASK 0xFFFFFFFF
#define PSP_SR 44100  // PSP sample rate - централизованная константа

// Константы огибающей (~2-3 мс атака/релиз при 44.1 кГц)
#define ATT_Q15  512   // ≈0.0156
#define REL_Q15  512
#define OTT_BUFFER_SIZE 16738  // Максимальный размер OTT буфера (как в оригинале)

// Предвычисленная синусоидальная таблица (16-bit signed samples)
static short g_sine_table[WAVETABLE_SIZE];
static int g_wavetable_initialized = 0;

// Инициализация wavetable (вызывается один раз)
static void init_wavetable(void) {
    if (g_wavetable_initialized) return;
    
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float angle = 2.0f * (float)M_PI * (float)i / (float)WAVETABLE_SIZE;
        float sine_value = sinf(angle);
        // Полная амплитуда 16-bit: ±32767 (масштабирование вынесено в микшер)
        g_sine_table[i] = (short)(sine_value * 32767.0f);
    }
    
    g_wavetable_initialized = 1;
}

// Конвертация частоты в fixed-point приращение фазы
static uint32_t frequency_to_phase_inc(float frequency, int sample_rate) {
    if (frequency <= 0.0f) return 0;
    
    // phase_inc = (frequency / sample_rate) * 2^32
    // Для PSP_SR (44100 Hz): phase_inc ≈ freq * 97391
    double phase_inc_float = ((double)frequency / (double)sample_rate) * 4294967296.0; // 2^32
    return (uint32_t)phase_inc_float;
}

// Глобальные переменные для аудио системы
static struct ott_info_t g_hoop_sound;
static struct ott_player_t g_hoop_player;
static struct ott_info_t g_pickup_sound;
static struct ott_player_t g_pickup_player;
static struct ott_info_t g_pop_sound;
static struct ott_player_t g_pop_player;
static int g_sound_initialized = 0;

// извлечено из general.c - точная копия
int reverse_tempo(int l)
{
    short int tempo_code[32] = { 25,28,31,35,40,45,50,56,63,70,80,90,100,
                                112,125,140,160,180,200,225,250,285,320,
                                355,400,450,500,565,635,715,800,900 };
    return tempo_code[l];
}

// Extracted from parse_sckl.c - с защитой от переполнения буфера
int get_bits(unsigned char *buffer, int *ptr, int *bitptr, int bits)
{
    unsigned int holding;
    int i;

    // Защита от чтения за границами буфера (размер 16738)
    if (*ptr >= OTT_BUFFER_SIZE - 1) {
        return 0; // Возвращаем 0 если достигли конца буфера
    }

    holding = (buffer[*ptr] << 8) + buffer[*ptr + 1];
    i = (holding >> (16 - (bits + (*bitptr)))) & ((1 << bits) - 1);

    *bitptr = *bitptr + bits;
    if (*bitptr > 7)
    {
        *bitptr = *bitptr - 8;
        (*ptr)++;
    }

    return i;
}

// Modified parse_ringtone to store notes instead of outputting them
int parse_ringtone(unsigned char *buffer, int ptr, struct ott_info_t *ott_info)
{
    int bitptr;
    int k, t, x;
    int patterns, count;
    int pattern_id;

    bitptr = 0; 
    t = 0;
    ott_info->note_count = 0;

    k = get_bits(buffer, &t, &bitptr, 8);
    get_bits(buffer, &t, &bitptr, 8);
    k = get_bits(buffer, &t, &bitptr, 7);
    k = get_bits(buffer, &t, &bitptr, 3);

    if (k != 1 && k != 2)
    {
        // Неподдерживаемый тип песни - просто возвращаем ошибку
        return 0;
    }

    k = get_bits(buffer, &t, &bitptr, 4);

    // Extract song name
    for (x = 0; x < k; x++)
    {
        ott_info->songname[x] = get_bits(buffer, &t, &bitptr, 8);
    }
    ott_info->songname[x] = 0;

    patterns = get_bits(buffer, &t, &bitptr, 8);

    while (t < ptr && t < OTT_BUFFER_SIZE - 1)  // Дополнительная защита от переполнения
    {
        if (patterns == 0) break;

        get_bits(buffer, &t, &bitptr, 3);
        get_bits(buffer, &t, &bitptr, 2);
        ott_info->loop = get_bits(buffer, &t, &bitptr, 4);
        count = get_bits(buffer, &t, &bitptr, 8);
        
        // Защита от слишком большого количества нот
        if (count > 100) count = 100; // Разумное ограничение

        for (x = 0; x < count; x++)
        {
            if (t >= ptr || t >= OTT_BUFFER_SIZE - 1) break; // Двойная проверка границ

            k = get_bits(buffer, &t, &bitptr, 3);
            
            if (k == 0)
            {
                pattern_id = get_bits(buffer, &t, &bitptr, 2);
                (void)pattern_id; // подавляем предупреждение об неиспользуемой переменной
                // значение считано, но не обрабатывается (как в оригинале ringtonetools)
            }
            else if (k == 1)
            {
                // This is a note - store it completely
                if (ott_info->note_count < 1024) 
                {
                    struct ott_note_t *note = &ott_info->notes[ott_info->note_count];
                    note->tone = get_bits(buffer, &t, &bitptr, 4);
                    note->length = get_bits(buffer, &t, &bitptr, 3);
                    note->modifier = get_bits(buffer, &t, &bitptr, 2);
                    note->scale = ott_info->scale;
                    note->style = ott_info->style;
                    note->bpm = ott_info->bpm;
                    note->volume = ott_info->volume;
                    ott_info->note_count++;
                }
            }
            else if (k == 2)
            {
                ott_info->scale = get_bits(buffer, &t, &bitptr, 2);
                if (ott_info->scale > 0) ott_info->scale--;
            }
            else if (k == 3)
            {
                ott_info->style = get_bits(buffer, &t, &bitptr, 2);
            }
            else if (k == 4)
            {
                k = get_bits(buffer, &t, &bitptr, 5);
                ott_info->bpm = reverse_tempo(k);
            }
            else if (k == 5)
            {
                ott_info->volume = get_bits(buffer, &t, &bitptr, 4);
            }
        }

        if (t >= ptr) break;
        patterns--;
    }

    return 0;
}

// Extracted from parse_sckl.c - EXACT COPY with minimal modifications
int parse_ott(FILE *in, struct ott_info_t *ott_info)
{
    unsigned char buffer[OTT_BUFFER_SIZE];
    int ptr, ch;

    // Initialize structure
    memset(ott_info, 0, sizeof(struct ott_info_t));
    memset(buffer, 0, sizeof(buffer));
    
    // Read file completely - EXACT COPY of original parse_ott
    for (ptr = 0; ptr < (int)sizeof(buffer); ptr++)
    {
        ch = getc(in);
        if (ch == EOF) break;
        buffer[ptr] = ch;
    }

    // Parse the buffer
    parse_ringtone(buffer, ptr, ott_info);

    return 0;
}

// PSP Audio Implementation

// Frequency table from ringtonetools WAV generator (EXACT COPY)
static double tone_freqs[13] = { 
    0, 261.625, 277.175, 293.675, 311.125, 329.625, 349.225, 
    370, 392, 415.3, 440, 466.15, 493.883 
};
// Tone mapping: P, C, C#, D, D#, E, F, F#, G, G#, A, A#, B

// конвертируем тон OTT + октаву в частоту (Hz)
float ott_tone_to_frequency(int tone, int scale) {
    if (tone < 0 || tone > 12) return 0.0f; // Pause or invalid
    if (tone == 0) return 0.0f; // Pause
    
    // Formula from ringtonetools: freq * (1 << scale)
    return (float)(tone_freqs[tone] * (1 << scale));
}

// конвертируем длину OTT + BPM в длительность в секундах
float ott_length_to_duration(int length, int bpm) {
    if (bpm <= 0) bpm = 120; // Default tempo
    if (length < 0 || length > 7) length = 2; // Default to quarter note если невалидный
    
    // From ringtonetools: samples_per_beat = sample_rate * (60/bpm)
    // samples_per_note = samples_per_beat * (4 / (1 << length))
    // duration = samples_per_note / sample_rate
    // 
    // length интерпретация (как в Nokia OTT формате):
    // 0 = whole note (1), 1 = half note (1/2), 2 = quarter (1/4), 3 = eighth (1/8), etc.
    float beats_per_minute = (float)bpm;
    float seconds_per_beat = 60.0f / beats_per_minute;
    float note_fraction = 4.0f / (float)(1 << length); // whole=4, half=2, quarter=1, eighth=0.5, etc
    
    return seconds_per_beat * note_fraction;
}

// Initialize OTT player
void ott_player_init(struct ott_player_t *player, struct ott_info_t *ott_info) {
    player->ott_info = ott_info;
    player->current_note = 0;
    player->note_time = 0.0f;
    player->note_duration = 0.0f;
    player->frequency = 0.0f;
    player->phase = 0;          // Fixed-point фаза
    player->phase_inc = 0;      // Fixed-point приращение
    player->env_q15 = 0;        // Инициализация огибающей
    player->is_playing = 0;
    player->sample_rate = PSP_SR; // PSP standard
}

// Start playing OTT - с защитой от race conditions
void ott_player_start(struct ott_player_t *player) {
    // Отключаем прерывания на короткое время для атомарности
    int intr = sceKernelCpuSuspendIntr();

    player->current_note = 0;
    player->note_time = 0.0f;
    player->phase = 0;     // Сбрасываем fixed-point фазу
    player->phase_inc = 0; // Сбрасываем приращение
    // НЕ сбрасываем env_q15 - плавный переход от текущего уровня
    
    if (player->ott_info->note_count > 0) {
        struct ott_note_t *note = &player->ott_info->notes[0];
        player->frequency = ott_tone_to_frequency(note->tone, note->scale);
        player->note_duration = ott_length_to_duration(note->length, note->bpm);
        // Вычисляем fixed-point приращение фазы
        player->phase_inc = frequency_to_phase_inc(player->frequency, player->sample_rate);
    }
    
    // Устанавливаем флаг воспроизведения последним (после всех полей)
    player->is_playing = 1;
    
    sceKernelCpuResumeIntr(intr);
}

// Stop playing OTT - с защитой от race conditions  
void ott_player_stop(struct ott_player_t *player) {
    // Простая атомарная операция - достаточно флага
    player->is_playing = 0;
}

// Генерация сэмпла от одного плеера - с огибающей для устранения щелчков
static short generate_player_sample(struct ott_player_t *player, float sample_length) {
    if (!player || !player->ott_info) return 0;

    // Планирование нот – только когда плеер активен
    if (player->is_playing) {
        if (player->note_time >= player->note_duration) {
            player->current_note++;
            player->note_time = 0.0f;

            if (player->current_note >= player->ott_info->note_count) {
                if (player->ott_info->loop) {
                    player->current_note = 0;
                } else {
                    player->is_playing = 0; // перейдём в релиз
                }
            }

            if (player->is_playing) {
                struct ott_note_t *note = &player->ott_info->notes[player->current_note];
                player->frequency     = ott_tone_to_frequency(note->tone, note->scale);
                player->note_duration = ott_length_to_duration(note->length, note->bpm);
                player->phase_inc     = frequency_to_phase_inc(player->frequency, player->sample_rate);
                // НЕ сбрасываем фазу - плавный переход частоты
                // НЕ сбрасываем env_q15 - продолжаем с текущего уровня
            }
        }
    }

    // Осциллятор работает, если либо играем ноту, либо ещё не выдохся релиз
    const int osc_on = (player->is_playing && player->phase_inc) || (player->env_q15 > 0);

    short s = 0;
    if (osc_on && player->phase_inc) {
        uint32_t table_index = (player->phase >> (PHASE_BITS - 10)) & (WAVETABLE_SIZE - 1);
        s = g_sine_table[table_index];
        player->phase += player->phase_inc;
    }

    // Огибающая: атака при проигрывании, релиз — иначе (плавный сход к нулю)
    if (player->is_playing && player->phase_inc) {
        player->env_q15 += ((32767 - player->env_q15) * ATT_Q15) >> 15;
    } else {
        player->env_q15 -= (player->env_q15 * REL_Q15) >> 15;
    }

    // Применить огибающую
    s = (short)(((int)s * (int)player->env_q15) >> 15);

    // Время ноты тикает только когда играем
    if (player->is_playing) {
        player->note_time += sample_length;
    }
    return s;
}

// PSP Audio callback - смешивает все активные плееры
void ott_audio_callback(void* buf, unsigned int length, void *userdata) {
    (void)userdata; // Не используем userdata, работаем со всеми плеерами
    psp_sample_t *samples = (psp_sample_t *)buf;
    
    const float sample_length = 1.0f / (float)PSP_SR; // PSP sample rate
    
    for (unsigned int i = 0; i < length; i++) {
        // Генерируем сэмплы от каждого плеера
        short s0 = generate_player_sample(&g_hoop_player, sample_length);
        short s1 = generate_player_sample(&g_pickup_player, sample_length);
        short s2 = generate_player_sample(&g_pop_player, sample_length);

        // Активность каналов — по флагу плеера и наличию фазы/частоты
        int active = 0;
        if (g_hoop_player.is_playing && g_hoop_player.phase_inc) active++;
        if (g_pickup_player.is_playing && g_pickup_player.phase_inc) active++;
        if (g_pop_player.is_playing && g_pop_player.phase_inc) active++;

        int mix = (int)s0 + (int)s1 + (int)s2;

        // Нормализация: делим только если >1 активного канала
        if (active > 1) {
            // Среднее арифметическое для предотвращения клиппинга
            mix /= active;
            
            // TODO: Альтернатива - equal-power коэффициенты для более музыкального звучания:
            // static const int gain_q15[4] = { 32767, 32767, 23170, 18919 }; // 1.0, 1.0, 0.707, 0.577
            // mix = (mix * gain_q15[active]) >> 15;
        }

        // Clamp to 16-bit range
        if (mix >  32767) mix =  32767;
        if (mix < -32768) mix = -32768;

        samples[i].l = (short)mix;
        samples[i].r = (short)mix;
    }
}

// Высокоуровневый API для игры

// Инициализация аудио системы
int sound_init(void) {
    if (g_sound_initialized) {
        return 1; // Уже инициализирована
    }
    
    // Инициализация PSP audio с проверкой возврата
    int audio_result = pspAudioInit();
    if (audio_result < 0) {
        // Ошибка инициализации PSP audio системы
        return 10; // PSP audio init failed
    }

    // Инициализация wavetable для оптимизации производительности
    init_wavetable();

    // Загружаем up.ott для звука кольца
    FILE *up_file = util_open_file(SOUND_HOOP_NAME, "rb");
    if (!up_file) {
        return 2; // hoop файл не найден
    }

    if (parse_ott(up_file, &g_hoop_sound) != 0) {
        fclose(up_file);
        return 3; // hoop парсинг ошибка
    }
    fclose(up_file);

    // Загружаем pickup.ott для звука сбора предметов
    FILE *pickup_file = util_open_file(SOUND_PICKUP_NAME, "rb");
    if (!pickup_file) {
        return 4; // pickup файл не найден
    }

    if (parse_ott(pickup_file, &g_pickup_sound) != 0) {
        fclose(pickup_file);
        return 5; // pickup парсинг ошибка
    }
    fclose(pickup_file);

    // Загружаем pop.ott для звука лопания мяча
    FILE *pop_file = util_open_file(SOUND_POP_NAME, "rb");
    if (!pop_file) {
        return 6; // pop файл не найден
    }

    if (parse_ott(pop_file, &g_pop_sound) != 0) {
        fclose(pop_file);
        return 7; // pop парсинг ошибка
    }
    fclose(pop_file);
    
    // Инициализируем плееры
    ott_player_init(&g_hoop_player, &g_hoop_sound);
    ott_player_init(&g_pickup_player, &g_pickup_sound);
    ott_player_init(&g_pop_player, &g_pop_sound);
    
    // Устанавливаем callback для PSP audio (миксер обрабатывает все плееры)
    // ПРИМЕЧАНИЕ: Используем один канал (0) с микшером для экономии ресурсов PSP
    // Альтернатива: отдельные каналы 0,1,2 для hoop/pickup/pop соответственно
    // Примечание: pspAudioSetChannelCallback возвращает void, ошибки не проверяются
    pspAudioSetChannelCallback(0, ott_audio_callback, NULL);
    
    // Устанавливаем громкость канала (0x8000 = максимум)  
    // Примечание: pspAudioSetVolume возвращает void, ошибки не проверяются
    pspAudioSetVolume(0, 0x6000, 0x6000);  // ~75% громкости
    
    g_sound_initialized = 1;
    return 1; // Успешно
}

// Завершение работы аудио системы
void sound_shutdown(void) {
    if (g_sound_initialized) {
        // Останавливаем все плееры
        ott_player_stop(&g_hoop_player);
        ott_player_stop(&g_pickup_player);
        ott_player_stop(&g_pop_player);
        
        // Отключаем callback канала (игнорируем ошибки при shutdown)
        pspAudioSetChannelCallback(0, NULL, NULL);
        
        // Завершаем работу PSP audio системы (игнорируем ошибки при shutdown)
        pspAudioEnd();
        
        g_sound_initialized = 0;
    }
}

// Воспроизведение звука кольца
void sound_play_hoop(void) {
    if (g_sound_initialized) {
        ott_player_start(&g_hoop_player);
    }
}

// Воспроизведение звука сбора предметов
void sound_play_pickup(void) {
    if (g_sound_initialized) {
        ott_player_start(&g_pickup_player);
    }
}

// Воспроизведение звука лопания мяча
void sound_play_pop(void) {
    if (g_sound_initialized) {
        ott_player_start(&g_pop_player);
    }
}

// Установка общей громкости звука (0x0000 = тишина, 0x8000 = максимум)
void sound_set_volume(int volume) {
    if (g_sound_initialized) {
        // Ограничиваем диапазон
        if (volume < 0) volume = 0;
        if (volume > 0x8000) volume = 0x8000;
        
        pspAudioSetVolume(0, volume, volume);
    }
}
