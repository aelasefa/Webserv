#!/usr/bin/env python3
import sys
import os

print("Content-Type: text/plain\r\n\r\n", end="")

# Read exactly CONTENT_LENGTH bytes to avoid blocking if the server pipe is kept open
try:
    content_length = int(os.environ.get("CONTENT_LENGTH", 0))
    body = sys.stdin.read(content_length)
except ValueError:
    body = ""

print(f"Received {len(body)} bytes.")
print(f"Content: {body}")