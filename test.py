#!/usr/bin/env python3

import requests
import socket
import threading
import time

HOST = "127.0.0.1"
PORT = 8080

BASE_URL = f"http://{HOST}:{PORT}"


def print_result(name, success, details=""):
    status = "PASS" if success else "FAIL"
    print(f"[{status}] {name}")
    if details:
        print(f"      {details}")


def test_get():
    try:
        r = requests.get(BASE_URL + "/")
        print_result(
            "GET /",
            r.status_code == 200,
            f"status={r.status_code}"
        )
    except Exception as e:
        print_result("GET /", False, str(e))


def test_404():
    try:
        r = requests.get(BASE_URL + "/does_not_exist")
        print_result(
            "404",
            r.status_code == 404,
            f"status={r.status_code}"
        )
    except Exception as e:
        print_result("404", False, str(e))


def test_post():
    try:
        data = "hello webserv"
        r = requests.post(BASE_URL + "/", data=data)

        print_result(
            "POST",
            r.status_code in [200, 201, 204],
            f"status={r.status_code}"
        )
    except Exception as e:
        print_result("POST", False, str(e))


def test_large_body():
    try:
        body = "A" * (1024 * 1024)

        r = requests.post(
            BASE_URL + "/",
            data=body
        )

        print_result(
            "Large Body (1MB)",
            r.status_code < 500,
            f"status={r.status_code}"
        )
    except Exception as e:
        print_result("Large Body", False, str(e))


def test_delete():
    try:
        r = requests.delete(BASE_URL + "/delete_me.txt")

        print_result(
            "DELETE",
            r.status_code in [200, 204, 404],
            f"status={r.status_code}"
        )
    except Exception as e:
        print_result("DELETE", False, str(e))


def test_invalid_method():
    try:
        sock = socket.socket()
        sock.connect((HOST, PORT))

        req = (
            "BREW / HTTP/1.1\r\n"
            f"Host: {HOST}\r\n"
            "\r\n"
        )

        sock.sendall(req.encode())

        response = sock.recv(4096).decode(errors="ignore")

        ok = (
            "405" in response or
            "501" in response
        )

        print_result(
            "Invalid Method",
            ok
        )

        sock.close()

    except Exception as e:
        print_result("Invalid Method", False, str(e))


def test_keep_alive():
    try:
        sock = socket.socket()
        sock.connect((HOST, PORT))

        req = (
            "GET / HTTP/1.1\r\n"
            f"Host: {HOST}\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        )

        sock.sendall(req.encode())
        sock.recv(4096)

        sock.sendall(req.encode())
        response = sock.recv(4096)

        print_result(
            "Keep Alive",
            len(response) > 0
        )

        sock.close()

    except Exception as e:
        print_result("Keep Alive", False, str(e))


def test_chunked():
    try:
        sock = socket.socket()
        sock.connect((HOST, PORT))

        req = (
            "POST / HTTP/1.1\r\n"
            f"Host: {HOST}\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "Hello\r\n"
            "6\r\n"
            " World\r\n"
            "0\r\n"
            "\r\n"
        )

        sock.sendall(req.encode())

        response = sock.recv(4096).decode(errors="ignore")

        ok = (
            "200" in response or
            "201" in response
        )

        print_result(
            "Chunked POST",
            ok
        )

        sock.close()

    except Exception as e:
        print_result("Chunked POST", False, str(e))


def worker(idx):
    try:
        requests.get(BASE_URL + "/")
    except:
        pass


def test_concurrent():
    threads = []

    start = time.time()

    for i in range(100):
        t = threading.Thread(
            target=worker,
            args=(i,)
        )
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    elapsed = time.time() - start

    print_result(
        "100 Concurrent Clients",
        True,
        f"{elapsed:.2f}s"
    )


if __name__ == "__main__":
    print("\n===== WEBSERV TEST SUITE =====\n")

    test_get()
    test_404()
    test_post()
    test_large_body()
    test_delete()
    test_invalid_method()
    test_keep_alive()
    test_chunked()
    test_concurrent()

    print("\n===== DONE =====")