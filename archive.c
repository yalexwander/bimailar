#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "structs.h"
#include "functions.h"


void mailar_message_append_to_archive(MailarMessage **msgs, size_t count, const char *archive)
{
    uint32_t last_id = mailar_archive_get_last_id(archive);

    FILE *fp = fopen(archive, "ab");
    if (!fp) {
        mailar_debug_print("Failed to open archive for appending");
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        MailarMessage *msg = msgs[i];
        if (!msg->id) {
            msg->id = last_id + i;
        }
        unsigned char *buf = mailar_serialize_message(msg);
        if (!buf) {
            mailar_debug_print("Failed to serialize message");
            continue;
        }
        size_t written = fwrite(buf, 1, msg->length, fp);
        if (written != msg->length) {
            mailar_debug_print("Failed to write complete message to archive");
        }

        free(buf);
    }

    fclose(fp);
}

uint32_t mailar_archive_get_last_id(const char *archive) 
{
    uint32_t last_id = 0;
    /* Открываем файл для чтения, чтобы узнать последний id */
    FILE *f = fopen(archive, "rb");
    if (!f) {
        /* Файл не существует – создаём новый и ставим id = 1 */
        last_id = 1;
    } else {
        /* Считываем последний 4‑байтовый id */
        if (fseek(f, -4, SEEK_END) == 0) {
            if (fread(&last_id, 1, 4, f) == 4) {
                last_id++;
            } else {
                last_id = 1;
            }
        } else {
            /* Не удалось переместиться – считаем, что файл пустой */
            last_id = 1;
        }
        fclose(f);
    }

    return last_id;
}


/*
 * Чтение одного сообщения из открытого файла.
 * Файл должен быть позиционирован на начало сообщения.
 * Функция читает длину сообщения, затем всю запись,
 * вызывает mailar_parse_message и возвращает структуру.
 */
MailarMessage* mailar_archive_file_read_message(FILE *f)
{
    if (!f) {
        return NULL;
    }

    /* Считываем 4‑байтовый заголовок длины */
    uint32_t length;
    if (fread(&length, 1, 4, f) != 4) {
        return NULL;
    }

    /* Если длина меньше 4, это некорректно */
    if (length < 4) {
        return NULL;
    }

    /* Выделяем буфер для всей записи */
    char *buf = malloc(length);
    if (!buf) {
        return NULL;
    }

    /* Скопируем уже прочитанные 4 байта */
    memcpy(buf, &length, 4);

    /* Считываем оставшуюся часть сообщения */
    if (fread(buf + 4, 1, length - 4, f) != length - 4) {
        free(buf);
        return NULL;
    }

    /* Парсим сообщение */
    MailarMessage *msg = mailar_parse_message(buf);

    /* Освобождаем временный буфер */
    free(buf);

    return msg;
}

MailarArchive* mailar_archive_full_load(char *fileName)
{
    FILE *f = fopen(fileName, "rb");
    if (!f) {
        return NULL;
    }

    MailarArchive* archive = malloc(sizeof(MailarArchive));

    size_t capacity = 8;
    size_t count = 0;
    MailarMessage **messages = calloc(capacity, sizeof(MailarMessage*));
    if (!messages) {
        fclose(f);
        return NULL;
    }

    while (1) {
        MailarMessage *msg = mailar_archive_file_read_message(f);
        if (!msg) {
            break; /* EOF or error */
        }

        if (count >= capacity) {
            capacity *= 2;
            MailarMessage **tmp = realloc(messages, capacity * sizeof(MailarMessage*));
            if (!tmp) {
                /* В случае ошибки освобождаем уже прочитанные сообщения */
                for (size_t i = 0; i < count; ++i) {
                    mailar_message_free(messages[i]);
                }
                free(messages);
                fclose(f);
                return NULL;
            }
            messages = tmp;
        }

        messages[count++] = msg;
    }

    /* Завершаем массив NULL‑указателем */
    if (count >= capacity) {
        MailarMessage **tmp = realloc(messages, (capacity + 1) * sizeof(MailarMessage*));
        if (tmp) {
            messages = tmp;
        }
    }
    messages[count] = NULL;

    fclose(f);

    archive->messages = messages;

    archive->headers = mailar_header_load_names(fileName);
    if (! archive->headers) {
        archive->headers = mailar_init_headerset();
    }

    archive->tags = mailar_tag_load_names(fileName);
    if (! archive->tags) {
        archive->tags = mailar_init_tagset();
    }


    return archive;
}
