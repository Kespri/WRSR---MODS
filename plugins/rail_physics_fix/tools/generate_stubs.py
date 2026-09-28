"""Generate the self-contained header; ordinary MSVC builds need no assembler.

Usage: python tools/generate_stubs.py --clang <clang.exe> [--check]
Only internal AMD64 COFF REL32 relocations are accepted. Unknown/external
relocations are errors. No binary is loaded/executed by this generator.
"""
import argparse
import hashlib
import struct
import subprocess
import tempfile
from pathlib import Path


def extract(path):
    data = Path(path).read_bytes()
    machine, count, _, symptr, symcount, opt, _ = struct.unpack_from('<HHIIIHH', data)
    assert machine == 0x8664 and opt == 0
    strings = data[symptr+symcount*18:]
    symbols = {}
    for i in range(symcount):
        name, value, section, _, _, _ = struct.unpack_from('<8sIhHBB', data, symptr+i*18)
        if name[:4] == b'\0'*4:
            offset = struct.unpack_from('<I', name, 4)[0]
            name = strings[offset:].split(b'\0', 1)[0]
        symbols[i] = (name.rstrip(b'\0'), value, section)
    for i in range(count):
        fields = struct.unpack_from('<8sIIIIIIHHI', data, 20+i*40)
        name, _, _, size, raw, relocs, _, nrelocs, _, _ = fields
        if name.rstrip(b'\0') != b'.text':
            continue
        text = bytearray(data[raw:raw+size])
        for r in range(nrelocs):
            address, symbol, kind = struct.unpack_from('<IIH', data, relocs+r*10)
            name, value, section = symbols[symbol]
            assert section == i+1, f'External relocation: {name!r}'
            assert kind == 4, f'Unsupported relocation type {kind}'
            addend = struct.unpack_from('<i', text, address)[0]
            struct.pack_into('<i', text, address, value+addend-address-4)
        offsets = {n.decode(): v for n, v, s in symbols.values()
                   if s == i+1 and n.startswith(b'rp_')}
        return text, offsets
    raise AssertionError('Missing .text')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--clang', required=True)
    p.add_argument('--check', action='store_true')
    a = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root/'rail_physics_fix_stubs.S'
    with tempfile.TemporaryDirectory(prefix='rail_physics_fix_stubs_') as temp:
        obj = Path(temp)/'stubs.obj'
        subprocess.run([a.clang, '--target=x86_64-pc-windows-msvc', '-c', str(source), '-o', str(obj)], check=True)
        code, offsets = extract(obj)
    assert len(offsets) == 15
    header = '// Generated from rail_physics_fix_stubs.S. GPL-3.0. Do not hand-edit.\n'
    # Normalize line endings so archive/checkout conversions are harmless.
    digest = hashlib.sha256(source.read_text(encoding='utf-8').encode()).hexdigest()
    header += '// Source SHA256 (LF): '+digest+'\n#pragma once\n'
    header += 'static const unsigned char kRailStubCode[] = {\n'
    for i in range(0, len(code), 16):
        header += '    '+', '.join(f'0x{b:02X}' for b in code[i:i+16])+',\n'
    header += '};\n'
    for name, value in sorted(offsets.items()):
        header += f'static const size_t k_{name} = 0x{value:X};\n'
    target = root/'rail_physics_fix_stubs.h'
    if a.check:
        assert target.read_text(encoding='utf-8') == header, 'Generated header out of date'
    else:
        target.write_text(header, encoding='utf-8', newline='\n')
    print(f'PASS: {len(code)} bytes, {len(offsets)} symbols, internal relocations resolved')


if __name__ == '__main__':
    main()
