#!/usr/bin/env python3
import argparse
import socket
import sys
import time

def send_cmd(sock, cmd, timeout=2.0):
    sock.settimeout(timeout)
    data = (cmd + "\r\n").encode()
    sock.sendall(data)
    chunks = []
    while True:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        chunks.append(chunk)
        if b"\r\n" in chunk:
            break
    return b"".join(chunks).decode(errors="ignore")

def main():
    parser = argparse.ArgumentParser(description="KVS+Reactor integration test client")
    parser.add_argument("host", help="server host")
    parser.add_argument("port", type=int, help="server port")
    parser.add_argument("command", nargs='*', help="command to send, e.g. SET k v")
    parser.add_argument("--batch", action="store_true", help="run assertion-based batch tests")
    parser.add_argument("--prefix", default=None, help="key namespace prefix to avoid collisions")
    args = parser.parse_args()

    addr = (args.host, args.port)

    ns = args.prefix or ("ns" + str(int(time.time())))
    print(f"namespace: {ns}")

    try:
        with socket.create_connection(addr, timeout=3.0) as sock:
            if args.command:
                cmd = " ".join(args.command)
                print(f"> {cmd}")
                resp = send_cmd(sock, cmd).strip()
                print(resp)
                sys.exit(0)
            else:
                a = f"{ns}_a"; x = f"{ns}_x"; y = f"{ns}_y"; r = f"{ns}_r"; h = f"{ns}_h"
                cases = [
                    (f"SET {a} 1", "OK"),
                    (f"GET {a}", "OK 1"),
                    (f"EXIST {a}", "OK"),
                    (f"MOD {a} 2", "OK"),
                    (f"GET {a}", "OK 2"),
                    (f"SET {a} 3", "ERROR: Key already exists"),
                    (f"DEL {a}", "OK"),
                    (f"GET {a}", "ERROR: Key not found"),
                    (f"EXIST {a}", "ERROR: Key not found"),
                    (f"SET {x} 10", "OK"),
                    (f"SET {y} 20", "OK"),
                    (f"GET {x}", "OK 10"),
                    (f"GET {y}", "OK 20"),
                    (f"RSET {r} 100", "OK"),
                    (f"RGET {r}", "OK 100"),
                    (f"RDEL {r}", "OK"),
                    (f"RGET {r}", "ERROR: Key not found"),
                    (f"HSET {h} 42", "OK"),
                    (f"HGET {h}", "OK 42"),
                    (f"HMOD {h} 43", "OK"),
                    (f"HGET {h}", "OK 43"),
                    (f"HDEL {h}", "OK"),
                    (f"HGET {h}", "ERROR: Key not found"),
                ]
                passed = 0
                total = len(cases)
                for cmd, expect in cases:
                    print(f"> {cmd}")
                    resp = send_cmd(sock, cmd).strip()
                    if resp == expect:
                        print(resp)
                        passed += 1
                    else:
                        print(resp)
                        print(f"EXPECTED: {expect}")
                        print(f"FAIL {passed}/{total}")
                        sys.exit(1)
                    time.sleep(0.05)
                print(f"PASS {passed}/{total}")
                sys.exit(0)
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()