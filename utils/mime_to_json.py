#!/usr/bin/env python3
"""
Преобразует все файлы в указанной директории в JSON‑объекты,
представляющие MIME‑сообщения.

Для каждого файла:
  - читается как MIME‑сообщение (модуль email)
  - извлекаются заголовки:
      From, To, Date, Subject, X-Flag, Message-Id
  - тело сообщения берётся как строка (если multipart – берётся первый
    не‑multipart‑часть)
  - формируется словарь с ключами "headers" и "body"
  - печатается JSON‑строка (pretty‑print, indent=2)

Параметры:
  1) путь к директории, содержащей файлы
"""

import os
import sys
import json
import email
from email import policy
from email.utils import parsedate_to_datetime
from email.parser import BytesParser
import subprocess
import re
import getopt

outfile_json = "./tmp-import.json"
slice_step = 1000
bimailar_importer_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), "bimailar_import_json")
headers_to_keep = {}

def flush_outdata(outdata, bimailar_outfile):
    with open(outfile_json, mode="w") as file_obj:
        json_data = json.dumps(outdata, ensure_ascii=False)
        file_obj.write(json_data)

    subprocess.run([bimailar_importer_path, "-j", outfile_json, "-a", bimailar_outfile])


def extract_body(msg):
    """
    Возвращает тело сообщения как строку.
    Если сообщение multipart, берём первую часть, которая не является
    multipart‑частью. Если тело не найдено – возвращаем пустую строку.
    """
    if msg.is_multipart():
        for part in msg.iter_parts():
            if not part.is_multipart():
                payload = part.get_payload(decode=True)
                if payload is not None:
                    return payload.decode(part.get_content_charset() or 'utf-8', errors='replace')
        return ""
    else:
        payload = msg.get_payload(decode=True)
        if payload is not None:
            return payload.decode(msg.get_content_charset() or 'utf-8', errors='replace')
        return ""

def process_file(path):
    """
    Читает файл, парсит как MIME‑сообщение и выводит JSON‑строку.
    """

    global headers_to_keep

    with open(path, 'rb') as f:
        msg = BytesParser(policy=policy.default).parse(f)


    headers = {}
    for header_name, header_val in msg.items():
        if len(headers_to_keep) > 0:
            if headers_to_keep.get(header_name.lower()):
                headers[header_name] = header_val
        else:
            headers[header_name] = header_val

    data = {}

    if "Message-Id" in headers:
        matches = re.search("<([0-9]+)", msg.get("Message-Id", ""))
        data["id"] = int(matches[1])

    if "References" in headers:
        matches = re.search("<([0-9]+)", msg.get("References", ""))
        parent_id = 0
        if matches:
            data["parents"] = [ int(matches[1]) ]

    if "From" in headers:
        headers["From"] = headers["From"].split("<")[0]

    print("11")
    if "Date" in headers:
        data["timestamp"] = int( parsedate_to_datetime(headers["Date"]).timestamp() )
        data["flags"] = dict()
        data["flags"]["has_timestamp"] = True

    body = extract_body(msg)
    body = re.sub('\nfile: .+\n', '', body)

    data["headers"] = headers
    data["body"] = body

    return data

def usage():
    print(f"Usage: {sys.argv[0]} -d <maildir directory> -o <outfile> -h <headers,to,keep>", file=sys.stderr)

def main():
    global headers_to_keep
    opts, args = getopt.getopt(sys.argv[1:], "d:o:h:", ["help", "output="])

    root_dir = None

    bimailar_outfile = ""

    for o,a in opts:
        if o == "-d":
            root_dir = a
        elif o == "-o":
            bimailar_outfile = a
        elif o == "-h":
            for htk in a.split(","):
                headers_to_keep[htk.lower()] = True
        else:
            usage()
            exit(1)

    if not root_dir:
        usage()
        exit(1)

    if not os.path.isdir(root_dir):
        print(f"Error: {root_dir} is not a directory", file=sys.stderr)
        exit(1)

    outdata = []
    for dirpath, _, filenames in os.walk(root_dir):
        for filename in sorted(filenames):
            file_path = os.path.join(dirpath, filename)
            try:
                outdata.append(process_file(file_path))
                if len(outdata) >= slice_step:
                    flush_outdata(outdata, bimailar_outfile)
                    outdata = []
            except Exception as e:
                print(f"Failed to process {file_path}: {e}", file=sys.stderr)

    flush_outdata(outdata, bimailar_outfile)

if __name__ == "__main__":
    main()
