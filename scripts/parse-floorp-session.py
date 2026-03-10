#!/usr/bin/env python3
"""Parse Floorp session store, match windows to desktops by title, output tabs per desktop.

Usage: parse-floorp-session.py <recovery.jsonlz4> <title_map_file>

title_map_file format (TSV): desktop_idx\tactive_tab_title

Output format (per window, separated by blank lines):
  desktop_idx
  url1
  url2
  ...
"""

import json
import sys

import lz4.block


def main():
    session_file = sys.argv[1]
    title_map_file = sys.argv[2]

    with open(session_file, "rb") as f:
        f.read(8)  # mozLz40\0
        session = json.loads(lz4.block.decompress(f.read()))

    title_to_desktop = {}
    with open(title_map_file) as f:
        for line in f:
            line = line.rstrip("\n")
            if "\t" in line:
                idx, title = line.split("\t", 1)
                title_to_desktop[title] = int(idx)

    for w in session["windows"]:
        tabs = w.get("tabs", [])
        if not tabs:
            continue

        active_idx = w.get("selected", 1) - 1
        if 0 <= active_idx < len(tabs):
            entries = tabs[active_idx].get("entries", [])
            active_title = entries[-1].get("title", "") if entries else ""
        else:
            active_title = ""

        desktop_idx = title_to_desktop.get(active_title)
        if desktop_idx is None:
            # Try substring match
            for known_title, idx in title_to_desktop.items():
                if known_title in active_title or active_title in known_title:
                    desktop_idx = idx
                    break
        if desktop_idx is None:
            # Try matching by longest common prefix (at least 20 chars)
            best_len = 0
            for known_title, idx in title_to_desktop.items():
                common = 0
                for a, b in zip(active_title, known_title):
                    if a != b:
                        break
                    common += 1
                if common > best_len and common >= 20:
                    best_len = common
                    desktop_idx = idx

        if desktop_idx is None:
            print(
                f'Window "{active_title[:60]}" — no desktop match, skipping',
                file=sys.stderr,
            )
            continue

        urls = []
        for t in tabs:
            entries = t.get("entries", [])
            if entries:
                url = entries[-1].get("url", "")
                if url and not url.startswith("about:"):
                    urls.append(url)

        if urls:
            print(desktop_idx)
            for url in urls:
                print(url)
            print()  # blank line separator


if __name__ == "__main__":
    main()
