"""Pack build/blob/sidcore.elf into sidcore.bin (layout in src/blob/sidcore_api.h).

The ELF is ordinary (not position-independent) ARM code linked at address 0 (src/blob/sidcore.ld)
with --emit-relocs, so the linker's relocation records are still in it.  Every absolute 32-bit address
in the image (R_ARM_ABS32, R_ARM_TARGET1) goes in the list the host fixes up by adding the base
address.  PC-relative relocations need nothing.  Any other type is an error: something on the Palm
would point at the wrong place.
Usage: python tools/blob_pack.py in.elf out.bin
"""
import struct, sys

ABSOLUTE = {2: "R_ARM_ABS32", 38: "R_ARM_TARGET1"}
PC_RELATIVE = {0: "NONE", 3: "R_ARM_REL32", 28: "R_ARM_CALL", 29: "R_ARM_JUMP24", 40: "R_ARM_V4BX",
               42: "R_ARM_PREL31"}
MAGIC, VERSION = 0x43444953, 1


def main(src, dst):
    elf = open(src, "rb").read()
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1:
        sys.exit("not a 32-bit little-endian ELF")
    entry, = struct.unpack_from("<I", elf, 24)
    shoff, = struct.unpack_from("<I", elf, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 46)
    secs = []
    for i in range(shnum):
        name, typ, flags, addr, off, size, link, info, align, entsize = \
            struct.unpack_from("<10I", elf, shoff + i * shentsize)
        secs.append(dict(name=name, type=typ, flags=flags, addr=addr, off=off, size=size, link=link))
    strtab = secs[shstrndx]
    for s in secs:
        s["name"] = elf[strtab["off"] + s["name"]:elf.index(b"\0", strtab["off"] + s["name"])].decode()

    # symbols
    syms = {}
    for s in secs:
        if s["type"] == 2:  # SHT_SYMTAB
            st = secs[s["link"]]
            for k in range(s["size"] // 16):
                nm, val, sz, inf, oth, shn = struct.unpack_from("<IIIBBH", elf, s["off"] + k * 16)
                n = elf[st["off"] + nm:elf.index(b"\0", st["off"] + nm)].decode()
                syms[n] = val
    image_end, bss_end = syms["__image_end"], syms["__bss_end"]
    # Linker-made veneers (ARM/Thumb interworking, long branches) hold absolute addresses without
    # relocation records, so they would not be fixed up on the Palm.
    veneers = [n for n in syms if n.endswith("_veneer") or "_from_arm" in n or "_from_thumb" in n]
    if veneers:
        sys.exit("linker veneers found (Thumb library code?): " + ", ".join(sorted(veneers)))

    # image: every allocated section with contents, placed at its address
    image = bytearray(image_end)
    for s in secs:
        if s["flags"] & 2 and s["type"] != 8 and s["addr"] < image_end and s["size"]:  # ALLOC, not NOBITS
            if s["name"].startswith(".rel"):
                continue
            image[s["addr"]:s["addr"] + s["size"]] = elf[s["off"]:s["off"] + s["size"]]

    relocs = set()
    for s in secs:
        if s["type"] == 4:  # SHT_RELA
            sys.exit("unexpected RELA section " + s["name"])
        if s["type"] != 9:  # SHT_REL
            continue
        target = secs[struct.unpack_from("<I", elf, shoff + secs.index(s) * shentsize + 28)[0]]
        if not target["flags"] & 2:      # relocations for debug info etc.: not in the image
            continue
        for k in range(s["size"] // 8):
            off, inf = struct.unpack_from("<II", elf, s["off"] + k * 8)
            typ = inf & 0xff
            if typ in PC_RELATIVE:
                continue
            if typ not in ABSOLUTE:
                sys.exit("unsupported relocation type %d at 0x%x (%s)" % (typ, off, s["name"]))
            if target["type"] == 8:     # NOBITS: an address stored in bss would be zero anyway
                sys.exit("absolute relocation in bss at 0x%x" % off)
            if off >= image_end or off & 3:
                sys.exit("bad relocation offset 0x%x (%s)" % (off, s["name"]))
            relocs.add(off)
    relocs = sorted(relocs)

    hdr = struct.pack("<6I", MAGIC, VERSION, image_end, bss_end - image_end, entry, len(relocs))
    with open(dst, "wb") as f:
        f.write(hdr + bytes(image) + struct.pack("<%dI" % len(relocs), *relocs))
    print("%s: image %d bytes, bss %d bytes, entry 0x%x, %d relocations"
          % (dst, image_end, bss_end - image_end, entry, len(relocs)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
