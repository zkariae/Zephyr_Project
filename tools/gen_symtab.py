#!/usr/bin/env python3
"""Genere src/generated/symtab_data.c a partir de la table de symboles d'un
zephyr.elf deja lie, pour le profileur PC-sampling (src/pc_profiler.c).

Invoque automatiquement par CMakeLists.txt en POST_BUILD sur la cible
zephyr_final. Le fichier genere decrit le binaire QUI VIENT D'ETRE LIE ;
il n'est utilise (compile) que lors du build SUIVANT (voir docs/architecture.md,
section "Task Management" : build en 2 passes).

Usage: gen_symtab.py --nm <chemin nm> --elf <zephyr.elf> --out <symtab_data.c>
"""
import argparse
import re
import subprocess
import sys

# Lignes `nm -S --size-sort` : "<addr> <size> <type> <name>"
NM_LINE_RE = re.compile(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([a-zA-Z])\s+(\S+)$")

HEADER = '#include "pc_profiler.h"\n\nconst struct symtab_entry pc_profiler_symtab[] = {\n'
FOOTER = (
    "};\n\n"
    "const unsigned int pc_profiler_symtab_count =\n"
    "    sizeof(pc_profiler_symtab) / sizeof(pc_profiler_symtab[0]);\n"
)


def parse_nm_output(text: str) -> list[tuple[int, int, str]]:
    """Retourne une liste (addr, size, name), dedupliquee par addr (garde le
    symbole global 'T' en priorite sur un alias local 't')."""
    by_addr: dict[int, tuple[int, str, bool]] = {}  # addr -> (size, name, is_global)

    for line in text.splitlines():
        m = NM_LINE_RE.match(line.strip())
        if not m:
            continue
        addr_s, size_s, sym_type, name = m.groups()
        if sym_type not in ("t", "T"):
            continue
        addr = int(addr_s, 16)
        size = int(size_s, 16)
        if size == 0:
            continue
        is_global = sym_type == "T"
        existing = by_addr.get(addr)
        if existing is None or (is_global and not existing[2]):
            by_addr[addr] = (size, name, is_global)

    return sorted((addr, size, name) for addr, (size, name, _) in by_addr.items())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nm", required=True, help="chemin vers l'utilitaire nm du toolchain")
    parser.add_argument("--elf", required=True, help="chemin vers zephyr.elf")
    parser.add_argument("--out", required=True, help="chemin de sortie symtab_data.c")
    args = parser.parse_args()

    result = subprocess.run(
        [args.nm, "-S", args.elf], capture_output=True, text=True, check=True
    )
    symbols = parse_nm_output(result.stdout)

    with open(args.out, "w", encoding="utf-8") as f:
        f.write(HEADER)
        for addr, size, name in symbols:
            f.write(f'    {{ 0x{addr:08x}u, 0x{size:x}u, "{name}" }},\n')
        f.write(FOOTER)

    print(f"gen_symtab: {len(symbols)} symboles ecrits dans {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
