#!/usr/bin/env python3
import http.server, ssl, os, subprocess, sys, socketserver
from functools import partial

CERT_DIR = "cert"
CERT_FILE = os.path.abspath(f"{CERT_DIR}/cert.pem")
KEY_FILE = os.path.abspath(f"{CERT_DIR}/key.pem")

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

def ensure_cert():
    os.makedirs(CERT_DIR, exist_ok=True)
    if not (os.path.exists(CERT_FILE) and os.path.exists(KEY_FILE)):
        print("🔒 Generating self-signed certificate...")
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-keyout", KEY_FILE, "-out", CERT_FILE,
            "-days", "365", "-nodes",
            "-subj", "/CN=localhost"
        ], check=True)
    if not (os.path.exists(CERT_FILE) and os.path.exists(KEY_FILE)):
        sys.exit("❌ Failed to generate certificate.")

def run(port=8443):
    ensure_cert()
    os.chdir("build" if os.path.exists("build") else ".")
    handler = partial(Handler, directory=os.getcwd())

    with socketserver.ThreadingTCPServer(("0.0.0.0", port), handler) as httpd:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile=CERT_FILE, keyfile=KEY_FILE)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
        print(f"🚀 HTTPS server running on all interfaces at:")
        print(f"   → https://localhost:{port}/")
        print(f"   → https://127.0.0.1:{port}/")
        print(f"   → https://<your LAN IP>:{port}/  (e.g. from another device)")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n🛑 Server stopped.")
            sys.exit(0)

if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("-p", "--port", type=int, default=8443)
    args = p.parse_args()
    run(args.port)
