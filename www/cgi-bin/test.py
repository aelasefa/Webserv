#!/usr/bin/env python3

import os
import sys

body = sys.stdin.read()

print("Content-Type: text/html\n")
print("<html><body>")
print("<h1>CGI WORKS</h1>")
print("<p>Method: " + os.environ.get("REQUEST_METHOD", "") + "</p>")
print("<p>Body: " + body + "</p>")
print("</body></html>")