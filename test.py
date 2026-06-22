#!/usr/bin/env python3

import socket
import threading
import time
import requests

HOST = "127.0.0.1"
PORT = 8080

BASE_URL = f"http://{HOST}:{PORT}"


# =========================================
# RAW SOCKET HELPERS
# =========================================

def send_raw_http(request):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))

    s.sendall(request.encode())

    response = b""

    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            response += chunk
    except:
        pass

    s.close()
    return response.decode(errors="ignore")


# =========================================
# BASIC METHODS
# =========================================

def test_get():
    print("\n[GET]")
    r = requests.get(BASE_URL)
    print("Status:", r.status_code)


def test_404():
    print("\n[404]")
    r = requests.get(BASE_URL + "/notfound")
    print("Status:", r.status_code)


def test_post():
    print("\n[POST]")
    r = requests.post(
        BASE_URL,
        data="hello webserv"
    )
    print("Status:", r.status_code)


def test_delete():
    print("\n[DELETE]")
    r = requests.delete(BASE_URL + "/delete_me.txt")
    print("Status:", r.status_code)


# =========================================
# LARGE BODY
# =========================================

def test_large_body():
    print("\n[LARGE BODY]")

    data = "A" * (1024 * 1024)

    r = requests.post(
        BASE_URL,
        data=data
    )

    print("Status:", r.status_code)


# =========================================
# LARGE HEADER
# =========================================

def test_large_header():

    print("\n[LARGE HEADER]")

    req = (
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        f"X-Test: {'A'*10000}\r\n"
        "\r\n"
    )

    resp = send_raw_http(req)

    print(resp[:200])


# =========================================
# CHUNKED REQUEST
# =========================================

def test_chunked():

    print("\n[CHUNKED]")

    req = (
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "6\r\n"
        " World\r\n"
        "0\r\n"
        "\r\n"
    )

    resp = send_raw_http(req)

    print(resp[:300])


# =========================================
# INVALID METHOD
# =========================================

def test_invalid_method():

    print("\n[INVALID METHOD]")

    req = (
        "FAKE / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    )

    resp = send_raw_http(req)

    print(resp[:200])


# =========================================
# MALFORMED REQUEST
# =========================================

def test_malformed():

    print("\n[MALFORMED REQUEST]")

    req = (
        "GETTTTTTTTT\r\n"
        "\r\n"
    )

    resp = send_raw_http(req)

    print(resp[:200])


# =========================================
# KEEP ALIVE
# =========================================

def test_keep_alive():

    print("\n[KEEP ALIVE]")

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))

    req1 = (
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
    )

    req2 = (
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    )

    s.sendall(req1.encode())
    print(s.recv(4096).decode(errors="ignore")[:100])

    s.sendall(req2.encode())
    print(s.recv(4096).decode(errors="ignore")[:100])

    s.close()


# =========================================
# PIPELINING
# =========================================

def test_pipeline():

    print("\n[PIPELINE]")

    req = (
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    )

    resp = send_raw_http(req)

    print(resp[:500])


# =========================================
# CONCURRENT CLIENTS
# =========================================

def worker():

    try:
        r = requests.get(BASE_URL)
        print(r.status_code)
    except Exception as e:
        print("ERR", e)


def test_concurrent():

    print("\n[CONCURRENT 100 CLIENTS]")

    threads = []

    for _ in range(100):
        t = threading.Thread(target=worker)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()


# =========================================
# SLOW CLIENT
# =========================================

def test_slowloris():

    print("\n[SLOW CLIENT]")

    s = socket.socket()
    s.connect((HOST, PORT))

    s.send(b"GET / HTTP/1.1\r\n")

    time.sleep(10)

    s.send(b"Host: localhost\r\n")

    time.sleep(10)

    s.send(b"\r\n")

    print(s.recv(4096).decode(errors="ignore")[:200])

    s.close()


# =========================================
# MAIN
# =========================================

if __name__ == "__main__":

    test_get()
    test_404()
    test_post()
    test_delete()

    test_large_body()
    test_large_header()

    test_chunked()

    test_invalid_method()
    test_malformed()

    test_keep_alive()
    test_pipeline()

    test_concurrent()

    test_slowloris()

    print("\nDONE")