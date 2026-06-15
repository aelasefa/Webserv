import requests
import time

BASE_URL = "http://localhost:8080/cgi-bin"

def print_result(name, condition):
    if condition:
        print(f"✅ {name} - PASS")
    else:
        print(f"❌ {name} - FAIL")

def test_cgi():
    print("Starting WebServ CGI Tests...\n")

    # 1. Test GET and Query String
    try:
        r = requests.get(f"{BASE_URL}/env_test.py?foo=bar&baz=qux")
        print_result("GET Query String", "QUERY_STRING: foo=bar&baz=qux" in r.text)
        print_result("GET Request Method", "REQUEST_METHOD: GET" in r.text)
    except Exception as e:
        print(f"❌ GET Test Failed: {e}")

    # 2. Test Custom Headers
    try:
        headers = {"X-Secret-Header": "SuperSecret123"}
        r = requests.get(f"{BASE_URL}/env_test.py", headers=headers)
        # WebServ should prepend HTTP_ and replace dashes with underscores
        print_result("Custom HTTP Headers", "HTTP_X_SECRET_HEADER: SuperSecret123" in r.text)
    except Exception as e:
        print(f"❌ Header Test Failed: {e}")

    # 3. Test POST Standard Body
    try:
        payload = "This is a simple POST body."
        r = requests.post(f"{BASE_URL}/post_test.py", data=payload)
        print_result("POST Basic Body", payload in r.text)
    except Exception as e:
        print(f"❌ POST Basic Failed: {e}")

    # 4. Test POST Large Body (Chunking/Buffer limits)
    try:
        payload = "A" * 1000000  # 1 MB payload
        r = requests.post(f"{BASE_URL}/post_test.py", data=payload)
        print_result("POST Large Body (1MB)", f"Received 1000000 bytes." in r.text)
    except Exception as e:
        print(f"❌ POST Large Body Failed: {e}")

    # 5. Test Timeout / Infinite Loop
    try:
        start_time = time.time()
        r = requests.get(f"{BASE_URL}/timeout_test.py", timeout=10)
        elapsed = time.time() - start_time
        # Server should kill it before 10 seconds and return an error code (500 or 504)
        print_result("CGI Timeout Handling", r.status_code >= 500 and elapsed < 10)
    except requests.exceptions.ReadTimeout:
        print("❌ CGI Timeout Handling - FAIL (Server hung and didn't close connection)")

    # 6. Test Crash Handling
    try:
        r = requests.get(f"{BASE_URL}/crash_test.py")
        print_result("CGI Crash Handling", r.status_code == 500 or r.status_code == 502)
    except Exception as e:
        print(f"❌ Crash Handling Failed: {e}")

if __name__ == "__main__":
    test_cgi()