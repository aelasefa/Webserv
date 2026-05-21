PROJECT STATUS

Architecture summary: poll loop + per-client Request parser; router with CGI/static/upload; config-driven servers/locations. Main flow: Webserv -> Client -> Request -> Router -> Response.
Current readiness: ~65% (major compliance gaps remain).

WORKING FEATURES

Makefile: correct rules and flags. See Makefile:1-24.
Config parsing (servers/locations, methods, redirects, cgi, uploads, limits): functional. See ConfigParser.cpp:200-299.
Non-blocking sockets + single poll loop: sockets set non-blocking, poll loop drives IO. See Webserv.cpp:182-360.
Request parser with chunked + Host validation: incremental, includes 411, 400, 413, chunked decoding. See Request.cpp:230-380.
GET/POST/DELETE routing + uploads + autoindex: functional path. See routing.cpp:371-479.
Safer uploads: temp file + rename, overwrite rejected. See FileHandler.cpp:98-132.

BROKEN FEATURES

Custom error_page not served: parsed, but router always returns built-in HTML. See ConfigParser.cpp:284-296 and routing.cpp:159-167.
HTTP version in response is always HTTP/1.1: breaks HTTP/1.0 compliance. See Response.cpp:115-137.
Missing Allow header for 405 in router path. See routing.cpp:412-413.
Location root semantics: buildPath does not strip location prefix, causing double paths if root is set like nginx. See routing.cpp:248-259.

MISSING SUBJECT REQUIREMENTS

README.md with required sections/AI usage: missing.
Default error pages from config: not used (mandatory).
CGI environment completeness: minimal env only; missing common CGI variables.

CRITICAL EVALUATION FAILURES

Blocking CGI pipes + waitpid in main loop: violates non-blocking requirement and no blocking I/O for pipes. See CGI.cpp:41-82.
Error pages: config not honored, can fail subject checks. See routing.cpp:159-167.

HTTP COMPLIANCE ISSUES

Response version hardcoded to HTTP/1.1. See Response.cpp:115-137.
Missing Allow header on 405 in router. See routing.cpp:412-413.
Connection header inconsistencies (router does not always set one). See routing.cpp:159-191.

SECURITY ISSUES

Path traversal risk in router: buildPath uses raw request path; no normalization. See routing.cpp:248-259.
Autoindex HTML not escaped: directory listing injection risk. See routing.cpp:347-365.
CGI env incomplete: some scripts rely on REQUEST_URI, SERVER_NAME, SERVER_PORT, REMOTE_ADDR. See CGI.cpp:24-35.

NON-BLOCKING AUDIT

OK: sockets set non-blocking; poll loop drives IO. See Webserv.cpp:182-360.
Fail: CGI uses blocking write/read/waitpid on pipes. See CGI.cpp:41-82.

CGI AUDIT

Env vars: minimal only. See CGI.cpp:24-35.
Body forwarding: ok (unchunked body passed).
Chunked support for CGI: ok because parser unchunks before CGI. See Request.cpp:312-317.
Timeout handling: none; CGI can hang. See CGI.cpp:41-82.
Process cleanup: uses waitpid, but blocks.

CONFIG AUDIT

root/alias/methods/autoindex/redirects: parsed. See ConfigParser.cpp:200-299.
error_page: parsed only, not enforced. See ConfigParser.cpp:284-296.
upload config: parsed and used. See routing.cpp:418-479.

FINAL ROADMAP
P0 (must fix now)

Non-blocking CGI (pipes + poll + timeout; no blocking wait). Owner: CGI teammate. Difficulty: hard. Impact: instant fail. See CGI.cpp:41-82.
Serve error_page from config. Owner: routing teammate. Difficulty: medium. Impact: mandatory fail. See routing.cpp:159-167.

P1 (important)

Return correct HTTP version based on request. Owner: HTTP teammate. Difficulty: medium. Impact: compliance tests. See Response.cpp:115-137.
Add Allow header for 405 in router. Owner: HTTP teammate. Difficulty: low. Impact: compliance tests. See routing.cpp:412-413.
Normalize router paths / prevent traversal. Owner: routing teammate. Difficulty: medium. Impact: security checks. See routing.cpp:248-259.

P2 (cleanup)

CGI env completeness. Owner: CGI teammate. Difficulty: medium. Impact: compatibility. See CGI.cpp:24-35.
Autoindex escaping. Owner: routing teammate. Difficulty: low. Impact: minor security. See routing.cpp:347-365.
README.md with required sections. Owner: docs teammate. Difficulty: low. Impact: mandatory doc requirement.

FINAL VERDICT
Fail (for a strict evaluator) due to blocking CGI IO, missing error_page handling, and HTTP response version mismatch. Fixing P0 will move you into partial pass; P1 items are likely needed to fully pass.
