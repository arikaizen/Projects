"""network_ssl_cert — Check SSL certificate info for a hostname."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import ssl, socket
from datetime import datetime, timezone

def handler(args, keys):
    hostname = args["hostname"]
    port = int(args.get("port", 443))
    ctx = ssl.create_default_context()
    with socket.create_connection((hostname, port), timeout=15) as sock:
        with ctx.wrap_socket(sock, server_hostname=hostname) as ssock:
            cert = ssock.getpeercert()

    def parse_dn(dn_tuples):
        return {k: v for tup in dn_tuples for k, v in tup}

    subject = parse_dn(cert.get("subject", []))
    issuer = parse_dn(cert.get("issuer", []))
    not_before = cert.get("notBefore")
    not_after = cert.get("notAfter")
    fmt = "%b %d %H:%M:%S %Y %Z"
    expiry_dt = datetime.strptime(not_after, fmt).replace(tzinfo=timezone.utc)
    now = datetime.now(timezone.utc)
    days_until_expiry = (expiry_dt - now).days
    san = [v for t, v in cert.get("subjectAltName", []) if t == "DNS"]
    return {
        "subject": subject,
        "issuer": issuer,
        "not_before": not_before,
        "not_after": not_after,
        "days_until_expiry": days_until_expiry,
        "san": san,
    }

run(handler)
