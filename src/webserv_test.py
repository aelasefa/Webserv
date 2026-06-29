#!/usr/bin/env python3
"""
42 Webserv — Complete Test Suite
Tests every requirement of the webserv subject plus security and robustness.

Usage:
    python3 webserv_tester.py [HOST] [PORT]
    python3 webserv_tester.py              # default: 127.0.0.1 8080

Output: colored PASS / FAIL / WARN / SKIP per test, score at the end.
"""

import socket
import threading
import time
import os
import sys
import struct
import hashlib
import http.client
import tempfile
from datetime import datetime

# ── Configuration ─────────────────────────────────────────────────────────────
HOST        = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT        = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
TIMEOUT     = 8          # seconds per HTTP request
CGI_TIMEOUT = 35         # seconds for the CGI-timeout test (must be > CGI_TIMEOUT in server)
MAX_BODY    = 5_000_000  # from Default.conf: client_max_body_size
WWW_ROOT    = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "www") \
              if os.path.isdir(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "www")) \
              else None

# ── ANSI colors ────────────────────────────────────────────────────────────────
GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
BLUE   = "\033[94m"
CYAN   = "\033[96m"
BOLD   = "\033[1m"
DIM    = "\033[2m"
RESET  = "\033[0m"

# ── Low-level helpers ──────────────────────────────────────────────────────────

def http_req(method, path, body=None, headers=None, follow_redirect=False, timeout=TIMEOUT):
    """
    Make an HTTP/1.1 request. Returns (status, headers_dict, body_bytes).
    Raises socket.timeout or OSError on network failure.
    """
    conn = http.client.HTTPConnection(HOST, PORT, timeout=timeout)
    h = {"Host": HOST, "Connection": "close"}
    if headers:
        h.update(headers)
    if body is not None:
        if isinstance(body, str):
            body = body.encode()
        if "Content-Length" not in h and "Transfer-Encoding" not in h:
            h["Content-Length"] = str(len(body))
    conn.request(method, path, body, h)
    resp = conn.getresponse()
    status = resp.status
    resp_hdrs = {}
    for k, v in resp.getheaders():
        resp_hdrs[k.lower()] = v
    body_bytes = resp.read()
    conn.close()
    if follow_redirect and status in (301, 302, 303, 307, 308):
        loc = resp_hdrs.get("location", "")
        if loc.startswith("/"):
            return http_req(method, loc, follow_redirect=False, timeout=timeout)
    return status, resp_hdrs, body_bytes


