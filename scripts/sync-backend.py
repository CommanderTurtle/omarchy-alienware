"""Copy or verify database.json only from the AWCC revision pinned in upstream.lock.json."""
import argparse
import json
import pathlib
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--awcc", type=pathlib.Path, required=True)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent
lock = json.loads((root / "upstream.lock.json").read_text())
revision = subprocess.check_output(["git", "-C", str(args.awcc), "rev-parse", "HEAD"], text=True, timeout=10).strip()
if revision != lock["awcc"]["revision"]:
    raise SystemExit("AWCC checkout differs from upstream.lock.json; review and update the pin first")
database = subprocess.check_output(["git", "-C", str(args.awcc), "show", f"{revision}:database.json"], timeout=10)
json.loads(database)
destination = root / "database.json"
if args.check:
    if not destination.exists() or destination.read_bytes() != database:
        raise SystemExit("database.json differs from the pinned upstream database")
    print(f"Verified {len(json.loads(database))} AWCC models at {revision}")
else:
    destination.write_bytes(database)
    print(f"Synced {len(json.loads(database))} AWCC models at {revision}")
