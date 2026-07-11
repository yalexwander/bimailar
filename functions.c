#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "functions.h"
#include "structs.h"

/*
 * Освобождает всю память, выделенную для сообщения MailarMessage
 * и всех вложенных структур. Функция безопасна для NULL‑указателей.
 */
void mailar_message_free(MailarMessage *msg)
{
    if (!msg) {
        return;
    }

    /* Освобождаем массив родителей */
    if (msg->parents) {
        free(msg->parents);
        msg->parents = NULL;
    }

    /* Освобождаем массив заголовков */
    if (msg->headers) {
        free(msg->headers);
        msg->headers = NULL;
    }

    /* Освобождаем тело сообщения */
    if (msg->body) {
        free(msg->body);
        msg->body = NULL;
    }

    /* Освобождаем массив вложений */
    if (msg->attachments) {
        for (uint16_t i = 0; i < msg->attachment_count; ++i) {
            free_MailarAttachment(msg->attachments[i]);
        }
        free(msg->attachments);
        msg->attachments = NULL;
    }

    /* Наконец освобождаем саму структуру */
    free(msg);
}


void free_MailarAttachment(MailarAttachment* a) {
    if (a->name_data) {
        free(a->name_data);
        a->name_data = NULL;
    }
    if (a->body_data) {
        free(a->body_data);
        a->body_data = NULL;
    }
    free(a);
    a = NULL;
}