def raw_tcp(data, recv_size=8192, pause=0.3):
    """
    Send raw bytes over a TCP socket, collect whatever the server sends back.
    Useful for testing malformed requests, chunked encoding, etc.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    s.connect((HOST, PORT))
    if isinstance(data, str):
        data = data.encode()
    s.sendall(data)
    if pause:
        time.sleep(pause)
    buf = b""
    try:
        while True:
            chunk = s.recv(recv_size)
            if not chunk:
                break
            buf += chunk
    except socket.timeout:
        pass
    s.close()
    return buf


def server_alive(port=None):
    """Return True if the server is accepting connections on HOST:port."""
    port = port or PORT
    try:
        s = socket.socket()
        s.settimeout(2)
        s.connect((HOST, port))
        s.close()
        return True
    except Exception:
        return False


def response_time(path="/"):
    """Return the round-trip time in seconds for a GET request."""
    t0 = time.time()
    http_req("GET", path)
    return time.time() - t0


def multipart_body(fieldname, filename, content, ctype="text/plain"):
    """
    Build a multipart/form-data body.
    Returns (body_bytes, Content-Type header value).
    """
    boundary = "WebservTestBoundary42xXxX"
    if isinstance(content, str):
        content = content.encode()
    body = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="{fieldname}"; filename="{filename}"\r\n'
        f"Content-Type: {ctype}\r\n"
        f"\r\n"
    ).encode() + content + f"\r\n--{boundary}--\r\n".encode()
    return body, f"multipart/form-data; boundary={boundary}"


# ── Test runner ────────────────────────────────────────────────────────────────

class Suite:
    def __init__(self):
        self._pass = self._fail = self._warn = self._skip = 0
        self._log  = []

    def _print(self, icon, color, label, detail):
        line = f"  {color}{BOLD}{icon}{RESET}  {label}"
        if detail:
            line += f"  {DIM}({detail}){RESET}"
        print(line)

    def ok(self, label, detail=""):
        self._pass += 1
        self._log.append(("PASS", label))
        self._print("✓ PASS", GREEN, label, detail)

    def fail(self, label, detail=""):
        self._fail += 1
        self._log.append(("FAIL", label))
        self._print("✗ FAIL", RED, label, detail)

    def warn(self, label, detail=""):
        self._warn += 1
        self._log.append(("WARN", label))
        self._print("⚠ WARN", YELLOW, label, detail)

    def skip(self, label, detail=""):
        self._skip += 1
        self._log.append(("SKIP", label))
        self._print("⊙ SKIP", CYAN, label, detail)

    def check(self, label, passed, detail=""):
        """Convenience: call ok() or fail() based on a boolean."""
        if passed:
            self.ok(label, detail)
        else:
            self.fail(label, detail)

    def section(self, title):
        print(f"\n{BOLD}{BLUE}  ── {title} {'─'*(55-len(title))}{RESET}")

    def summary(self):
        total  = self._pass + self._fail
        pct    = int(100 * self._pass / max(total, 1))
        filled = pct // 5
        bar    = "█" * filled + "░" * (20 - filled)
        c      = GREEN if pct >= 80 else YELLOW if pct >= 60 else RED

        print(f"\n{BOLD}{'═'*62}{RESET}")
        print(f"{BOLD}  RESULTS{RESET}")
        print(f"  {GREEN}{BOLD}PASS{RESET}   {self._pass}")
        print(f"  {RED}{BOLD}FAIL{RESET}   {self._fail}")
        print(f"  {YELLOW}{BOLD}WARN{RESET}   {self._warn}  (not counted in score)")
        print(f"  {CYAN}{BOLD}SKIP{RESET}   {self._skip}  (not counted in score)")
        print(f"  {'─'*18}")
        print(f"  {BOLD}Score  {c}{pct}%{RESET}  {c}[{bar}]{RESET}  ({self._pass}/{total})")

        if self._fail == 0:
            print(f"\n  {GREEN}{BOLD}🎉  All tests passed — ready for 42 defense!{RESET}")
        elif self._fail <= 3:
            print(f"\n  {YELLOW}{BOLD}⚠   Close — fix the FAIL items above before defense.{RESET}")
        else:
            print(f"\n  {RED}{BOLD}✗   Several issues — review every FAIL before defense.{RESET}")

        if self._fail > 0:
            print(f"\n{BOLD}  Failed tests:{RESET}")
            for status, label in self._log:
                if status == "FAIL":
                    print(f"    {RED}•{RESET} {label}")
        print(f"{BOLD}{'═'*62}{RESET}\n")


# ── Test implementation ────────────────────────────────────────────────────────

def run(s: Suite):

    # ══════════════════════════════════════════════════════════
    s.section("0 · SERVER REACHABILITY")
    # ══════════════════════════════════════════════════════════

    if not server_alive():
        s.fail("Server reachable", f"Cannot connect to {HOST}:{PORT} — is webserv running?")
        print(f"\n{RED}{BOLD}  Cannot reach the server. Start it first, then re-run.{RESET}\n")
        s.summary()
        return
    s.ok("Server reachable", f"{HOST}:{PORT}")

    # ══════════════════════════════════════════════════════════
    s.section("1 · BASIC HTTP METHODS")
    # ══════════════════════════════════════════════════════════

    # GET existing
    try:
        st, _, body = http_req("GET", "/")
        s.check("GET / → 200", st == 200, f"status={st}")
    except Exception as e:
        s.fail("GET / → 200", str(e))

    # GET missing
    try:
        st, _, _ = http_req("GET", "/no_such_file_xyzzy42")
        s.check("GET missing file → 404", st == 404, f"status={st}")
    except Exception as e:
        s.fail("GET missing file → 404", str(e))

    # POST (multipart upload to /uploads/)
    uploaded_file = "ts_upload_%d.txt" % os.getpid()
    upload_content = b"42 webserv test upload content"
    try:
        body_bytes, ct = multipart_body("file", uploaded_file, upload_content)
        st, _, _ = http_req("POST", "/uploads/",
                             body=body_bytes, headers={"Content-Type": ct})
        upload_ok = st in (200, 201)
        s.check("POST /uploads/ (multipart) → 200/201", upload_ok, f"status={st}")
    except Exception as e:
        s.fail("POST /uploads/ (multipart)", str(e))
        upload_ok = False

    # DELETE the file we just uploaded
    if upload_ok:
        try:
            st, _, _ = http_req("DELETE", f"/uploads/{uploaded_file}")
            s.check("DELETE uploaded file → 200", st == 200, f"status={st}")
        except Exception as e:
            s.fail("DELETE uploaded file", str(e))
    else:
        s.skip("DELETE uploaded file", "upload step failed")

    # DELETE non-existent → 404
    try:
        st, _, _ = http_req("DELETE", "/uploads/this_file_does_not_exist_42.txt")
        s.check("DELETE non-existent → 404", st == 404, f"status={st}")
    except Exception as e:
        s.fail("DELETE non-existent → 404", str(e))

    # PUT — not a required method, must NOT be 200
    try:
        st, _, _ = http_req("PUT", "/")
        s.check("PUT / → NOT 200 (unsupported)", st not in (200, 201),
                f"status={st} (400/405/501 expected)")
    except Exception as e:
        s.ok("PUT / → connection error (also acceptable)", str(e))

    # HEAD — not required, must NOT be 200
    try:
        st, _, _ = http_req("HEAD", "/")
        s.check("HEAD / → NOT 200 (unsupported)", st not in (200,),
                f"status={st} (400/405/501 expected)")
    except Exception as e:
        s.ok("HEAD / → connection error (also acceptable)", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("2 · STATUS CODES")
    # ══════════════════════════════════════════════════════════

    # 200 OK
    try:
        st, _, _ = http_req("GET", "/")
        s.check("200 for existing resource", st == 200, f"status={st}")
    except Exception as e:
        s.fail("200 for existing resource", str(e))

    # 301 redirect (/about → /pages/about.html)
    try:
        st, hdrs, _ = http_req("GET", "/about")
        loc = hdrs.get("location", "")
        s.check("301 redirect has Location header",
                st == 301 and "about" in loc,
                f"status={st} Location={loc}")
    except Exception as e:
        s.fail("301 redirect", str(e))

    # 400 bad request
    try:
        resp = raw_tcp("THIS IS NOT HTTP\r\n\r\n")
        s.check("400 for invalid request line",
                b"400" in resp[:30], f"response: {resp[:30]}")
    except Exception as e:
        s.fail("400 for invalid request line", str(e))

    # 404 not found
    try:
        st, _, _ = http_req("GET", "/guaranteed_missing_42xyzzy")
        s.check("404 for missing resource", st == 404, f"status={st}")
    except Exception as e:
        s.fail("404 for missing resource", str(e))

    # 405 method not allowed (/assets only allows GET)
    try:
        st, _, _ = http_req("DELETE", "/assets/")
        s.check("405 for disallowed method on /assets",
                st == 405, f"status={st}")
    except Exception as e:
        s.fail("405 method not allowed", str(e))

    # 411 length required (POST without Content-Length and not chunked)
    try:
        resp = raw_tcp(
            "POST /uploads/ HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
        )
        s.check("411 Length Required for POST without Content-Length",
                b"411" in resp[:30], f"response: {resp[:30]}")
    except Exception as e:
        s.fail("411 Length Required", str(e))

    # 413 Payload Too Large
    try:
        big = b"X" * (MAX_BODY + 1)
        body_b, ct = multipart_body("file", "big.bin", big)
        st, _, _ = http_req("POST", "/uploads/",
                             body=body_b, headers={"Content-Type": ct})
        s.check("413 Payload Too Large", st == 413, f"status={st}")
    except Exception as e:
        s.fail("413 Payload Too Large", str(e))

    # 500 from CGI failure
    if WWW_ROOT:
        cgi_fail = os.path.join(WWW_ROOT, "cgi-bin", "_ts_fail.py")
        try:
            with open(cgi_fail, "w") as f:
                f.write("#!/usr/bin/python3\nimport sys\n"
                        "print('Content-Type: text/plain\\r\\n\\r\\n')\n"
                        "sys.exit(1)\n")
            os.chmod(cgi_fail, 0o755)
            st, _, _ = http_req("GET", "/cgi-bin/_ts_fail.py")
            s.check("500 from failing CGI (not server crash)", st == 500, f"status={st}")
        except Exception as e:
            s.fail("500 from failing CGI", str(e))
        finally:
            try:
                os.remove(cgi_fail)
            except Exception:
                pass
    else:
        s.skip("500 from failing CGI", "www root not found — run from repo dir")

    # ══════════════════════════════════════════════════════════
    s.section("3 · HTTP/1.1 COMPLIANCE")
    # ══════════════════════════════════════════════════════════

    # Status line starts with HTTP/1.1
    try:
        resp = raw_tcp("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        s.check("Response status line is HTTP/1.1",
                resp.startswith(b"HTTP/1.1"),
                f"starts with: {resp[:15]}")
    except Exception as e:
        s.fail("Response status line is HTTP/1.1", str(e))

    # Content-Length must be present on 200 responses
    try:
        st, hdrs, body = http_req("GET", "/")
        has_cl = "content-length" in hdrs
        if has_cl:
            cl = int(hdrs["content-length"])
            correct = cl == len(body)
            s.check("Content-Length header present and accurate",
                    has_cl and correct, f"header={cl} actual={len(body)}")
        else:
            s.fail("Content-Length header present and accurate",
                   f"missing; headers: {list(hdrs.keys())}")
    except Exception as e:
        s.fail("Content-Length header", str(e))

    # Missing Host header → 400
    try:
        resp = raw_tcp("GET / HTTP/1.1\r\n\r\n")
        s.check("Missing Host header → 400",
                b"400" in resp[:30], f"response: {resp[:30]}")
    except Exception as e:
        s.fail("Missing Host header → 400", str(e))

    # HTTP/1.0 still gets a response
    try:
        resp = raw_tcp("GET / HTTP/1.0\r\n\r\n")
        s.check("HTTP/1.0 request gets a response",
                b"HTTP/" in resp[:10], f"response: {resp[:30]}")
    except Exception as e:
        s.fail("HTTP/1.0 request gets a response", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("4 · PERSISTENT CONNECTIONS (KEEP-ALIVE)")
    # ══════════════════════════════════════════════════════════

    # Three sequential requests on one TCP connection
    try:
        sock = socket.socket()
        sock.settimeout(TIMEOUT)
        sock.connect((HOST, PORT))
        statuses = []
        for i in range(3):
            req = b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
            sock.sendall(req)
            # Read until we have complete headers
            buf = b""
            while b"\r\n\r\n" not in buf:
                buf += sock.recv(4096)
            hdr_block = buf.split(b"\r\n\r\n")[0]
            body_start = buf[len(hdr_block)+4:]
            # Parse Content-Length
            cl = 0
            for line in hdr_block.split(b"\r\n"):
                if line.lower().startswith(b"content-length:"):
                    cl = int(line.split(b":")[1].strip())
            # Read the full body
            while len(body_start) < cl:
                body_start += sock.recv(4096)
            status_word = hdr_block.split(b"\r\n")[0]
            statuses.append(b"200" in status_word)
        sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        sock.recv(4096)
        sock.close()
        s.check("3 requests on one connection all return 200",
                all(statuses), f"statuses: {statuses}")
    except Exception as e:
        s.fail("Keep-alive: 3 requests on one connection", str(e))

    # Connection: close actually closes the connection after the response
    try:
        sock = socket.socket()
        sock.settimeout(TIMEOUT)
        sock.connect((HOST, PORT))
        sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        buf = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
        sock.close()
        s.check("Connection: close → server closes after response",
                b"200" in buf[:30], f"got {len(buf)} bytes total")
    except Exception as e:
        s.fail("Connection: close behavior", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("5 · CHUNKED TRANSFER ENCODING")
    # ══════════════════════════════════════════════════════════

    # Server accepts chunked POST body
    try:
        payload = b"chunked_transfer_test_42"
        chunk_hex = hex(len(payload))[2:]
        raw = (
            "POST /uploads/ HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Connection: close\r\n"
            "\r\n"
        ).encode()
        raw += chunk_hex.encode() + b"\r\n" + payload + b"\r\n"
        raw += b"0\r\n\r\n"
        resp = raw_tcp(raw)
        status_line = resp.split(b"\r\n")[0]
        s.check("Chunked POST accepted (200/201)",
                b"200" in status_line or b"201" in status_line,
                f"status: {status_line.decode(errors='replace')}")
    except Exception as e:
        s.fail("Chunked POST body", str(e))

    # Content-Length + Transfer-Encoding together → 400 (anti-smuggling)
    try:
        resp = raw_tcp(
            "POST /uploads/ HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 5\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: close\r\n"
            "\r\n"
            "5\r\nhello\r\n0\r\n\r\n"
        )
        s.check("CL + TE: chunked together → 400 (request-smuggling protection)",
                b"400" in resp[:30], f"response: {resp[:30]}")
    except Exception as e:
        s.fail("CL + TE smuggling protection", str(e))

    # Duplicate Content-Length header → 400
    try:
        resp = raw_tcp(
            "POST /uploads/ HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 5\r\n"
            "Content-Length: 10\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello"
        )
        s.check("Duplicate Content-Length header → 400",
                b"400" in resp[:30], f"response: {resp[:30]}")
    except Exception as e:
        s.fail("Duplicate Content-Length → 400", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("6 · STATIC FILE SERVING")
    # ══════════════════════════════════════════════════════════

    # index.html served as HTML
    try:
        st, hdrs, body = http_req("GET", "/")
        ct = hdrs.get("content-type", "")
        is_html = b"<html" in body.lower() or b"<!doctype" in body.lower()
        s.check("GET / serves HTML content", st == 200 and is_html,
                f"status={st} Content-Type={ct}")
    except Exception as e:
        s.fail("GET / serves HTML", str(e))

    # Correct MIME type
    try:
        st, hdrs, _ = http_req("GET", "/")
        ct = hdrs.get("content-type", "")
        s.check("Content-Type: text/html for .html files", "text/html" in ct,
                f"Content-Type={ct}")
    except Exception as e:
        s.fail("MIME type text/html", str(e))

    # Large file served correctly and completely (streaming test)
    if WWW_ROOT:
        large_path = os.path.join(WWW_ROOT, "ts_large_file.bin")
        try:
            size = 8 * 1024 * 1024   # 8 MB
            data = bytes(range(256)) * (size // 256)
            expected_md5 = hashlib.md5(data).hexdigest()
            with open(large_path, "wb") as f:
                f.write(data)
            st, hdrs, body = http_req("GET", "/ts_large_file.bin",
                                       timeout=60)
            got_md5 = hashlib.md5(body).hexdigest()
            s.check("8 MB file served complete and intact (streaming)",
                    st == 200 and got_md5 == expected_md5,
                    f"status={st} size={len(body)} checksum={'OK' if got_md5==expected_md5 else 'MISMATCH'}")
        except Exception as e:
            s.fail("Large file served intact", str(e))
        finally:
            try:
                os.remove(large_path)
            except Exception:
                pass
    else:
        s.skip("Large file streaming test", "www root not found")

    # Custom 404 error page
    try:
        st, _, body = http_req("GET", "/xyzzy_definitelymissing_404")
        has_custom = len(body) > 20
        s.check("Custom 404 error page served",
                st == 404 and has_custom, f"status={st} body_len={len(body)}")
    except Exception as e:
        s.fail("Custom 404 error page", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("7 · DIRECTORY LISTING (AUTOINDEX)")
    # ══════════════════════════════════════════════════════════

    # Autoindex ON for /uploads/
    os.makedirs(os.path.join(WWW_ROOT, "uploads"), exist_ok=True) if WWW_ROOT else None
    marker_file = None
    if WWW_ROOT:
        marker_file = os.path.join(WWW_ROOT, "uploads", "ts_autoindex_marker.txt")
        try:
            with open(marker_file, "w") as f:
                f.write("marker")
        except Exception:
            marker_file = None

    try:
        st, _, body = http_req("GET", "/uploads/")
        has_listing = b"ts_autoindex_marker" in body or b"Index of" in body
        s.check("Autoindex ON: /uploads/ returns directory listing",
                st == 200 and has_listing, f"status={st} listing={has_listing}")
    except Exception as e:
        s.fail("Autoindex /uploads/ listing", str(e))

    # XSS protection in filenames
    if WWW_ROOT:
        xss_filename = "<img src=x onerror=alert(1)>.txt"
        xss_path = os.path.join(WWW_ROOT, "uploads", xss_filename)
        try:
            with open(xss_path, "w") as f:
                f.write("xss")
            st, _, body = http_req("GET", "/uploads/")
            raw_tag = b"<img src=x onerror" in body
            s.check("Autoindex escapes HTML in filenames (no stored XSS)",
                    not raw_tag,
                    f"raw tag in body={raw_tag}"
                    + (" (ESCAPED OK)" if b"&lt;img" in body else ""))
        except Exception as e:
            s.fail("Autoindex XSS protection", str(e))
        finally:
            try:
                os.remove(xss_path)
            except Exception:
                pass
    else:
        s.skip("Autoindex XSS protection", "www root not found")

    # Autoindex OFF for / — must not show a listing
    try:
        st, _, body = http_req("GET", "/")
        shows_listing = b"Index of" in body
        s.check("Autoindex OFF on / (no directory listing leaked)",
                not shows_listing, f"shows_listing={shows_listing}")
    except Exception as e:
        s.fail("Autoindex OFF on /", str(e))

    # Cleanup marker
    if marker_file:
        try:
            os.remove(marker_file)
        except Exception:
            pass

    # ══════════════════════════════════════════════════════════
    s.section("8 · REDIRECTIONS")
    # ══════════════════════════════════════════════════════════

    redirects = [
        ("/about",    "/pages/about.html"),
        ("/home",     "/"),
        ("/services", "/pages/services.html"),
        ("/contact",  "/pages/contact.html"),
    ]
    for path, expected_dest in redirects:
        try:
            st, hdrs, _ = http_req("GET", path)
            loc = hdrs.get("location", "")
            s.check(f"GET {path} → 3xx redirect to {expected_dest}",
                    st in (301, 302) and expected_dest in loc,
                    f"status={st} Location={loc}")
        except Exception as e:
            s.fail(f"Redirect {path} → {expected_dest}", str(e))

    # Following a redirect reaches destination (200)
    try:
        st, _, body = http_req("GET", "/about", follow_redirect=True)
        s.check("Following /about redirect reaches 200 page",
                st == 200, f"final status={st}")
    except Exception as e:
        s.fail("Following redirect", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("9 · FILE UPLOAD")
    # ══════════════════════════════════════════════════════════

    # Multipart upload — content survives round-trip
    rt_file = "ts_roundtrip_%d.txt" % os.getpid()
    rt_content = b"round-trip content %d" % os.getpid()
    try:
        body_b, ct = multipart_body("file", rt_file, rt_content)
        st, _, _ = http_req("POST", "/uploads/",
                             body=body_b, headers={"Content-Type": ct})
        rt_upload_ok = st in (200, 201)
        s.check("Multipart upload → 200/201", rt_upload_ok, f"status={st}")
        if rt_upload_ok:
            time.sleep(0.1)
            st2, _, body2 = http_req("GET", f"/uploads/{rt_file}")
            s.check("Uploaded file retrievable and byte-identical",
                    st2 == 200 and body2 == rt_content,
                    f"status={st2} match={body2 == rt_content}")
            http_req("DELETE", f"/uploads/{rt_file}")
        else:
            s.skip("Uploaded file retrievable", "upload step failed")
    except Exception as e:
        s.fail("Multipart upload round-trip", str(e))

    # Oversized upload → 413
    try:
        big = b"Z" * (MAX_BODY + 1024)
        body_b, ct = multipart_body("file", "ts_toobig.bin", big)
        st, _, _ = http_req("POST", "/uploads/",
                             body=body_b, headers={"Content-Type": ct})
        s.check("Upload > client_max_body_size → 413", st == 413, f"status={st}")
    except Exception as e:
        s.fail("413 for oversized upload", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("10 · CGI")
    # ══════════════════════════════════════════════════════════

    # Basic CGI GET (echo.py)
    try:
        st, hdrs, body = http_req("GET", "/cgi-bin/echo.py")
        s.check("CGI GET /cgi-bin/echo.py → 200", st == 200,
                f"status={st} body_len={len(body)}")
    except Exception as e:
        s.fail("CGI GET echo.py", str(e))

    # CGI sets Content-Type header in output
    try:
        st, hdrs, body = http_req("GET", "/cgi-bin/echo.py")
        ct = hdrs.get("content-type", "")
        s.check("CGI output has Content-Type", bool(ct),
                f"Content-Type={ct}")
    except Exception as e:
        s.fail("CGI Content-Type", str(e))

    # CGI receives QUERY_STRING
    try:
        st, _, body = http_req("GET", "/cgi-bin/echo.py?foo=bar&baz=42")
        has_qs = b"foo=bar" in body or b"QUERY_STRING" in body or b"foo" in body
        s.check("CGI receives QUERY_STRING correctly", st == 200 and has_qs,
                f"status={st} found_qs={has_qs}")
    except Exception as e:
        s.fail("CGI QUERY_STRING", str(e))

    # CGI POST: body sent to script via stdin
    try:
        payload = b"cgi_key=hello42&other=world"
        st, _, body = http_req("POST", "/cgi-bin/echo.py",
                                body=payload,
                                headers={"Content-Type": "application/x-www-form-urlencoded"})
        s.check("CGI POST body passed to script", st == 200,
                f"status={st}")
    except Exception as e:
        s.fail("CGI POST body", str(e))

    # CGI failure → 500, NOT a server crash
    if WWW_ROOT:
        fail_script = os.path.join(WWW_ROOT, "cgi-bin", "_ts_crash.py")
        try:
            with open(fail_script, "w") as f:
                f.write("#!/usr/bin/python3\nimport sys\n"
                        "print('Content-Type: text/plain\\r\\n\\r\\n')\n"
                        "sys.exit(1)\n")
            os.chmod(fail_script, 0o755)
            st, _, _ = http_req("GET", "/cgi-bin/_ts_crash.py")
            s.check("CGI non-zero exit → 500 (not server crash)", st == 500,
                    f"status={st}")
            time.sleep(0.3)
            st2, _, _ = http_req("GET", "/")
            s.check("Server still alive after CGI failure", st2 == 200,
                    f"follow-up GET / status={st2}")
        except Exception as e:
            s.fail("CGI failure resilience", str(e))
        finally:
            try:
                os.remove(fail_script)
            except Exception:
                pass
    else:
        s.skip("CGI failure → 500 (not crash)", "www root not found")

    # CGI timeout → server survives
    if WWW_ROOT:
        sleep_script = os.path.join(WWW_ROOT, "cgi-bin", "_ts_sleep.py")
        try:
            with open(sleep_script, "w") as f:
                f.write("#!/usr/bin/python3\nimport time\ntime.sleep(120)\n"
                        "print('Content-Type: text/plain\\r\\n\\r\\nok')\n")
            os.chmod(sleep_script, 0o755)
            print(f"    {DIM}(CGI timeout test: waiting up to {CGI_TIMEOUT}s — this is normal){RESET}")
            st, _, _ = http_req("GET", "/cgi-bin/_ts_sleep.py", timeout=CGI_TIMEOUT)
            s.check("CGI timeout returns error status (500/502/504)",
                    st in (500, 502, 504), f"status={st}")
            time.sleep(0.3)
            st2, _, _ = http_req("GET", "/")
            s.check("Server alive after CGI timeout", st2 == 200,
                    f"status={st2}")
        except Exception as e:
            still = server_alive()
            s.check("Server alive after CGI timeout (request timed out)",
                    still, f"server_alive={still} err={e}")
        finally:
            try:
                os.remove(sleep_script)
            except Exception:
                pass
    else:
        s.skip("CGI timeout resilience", "www root not found")

    # ══════════════════════════════════════════════════════════
    s.section("11 · COOKIES & SESSIONS")
    # ══════════════════════════════════════════════════════════

    # CGI sets Set-Cookie
    try:
        st, hdrs, body = http_req("GET", "/cgi-bin/session.py")
        has_cookie = "set-cookie" in hdrs
        s.check("Session CGI sets Set-Cookie header",
                st == 200 and has_cookie,
                f"status={st} Set-Cookie={hdrs.get('set-cookie', 'MISSING')}")
    except Exception as e:
        s.fail("Session CGI sets cookie", str(e))

    # Cookie round-trip: server recognizes what it set
    try:
        st, hdrs, _ = http_req("GET", "/cgi-bin/session.py")
        cookie_hdr = hdrs.get("set-cookie", "")
        if cookie_hdr:
            cookie_val = cookie_hdr.split(";")[0]
            st2, _, body2 = http_req("GET", "/cgi-bin/session.py",
                                      headers={"Cookie": cookie_val})
            s.check("Server recognizes cookie sent back by client",
                    st2 == 200, f"status={st2}")
        else:
            s.skip("Cookie round-trip", "no Set-Cookie header to round-trip")
    except Exception as e:
        s.fail("Cookie round-trip", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("12 · SECURITY")
    # ══════════════════════════════════════════════════════════

    # Path traversal GET → must NOT leak /etc/passwd
    try:
        resp = raw_tcp(
            "GET /../../../../../../../../etc/passwd HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n\r\n",
            pause=0.5
        )
        leaked = b"root:x:" in resp or b"/bin/" in resp
        blocked = b" 400 " in resp[:30] or b" 403 " in resp[:30] or b" 404 " in resp[:30]
        s.check("Path traversal GET blocked (no /etc/passwd leakage)",
                not leaked, f"leaked={leaked} blocked={blocked}")
    except Exception as e:
        s.fail("Path traversal GET", str(e))

    # Path traversal DELETE → must NOT delete files outside the web root
    tmp_file = f"/tmp/webserv_traversal_{os.getpid()}.txt"
    try:
        with open(tmp_file, "w") as f:
            f.write("must not be deleted by webserv")
        esc = "../" * 12 + "tmp/" + os.path.basename(tmp_file)
        raw_tcp(
            f"DELETE /{esc} HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n\r\n",
            pause=0.5
        )
        still_exists = os.path.exists(tmp_file)
        s.check("Path traversal DELETE blocked (file outside root not deleted)",
                still_exists, f"file_still_exists={still_exists}")
    except Exception as e:
        s.fail("Path traversal DELETE", str(e))
    finally:
        try:
            os.remove(tmp_file)
        except Exception:
            pass

    # Very long URL (potential buffer overflow / DoS)
    try:
        long_url = "/a" * 4096
        resp = raw_tcp(f"GET {long_url} HTTP/1.1\r\nHost: localhost\r\n\r\n")
        alive = server_alive()
        s.check("Very long URL handled (server not crashed)",
                alive, f"alive={alive} response_len={len(resp)}")
    except Exception as e:
        s.check("Very long URL handled", server_alive(), f"err={e}")

    # Very large header value (potential DoS)
    try:
        long_val = "X-Flood: " + "A" * 16384 + "\r\n"
        resp = raw_tcp(
            "GET / HTTP/1.1\r\nHost: localhost\r\n" +
            long_val +
            "Connection: close\r\n\r\n"
        )
        alive = server_alive()
        s.check("Very large header handled (server not crashed)",
                alive, f"alive={alive} response_len={len(resp)}")
    except Exception as e:
        s.check("Very large header handled", server_alive(), f"err={e}")

    # NULL byte in path
    try:
        resp = raw_tcp(b"GET /\x00etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n")
        leaked = b"root:x:" in resp
        s.check("NULL byte in path does not leak files",
                not leaked, f"leaked={leaked}")
    except Exception as e:
        s.ok("NULL byte in path (connection error — acceptable)", str(e))

    # Upload path traversal in filename → file must not appear outside upload dir
    if WWW_ROOT:
        sentinel = f"/tmp/webserv_upload_traversal_{os.getpid()}.txt"
        try:
            trav_name = "../../../tmp/" + os.path.basename(sentinel)
            body_b, ct = multipart_body("file", trav_name, b"traversal probe")
            http_req("POST", "/uploads/",
                     body=body_b, headers={"Content-Type": ct})
            time.sleep(0.1)
            escaped = os.path.exists(sentinel)
            s.check("Upload filename traversal blocked (no file outside upload dir)",
                    not escaped, f"file_at_target={escaped}")
        except Exception as e:
            s.fail("Upload filename traversal", str(e))
        finally:
            try:
                os.remove(sentinel)
            except Exception:
                pass
    else:
        s.skip("Upload filename traversal check", "www root not found")

    # ══════════════════════════════════════════════════════════
    s.section("13 · MALFORMED REQUESTS")
    # ══════════════════════════════════════════════════════════

    bad_reqs = [
        ("Completely empty",           b"\r\n\r\n"),
        ("Only a newline",             b"\n"),
        ("Missing HTTP version",       b"GET /\r\nHost: localhost\r\n\r\n"),
        ("No path in request line",    b"GET HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        ("Method with spaces",         b"G E T / HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        ("Unknown method",             b"FOOBAR / HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        ("HTTP/2.0 version",           b"GET / HTTP/2.0\r\nHost: localhost\r\n\r\n"),
        ("Negative Content-Length",    b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n"),
    ]
    for label, req in bad_reqs:
        try:
            resp = raw_tcp(req)
            alive = server_alive()
            # We don't mandate a specific error code — just that the server survives
            s.check(f"Malformed '{label}' — server survives",
                    alive, f"alive={alive} resp_len={len(resp)}")
        except Exception as e:
            alive = server_alive()
            s.check(f"Malformed '{label}' — server survives",
                    alive, f"alive={alive} err={e}")

    # ══════════════════════════════════════════════════════════
    s.section("14 · CONCURRENCY & ROBUSTNESS")
    # ══════════════════════════════════════════════════════════

    # 30 simultaneous GET requests
    results_lock = threading.Lock()
    concurrent_results = []

    def _one_get():
        try:
            st, _, _ = http_req("GET", "/")
            with results_lock:
                concurrent_results.append(st == 200)
        except Exception:
            with results_lock:
                concurrent_results.append(False)

    N_CONCURRENT = 30
    try:
        threads = [threading.Thread(target=_one_get) for _ in range(N_CONCURRENT)]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=15)
        ok = sum(concurrent_results)
        s.check(f"{N_CONCURRENT} simultaneous GETs all return 200",
                ok == N_CONCURRENT, f"{ok}/{N_CONCURRENT} succeeded")
    except Exception as e:
        s.fail(f"{N_CONCURRENT} simultaneous GETs", str(e))

    # 10 half-open connections (connect but send nothing) don't block the server
    idle_socks = []
    try:
        for _ in range(10):
            try:
                sock = socket.socket()
                sock.settimeout(1)
                sock.connect((HOST, PORT))
                idle_socks.append(sock)
            except Exception:
                pass
        time.sleep(0.3)
        st, _, _ = http_req("GET", "/")
        s.check("Server handles 10 idle connections (not blocked)",
                st == 200, f"GET / while 10 idle sockets open: status={st}")
    except Exception as e:
        s.fail("Idle connections don't block", str(e))
    finally:
        for sock in idle_socks:
            try:
                sock.close()
            except Exception:
                pass

    # 20 abrupt RST disconnects — server must not hang or crash
    try:
        for _ in range(20):
            sock = socket.socket()
            sock.settimeout(1)
            try:
                sock.connect((HOST, PORT))
                sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                struct.pack("ii", 1, 0))
                sock.close()
            except Exception:
                pass
        time.sleep(0.8)
        st, _, _ = http_req("GET", "/")
        s.check("Server survives 20 abrupt RST disconnects",
                st == 200, f"follow-up GET / status={st}")
    except Exception as e:
        s.fail("Abrupt RST disconnects", str(e))

    # Server not spinning (response time reasonable after all the above)
    try:
        t0 = time.time()
        st, _, _ = http_req("GET", "/")
        dt = time.time() - t0
        s.check("Response time < 500ms (no CPU spin / hang)",
                dt < 0.5, f"{dt*1000:.0f}ms")
    except Exception as e:
        s.fail("Response time check", str(e))

    # 50 rapid fire sequential requests
    try:
        ok = sum(1 for _ in range(50)
                 if http_req("GET", "/")[0] == 200)
        s.check("50 rapid sequential requests all return 200",
                ok == 50, f"{ok}/50 succeeded")
    except Exception as e:
        s.fail("50 rapid sequential requests", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("15 · MULTIPLE SERVERS & VIRTUAL HOSTING")
    # ══════════════════════════════════════════════════════════

    # Port 8082 (www_test.conf) — optional
    if server_alive(8082):
        try:
            st, _, _ = http_req("GET", "/")  # reuse helper for simplicity
            conn = http.client.HTTPConnection(HOST, 8082, timeout=TIMEOUT)
            conn.request("GET", "/", headers={"Host": HOST})
            resp = conn.getresponse()
            resp.read()
            s.check("Second server on port 8082 responds",
                    resp.status == 200, f"status={resp.status}")
            conn.close()
        except Exception as e:
            s.fail("Second server port 8082", str(e))
    else:
        s.warn("Multiple servers (port 8082 not running)",
               "Start webserv with www_test.conf too to test multi-server setup")

    # Host-header based routing (correct host)
    try:
        st, _, _ = http_req("GET", "/", headers={"Host": "localhost"})
        s.check("Correct Host header → 200", st == 200, f"status={st}")
    except Exception as e:
        s.fail("Host header routing", str(e))

    # Unknown host falls back gracefully (doesn't crash)
    try:
        st, _, _ = http_req("GET", "/", headers={"Host": "unknown.invalid"})
        s.check("Unknown Host header — server responds (fallback)",
                st in (200, 400, 404), f"status={st}")
    except Exception as e:
        s.ok("Unknown Host header (connection error — also acceptable)", str(e))

    # ══════════════════════════════════════════════════════════
    s.section("16 · FINAL HEALTH CHECK")
    # ══════════════════════════════════════════════════════════

    try:
        st, _, _ = http_req("GET", "/")
        s.check("Server still alive after all 100+ tests", st == 200,
                f"GET / → {st}")
    except Exception as e:
        s.fail("Final health check", str(e))

    try:
        t0 = time.time()
        http_req("GET", "/")
        dt = time.time() - t0
        s.check("Final response time < 200ms", dt < 0.2,
                f"{dt*1000:.0f}ms")
    except Exception as e:
        s.fail("Final response time", str(e))


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print(f"\n{BOLD}{'═'*62}{RESET}")
    print(f"{BOLD}  42 Webserv — Complete Test Suite{RESET}")
    print(f"{BOLD}  Target  : {HOST}:{PORT}{RESET}")
    print(f"{BOLD}  WWW root: {WWW_ROOT or 'not found — some tests will be skipped'}{RESET}")
    print(f"{BOLD}  Time    : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}{RESET}")
    print(f"{BOLD}{'═'*62}{RESET}")

    suite = Suite()
    try:
        run(suite)
    except KeyboardInterrupt:
        print(f"\n{YELLOW}{BOLD}  Interrupted.{RESET}")
    suite.summary()