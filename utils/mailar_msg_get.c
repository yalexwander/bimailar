#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../functions.h"
#include "../structs.h"

/*
 * Утилита mailar_msg_get
 *
 * Параметры:
 *   -m <archive file name>   (обязательный)
 *   -i <message_id>          (необязательный)
 *
 * Если указан -i, выводится только сообщение с заданным id.
 * Иначе выводятся все сообщения архива.
 * Вывод осуществляется через mailar_message_dump().
 */
int main(int argc, char *argv[])
{
    const char *archive_name = NULL;
    uint32_t  target_id = 0;
    int       id_specified = 0;

    /* Простейший парсинг аргументов */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            archive_name = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            target_id = (uint32_t)strtoul(argv[++i], NULL, 10);
            id_specified = 1;
        } else {
            fprintf(stderr, "Usage: %s -m <archive> [-i <message_id>]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!archive_name) {
        fprintf(stderr, "Ошибка: необходимо указать архив с помощью -m\n");
        return EXIT_FAILURE;
    }

    /* Читаем архив */
    MailarArchive *archive = mailar_archive_full_load((char *)archive_name);
    MailarMessage **messages = archive->messages;
    if (!messages) {
        fprintf(stderr, "Не удалось открыть архив: %s\n", archive_name);
        return EXIT_FAILURE;
    }

    /* Выводим сообщения */
    for (size_t idx = 0; messages[idx] != NULL; ++idx) {
        MailarMessage *msg = messages[idx];
        if (id_specified) {
            if (msg->id == target_id) {
                mailar_message_dump(msg, archive);
                break;
            }
        } else {
            mailar_message_dump(msg, archive);
        }
    }

    /* Очистка памяти */
    for (size_t idx = 0; messages[idx] != NULL; ++idx) {
        mailar_message_free(messages[idx]);
    }
    free(messages);

    return EXIT_SUCCESS;
}
