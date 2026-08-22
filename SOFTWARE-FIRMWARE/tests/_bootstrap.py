from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "shared" / "protocol"))
sys.path.insert(0, str(ROOT / "windows" / "parol6_backend"))

