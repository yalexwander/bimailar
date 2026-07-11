#ifndef MAILAR_STRUCTS_H
#define MAILAR_STRUCTS_H

#include <stdint.h>

/*
 * Flags for a single message entry.
 * Each bit corresponds to a flag described in the archive format.
 */
typedef struct {
    uint16_t deleted          : 1; /* 1) deleted flag */
    uint16_t has_tags         : 1; /* 2) has tags flag */
    uint16_t maildir_owner    : 1; /* 3) maildir owner flag */
    uint16_t has_attachments  : 1; /* 4) has attachments flag */
    uint16_t has_parents      : 1; /* 5) has parents flag */
    uint16_t body_compressed  : 1; /* 6) body compressed flag */
    uint16_t non_utf8         : 1; /* 7) non utf-8 flag */
    uint16_t encryption       : 1; /* 8) encryption flag */
    uint16_t updated          : 1; /* 9) update flag */
    uint16_t has_timestamp    : 1; /* 10) has timestamp flag */
    uint16_t reserved         : 6; /* -16) reserved */
} MailarFlags;


/*
 * Header of archive.
 * header_id      – identifier of the header
 * name_length    – length of the header name
 * name           – pointer to the header name data (not null‑terminated)
 */
typedef struct {
    uint32_t header_id;
    uint16_t name_length;
    char    *name;
} MailarArchiveHeader;


/**
 * Header data
 */
typedef struct {
    uint32_t header_id;
    uint16_t data_length;
    char    *data;
} MailarMessageHeader;


/*
 * Set of headers associated with archive
 */
typedef struct {
    uint16_t headers_count;
    MailarArchiveHeader** headers;
    uint16_t encoding;
} MailarArchiveHeadersSet;

/*
 * Flags for a single attachment.
 * Each bit corresponds to a flag described in the archive format.
 */
typedef struct {
    uint8_t compressed : 1; /* 1) attachment_compressed flag */
    uint8_t encrypted  : 1; /* 2) attachment_encrypted flag */
    uint8_t external   : 1; /* 3) attachment_external flag */
    uint8_t inline_    : 1; /* 4) attachment_inline flag */
    uint8_t reserved   : 4; /* 5-8) reserved */
} MailarAttachmentFlags;

/*
 * Attachment of a message.
 * attachment_flags – flags for the attachment (compressed, encrypted, external, inline)
 * mime_type_id     – MIME type identifier
 * name_length      – length of the attachment name
 * name_data        – pointer to the attachment name (not null‑terminated)
 * body_length      – length of the attachment body
 * body_data        – pointer to the attachment body
 */
typedef struct {
    MailarAttachmentFlags attachment_flags;
    uint16_t mime_type_id;
    uint16_t name_length;
    uint8_t *name_data;
    uint32_t body_length;
    uint8_t *body_data;
} MailarAttachment;

/*
 * Tag of a message.
 * tag_id        – identifier of the tag
 * name_length   – length of the tag name
 * name          – pointer to the tag name data (not null‑terminated)
 */
typedef struct {
    uint32_t tag_id;
    uint16_t name_length;
    char    *name;
} MailarMessageTag;

/*
 * Full representation of a parsed message entry.
 * All variable‑length fields are represented by pointers and
 * corresponding count/length fields.
 */
typedef struct {
    uint32_t          length;          /* total length of the entry in bytes */
    uint32_t          id;              /* message id */
    MailarFlags       flags;           /* message flags */

    /* Timestamp */
    uint64_t          timestamp;    /* timestamp */

    /* Parents */
    uint16_t          parent_count;    /* number of parents (if has_parents) */
    uint32_t *        parents;         /* array of parent message ids */

    /* Encoding */
    uint16_t          encoding_id;     /* encoding id (if non_utf8) */

    /* Encryption type id (present if encryption flag set) */
    uint8_t           encryption_type_id;

    /* Attachments */
    uint16_t          attachment_count;/* number of attachments (if has_attachments) */
    uint8_t           body_compression_type; /* compression type (if body_compressed) */

    /* Headers */
    uint8_t      headers_count;    /* number of headers */
    uint16_t     tags_count;    /* number of tags */
    MailarMessageHeader    **headers;         /* array of headers */
    uint32_t    *tags;         /* array of tags */

    /* Body */
    uint32_t          body_length;     /* length of the body */
    uint8_t *         body;            /* body data */

    /* Attachments array */
    MailarAttachment **attachments;     /* array of attachments */
} MailarMessage;

/*
 * Set of Tags associated with archive
 */
typedef struct {
    uint16_t tags_count;
    MailarMessageTag** tags;
    uint16_t encoding;
} MailarArchiveTagsSet;

/*
 * In memory representation of fully loaded Bimailar archive
 */
typedef struct {
    uint32_t messages_count;
    MailarMessage** messages;
    MailarArchiveHeadersSet* headers;
    MailarArchiveTagsSet* tags;
} MailarArchive;


#endif /* MAILAR_STRUCTS_H */
