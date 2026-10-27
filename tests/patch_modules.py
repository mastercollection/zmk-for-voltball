"""Apply reviewed compatibility fixes to exact pinned external module sources in CI."""
from pathlib import Path
import subprocess
import sys

workspace = Path(sys.argv[1])
module = workspace / "prospector-zmk-module"
expected = "ed98221f3b52b7066dbb10ba3af8a29150b93a5a"
actual = subprocess.check_output(["git", "-C", str(module), "rev-parse", "HEAD"], text=True).strip()
if actual != expected:
    raise SystemExit(f"Prospector revision changed: {actual}")
path = module / "boards/shields/prospector_adapter/src/layouts/classic/battery_bar.c"
source = path.read_text()
old = "return (struct battery_update_state){.source = 0, .level = 0};"
if source.count(old) != 2:
    raise SystemExit("Unexpected battery widget source; review patch before building")
# Invalid source is ignored by set_battery_bar_value. Preserve initial N/A until an actual report.
source = source.replace(old, "return (struct battery_update_state){.source = UINT8_MAX, .level = 0};")
path.write_text(source)
print(f"Patched Prospector {actual}: retain N/A before first peripheral report")
