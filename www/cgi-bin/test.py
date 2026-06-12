#!/usr/bin/env python3

import os
import sys
import json
import time

# read POST body
body = sys.stdin.read()

print("Content-Type: text/html; charset=UTF-8")
print()

print("<html>")
print("<body style='font-family:monospace'>")

print("<h1>🚀 CGI STRESS TEST</h1>")

# ---------------- METHOD ----------------
print("<h2>METHOD</h2>")
print("<pre>" + os.environ.get("REQUEST_METHOD", "") + "</pre>")

# ---------------- QUERY ----------------
print("<h2>QUERY STRING</h2>")
print("<pre>" + os.environ.get("QUERY_STRING", "") + "</pre>")

# ---------------- HEADERS ----------------
print("<h2>CONTENT TYPE</h2>")
print("<pre>" + os.environ.get("CONTENT_TYPE", "") + "</pre>")

print("<h2>CONTENT LENGTH</h2>")
print("<pre>" + os.environ.get("CONTENT_LENGTH", "") + "</pre>")

# ---------------- BODY ----------------
print("<h2>BODY</h2>")
print("<pre>" + body + "</pre>")

# ---------------- ENV DUMP ----------------
print("<h2>ENV DUMP (partial)</h2>")
print("<pre>")

for k in ["REQUEST_METHOD", "QUERY_STRING", "CONTENT_TYPE", "CONTENT_LENGTH", "SCRIPT_FILENAME"]:
    print(f"{k}={os.environ.get(k, '')}")

print("</pre>")

# ---------------- JSON TEST ----------------
print("<h2>JSON PARSE TEST</h2>")
try:
    data = json.loads(body) if body else {}
    print("<pre>" + json.dumps(data, indent=2) + "</pre>")
except:
    print("<pre>Not JSON or invalid</pre>")

# ---------------- TIME TEST ----------------
print("<h2>TIME</h2>")
print("<pre>" + time.ctime() + "</pre>")

# ---------------- LARGE OUTPUT ----------------
print("<h2>LOOP TEST</h2>")
print("<pre>")
for i in range(20):
    print("line " + str(i))
print("</pre>")

print("</body>")
print("</html>")