#!/usr/bin/env python3
"""Inspect a Canon tagged vector resource from a flat ROM (FC or FE alias)."""
import argparse
import struct
from pathlib import Path


def decode(data):
    if len(data) < 0x48:
        raise ValueError('short Canon vector resource')
    u16 = lambda p: struct.unpack_from('<H', data, p)[0]
    if u16(0) != 0x9999:
        raise ValueError('not a Canon vector resource')
    paths, commands = [], None
    for p in range(0x48, len(data) - 6, 2):
        tag, size = u16(p), u16(p + 2)
        if size < 6 or p + size > len(data):
            continue
        if tag == 0xAAA0:
            count = u16(p + 4)
            if count > size - 6:
                raise ValueError('path command count exceeds tag size')
            commands = data[p + 6:p + 6 + count]
        elif tag == 0xAAA1 and commands is not None:
            count = u16(p + 4)
            if count > (size - 6) // 2:
                raise ValueError('coordinate count exceeds tag size')
            coords = iter(struct.unpack_from('<' + 'h' * count, data, p + 6))
            path = []
            for command in commands:
                kind = command & ~1
                names = {0: ('Z', 0), 2: ('M', 2), 4: ('L', 2),
                         6: ('H', 1), 8: ('V', 1), 10: ('Q', 4), 12: ('C', 6)}
                if kind not in names:
                    raise ValueError(f'unsupported path command {command:#x}')
                name, n = names[kind]
                if command & 1:
                    name = name.lower()
                values = []
                for _ in range(n):
                    value = next(coords, None)
                    if value is None:
                        raise ValueError('too few path coordinates')
                    values.append(str(value / 16))
                path.append(name + ' '.join(values))
            paths.append(' '.join(path))
            commands = None
    return paths


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('address', type=lambda s: int(s, 0))
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    if not 0xFC000000 <= args.address <= 0xFFFFFFFF:
        parser.error('address must use the FC or FE ROM alias')
    offset = (args.address - 0xFC000000) % 0x02000000
    if offset + 0x48 > len(rom):
        parser.error('resource header is outside the ROM file')
    length = struct.unpack_from('<I', rom, offset + 4)[0]
    if length < 0x48 or length > len(rom) - offset:
        parser.error('invalid resource length')
    data = rom[offset:offset + length]
    try:
        paths = decode(data)
    except ValueError as error:
        parser.error(str(error))
    width, height = [struct.unpack_from('<I', data, p)[0] / 1048576 for p in (0x24, 0x28)]
    args.output.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}">' +
                          ''.join(f'<path fill="black" fill-rule="evenodd" d="{p}"/>' for p in paths) + '</svg>')
    print(f'{len(paths)} paths, {width} x {height}')
