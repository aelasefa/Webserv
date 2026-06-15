#!/usr/bin/env python3
import os
import sys
import html
from urllib.parse import parse_qs


def read_body():
    length = os.environ.get("CONTENT_LENGTH", "")
    try:
        size = int(length)
    except ValueError:
        size = 0

    if size <= 0:
        return ""

    return sys.stdin.read(size)


def main():
    method = os.environ.get("REQUEST_METHOD", "GET")

    if method == "GET":
        data = os.environ.get("QUERY_STRING", "")
    else:
        data = read_body()

    params = parse_qs(data, keep_blank_values=True)

    print("Content-Type: text/html")
    print()
    print("<html><body>")
    print("<h1>CGI Echo</h1>")

    if not params:
        print("<p>No form data received.</p>")
    else:
        print("<ul>")
        for key in sorted(params.keys()):
            for value in params[key]:
                print("<li>{}: {}</li>".format(
                    html.escape(key),
                    html.escape(value)
                ))
        print("</ul>")

    print("</body></html>")


if __name__ == "__main__":
    main()