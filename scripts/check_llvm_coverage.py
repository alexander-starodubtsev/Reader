#!/usr/bin/env python3
"""Enforce an LLVM line-coverage minimum for core business rules."""

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) not in (2, 3):
        print("usage: check_llvm_coverage.py <llvm-cov-export.json> [minimum-percent]")
        return 2

    report_path = Path(sys.argv[1])
    minimum_percent = float(sys.argv[2]) if len(sys.argv) == 3 else 80.0
    report = json.loads(report_path.read_text(encoding="utf-8"))
    files = report["data"][0]["files"]
    failed = False

    business_rules = ("/core/library/", "/core/sync/", "/core/stats/")
    matching = [
        item
        for item in files
        if any(path in item["filename"].replace("\\", "/") for path in business_rules)
    ]
    line_count = sum(item["summary"]["lines"]["count"] for item in matching)
    covered_lines = sum(item["summary"]["lines"]["covered"] for item in matching)
    percent = 100.0 if line_count == 0 else covered_lines * 100.0 / line_count
    print(f"core business logic line coverage: {covered_lines}/{line_count} ({percent:.2f}%)")
    if line_count == 0 or percent < minimum_percent:
        failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
