# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

from pathlib import Path
import sys
from urllib.parse import quote
from xml.etree import ElementTree


def write_case_reports(artifact_root):
    cases = ElementTree.parse(str(artifact_root / "junit.xml")).iter("testcase")
    rows = [
        "| Test case | Result | Output | Config snapshots |",
        "| --- | --- | --- | --- |",
    ]
    logs = artifact_root / "logs"
    logs.mkdir(exist_ok=True)
    for case in cases:
        name = case.attrib["name"]
        log = logs / f"{quote(name, safe='')}.log"
        output = [case.findtext("system-out", ""), case.findtext("system-err", "")]
        status = "Passed"
        for node in case:
            if node.tag in ("failure", "error", "skipped"):
                status = "Skipped" if node.tag == "skipped" else "Failed"
                output.append(node.text or node.get("message", ""))
        log.write_text("\n".join(part for part in output if part), encoding="utf-8")
        log_link = quote(log.relative_to(artifact_root).as_posix(), safe="/")

        configs = artifact_root / name / "configs"
        if not configs.is_dir():
            # Shipped-config CTest names differ from their GoogleTest names.
            gtest_case = case.findtext("system-out", "").split("Google Test filter = ", 1)
            if len(gtest_case) == 2:
                configs = artifact_root / gtest_case[1].splitlines()[0].strip() / "configs"
        snapshots = ", ".join(
            f"[{file.name}]({quote(file.relative_to(artifact_root).as_posix(), safe='/')})"
            for file in sorted(configs.glob("*.yaml"))
        ) or "—"
        rows.append(f"| `{name}` | {status} | [Output]({log_link}) | {snapshots} |")

    with (artifact_root / "report.md").open("a", encoding="utf-8") as report:
        report.write("\n".join(rows) + "\n")


if __name__ == "__main__":
    write_case_reports(Path(sys.argv[1]))
