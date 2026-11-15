#!/usr/bin/env python3
"""
gta_ban_bypass.py - Console-ID-Rotation + Telemetrie-Shield fuer GTA Online (PS4 / GoldHEN)

Zweck: erzeugt eine neue Konsolen-Identitaet, legt sie auf der PS4 ab,
injiziert das Spoofer-Payload ueber GoldHEN BinLoader und baut eine
Telemetrie-Blockliste fuer Router/DNS.

Nutzung:
  python gta_ban_bypass.py --ps4 192.168.1.50 --rotate --payload spoof.bin
  python gta_ban_bypass.py --ps4 192.168.1.50 --status
  python gta_ban_bypass.py --telemetry-only
"""
import argparse, ftplib, io, json, os, secrets, socket, sys, time

FTP_PORT   = 2121
BIN_PORT   = 9090
CONF_DIR   = "/data/GoldHEN/plugins/gta_ban_bypass"
STATE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bypass_state.json")

TELEMETRY_HOSTS = [
    "prod.telemetry.ros.rockstargames.com",
    "telemetry.ros.rockstargames.com",
    "prod.telemetry2.ros.rockstargames.com",
]
# optional, kann Online-Verhalten beeinflussen:
OPTIONAL_HOSTS = [
    "prod.cs.ros.rockstargames.com",
]

def new_identity():
    return {
        "open_psid": secrets.token_hex(16),          # 16 Byte OpenPsId
        "console_id": secrets.token_hex(16),         # 16 Byte Console-ID
        "psid": secrets.token_hex(16),
        "serial": "MB" + "".join(secrets.choice("0123456789") for _ in range(10)),
        "created": time.strftime("%Y-%m-%d %H:%M:%S"),
    }

def load_state():
    if os.path.exists(STATE_FILE):
        with open(STATE_FILE) as f:
            return json.load(f)
    return {"generation": 0, "current": None, "history": []}

def save_state(st):
    with open(STATE_FILE, "w") as f:
        json.dump(st, f, indent=2)

def ftp_connect(ip, port=FTP_PORT, user="anonymous", pw="anonymous"):
    ftp = ftplib.FTP()
    ftp.connect(ip, port, timeout=15)
    ftp.login(user, pw)
    return ftp

def ftp_makedirs(ftp, path):
    cur = ""
    for part in [p for p in path.split("/") if p]:
        cur += "/" + part
        try:
            ftp.mkd(cur)
        except ftplib.error_perm:
            pass

def ftp_put(ftp, path, data):
    ftp.storbinary(f"STOR {path}", io.BytesIO(data.encode()))

def inject(ip, payload_path, port=BIN_PORT):
    data = open(payload_path, "rb").read()
    s = socket.create_connection((ip, port), timeout=15)
    s.sendall(data)
    s.close()
    return len(data)

def cmd_rotate(args):
    st = load_state()
    ident = new_identity()
    st["generation"] += 1
    st["current"] = ident
    st["history"].append(ident)
    st["history"] = st["history"][-20:]
    save_state(st)

    conf = (
        f"# generation {st['generation']} - {ident['created']}\n"
        f"open_psid={ident['open_psid']}\n"
        f"console_id={ident['console_id']}\n"
        f"psid={ident['psid']}\n"
        f"serial={ident['serial']}\n"
    )

    ftp = ftp_connect(args.ps4)
    ftp_makedirs(ftp, CONF_DIR)
    ftp_put(ftp, f"{CONF_DIR}/ids.conf", conf)
    ftp_put(ftp, f"{CONF_DIR}/generation.txt", str(st["generation"]))
    ftp.quit()
    print(f"[+] Identitaet Generation {st['generation']} auf Konsole abgelegt.")

    if args.payload:
        n = inject(args.ps4, args.payload)
        print(f"[+] Spoofer-Payload injiziert ({n} Bytes).")
    else:
        print("[!] Kein --payload angegeben, Spoof nicht aktiv.")

    print("[i] Jetzt PSN-User + R*-Account anlegen, DANN GTA starten.")

def cmd_status(args):
    st = load_state()
    if not st["current"]:
        print("Keine Identitaet erzeugt.")
        return
    print(f"Generation : {st['generation']}")
    print(f"Erstellt   : {st['current']['created']}")
    print(f"OpenPsId   : {st['current']['open_psid']}")
    print(f"Console-ID : {st['current']['console_id']}")
    print(f"Serien-Nr. : {st['current']['serial']}")

def cmd_telemetry(args):
    dns = "# GTA Online Telemetrie-Shield - dnsmasq\ntelemetry_filter=1\n"
    for h in TELEMETRY_HOSTS:
        dns += f"address=/{h}/0.0.0.0\n"
    for h in OPTIONAL_HOSTS:
        dns += f"# address=/{h}/0.0.0.0\n"
    hosts = "# GTA Online Telemetrie-Shield - hosts\n"
    hosts += "".join(f"0.0.0.0 {h}\n" for h in TELEMETRY_HOSTS)

    open("gta_telemetry_dnsmasq.conf", "w").write(dns)
    open("gta_telemetry_hosts.txt", "w").write(hosts)
    print("[+] gta_telemetry_dnsmasq.conf + gta_telemetry_hosts.txt geschrieben.")
    print("[i] dnsmasq-Config auf den Router (Pi-hole/OpenWrt), PS4-DNS auf den Router zeigen.")

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--ps4", help="IP der PS4")
    p.add_argument("--payload", help="Pfad zum Spoofer-.bin")
    p.add_argument("--rotate", action="store_true", help="neue Identitaet erzeugen + injizieren")
    p.add_argument("--status", action="store_true")
    p.add_argument("--telemetry-only", action="store_true")
    a = p.parse_args()

    if a.telemetry_only:
        return cmd_telemetry(a)
    if a.status:
        return cmd_status(a)
    if a.rotate:
        if not a.ps4:
            sys.exit("--ps4 fehlt")
        return cmd_rotate(a)
    p.print_help()

if __name__ == "__main__":
    main()