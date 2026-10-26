"""mk3db.py - builds FLvibed's own icon mesh for the maneuver bar's autopilot button (docs\\UI_PLAN.md 2, decision 2):
a minimal Freelancer .3db (UTF) with one textured quad, its own material and texture, written to
EXE\\FLvibed\\autopilot.3db. Nothing of the game's files is copied: the node layout was LEARNED from an installed
DATA\\INTERFACE\\HUD\\MNVR_*.3db (`inspect`), the geometry, material and texture here are FLvibed's own.

  python -I tools\\mk3db.py inspect <file.3db|.vms>      the UTF tree with the decoded VMeshRef / VMeshData headers
  python -I tools\\mk3db.py crc <name>                    the game's model CRC of a name (materials, mesh libraries)
  python -I tools\\mk3db.py build <out.3db> [--size 64]   builds the autopilot icon (ring and chevron, white, alpha)

Run with the OCR venv's python (research\\tools\\ocr\\.venv\\Scripts\\python.exe -I); needs no package beyond the
standard library (the TGA is written by hand).
Format (verified against HDE 0.7.1's files, 25 Oct 2026; docs\\FREELANCER_RE.md "Icon meshes"):
  UTF header 56 bytes: 'UTF ' 0x101, nodeBlockOffset, nodeSize(44), ?, headerSize(44), stringBlockOffset,
  stringBlockSize, stringBlockSizeUsed?, dataBlockOffset, ..., timestamps. Node (44): peerOffset, nameOffset, flags
  (0x10 folder, 0x80 leaf), ?, childOffset/dataOffset, allocatedSize, size, size2, timestamps x3.
  Leaves of an icon 3db: Exporter Version; VMeshPart\\VMeshRef (60 bytes: headerSize 0x3C, VMeshLibId CRC, startVertex,
  vertexCount, startIndex, indexCount, startMesh, meshCount (u16 each), bbox max/min x y z, centre xyz, radius);
  VMeshWire\\VWireData (the outline lines); Material library\\<name>\\{Type 'DcDtOcOt', Dt_name 'x.tga', Dt_flags
  0x40}, Material count; Texture library\\<x.tga>\\MIP0 (an uncompressed 32-bit TGA). The vertices live in a
  VMeshLibrary\\<lib>.vms\\VMeshData (the HUD's in DATA\\INTERFACE\\interface.generic.vms): u32 meshType 1, u32
  surfaceType 4, u16 meshCount, u16 indexCount, u16 FVF (0x112 = XYZ|NORMAL|TEX1, 32-byte vertices), u16 vertexCount;
  meshCount x {u32 materialCrc, u16 startVertex, u16 endVertex, u16 refVertexCount, u16 0xCC}; indexCount x u16;
  the vertices.
"""
import math
import struct
import sys
import time

# ------------------------------------------------------------------ the game's CRC of names (materials, mesh ids)
# Freelancer's own CRC-32 table (not the standard polynomial's; the game's DACOM GetCRC32, as FLUF's Utf::Crc32
# reproduces it - research\repos\FLUF\FLUF\Include\Common\Utf\Crc.hpp), over the lower-cased name, init and final
# xor 0xFFFFFFFF. Verified (25 Oct 2026): crc('interface.generic-102.vms') = 0x1351B6D4 (the goto icon's VMeshRef
# library id) and crc('HUD_MNVR_goto') = 0x1B3E1E7C (the material id of its mesh, mesh 78 of that library).
_TABLE = [
    0x00000000, 0x09073096, 0x120E612C, 0x1B0951BA, 0xFF6DC419, 0xF66AF48F, 0xED63A535, 0xE46495A3,
    0xFEDB8832, 0xF7DCB8A4, 0xECD5E91E, 0xE5D2D988, 0x01B64C2B, 0x08B17CBD, 0x13B82D07, 0x1ABF1D91,
    0xFDB71064, 0xF4B020F2, 0xEFB97148, 0xE6BE41DE, 0x02DAD47D, 0x0BDDE4EB, 0x10D4B551, 0x19D385C7,
    0x036C9856, 0x0A6BA8C0, 0x1162F97A, 0x1865C9EC, 0xFC015C4F, 0xF5066CD9, 0xEE0F3D63, 0xE7080DF5,
    0xFB6E20C8, 0xF269105E, 0xE96041E4, 0xE0677172, 0x0403E4D1, 0x0D04D447, 0x160D85FD, 0x1F0AB56B,
    0x05B5A8FA, 0x0CB2986C, 0x17BBC9D6, 0x1EBCF940, 0xFAD86CE3, 0xF3DF5C75, 0xE8D60DCF, 0xE1D13D59,
    0x06D930AC, 0x0FDE003A, 0x14D75180, 0x1DD06116, 0xF9B4F4B5, 0xF0B3C423, 0xEBBA9599, 0xE2BDA50F,
    0xF802B89E, 0xF1058808, 0xEA0CD9B2, 0xE30BE924, 0x076F7C87, 0x0E684C11, 0x15611DAB, 0x1C662D3D,
    0xF6DC4190, 0xFFDB7106, 0xE4D220BC, 0xEDD5102A, 0x09B18589, 0x00B6B51F, 0x1BBFE4A5, 0x12B8D433,
    0x0807C9A2, 0x0100F934, 0x1A09A88E, 0x130E9818, 0xF76A0DBB, 0xFE6D3D2D, 0xE5646C97, 0xEC635C01,
    0x0B6B51F4, 0x026C6162, 0x196530D8, 0x1062004E, 0xF40695ED, 0xFD01A57B, 0xE608F4C1, 0xEF0FC457,
    0xF5B0D9C6, 0xFCB7E950, 0xE7BEB8EA, 0xEEB9887C, 0x0ADD1DDF, 0x03DA2D49, 0x18D37CF3, 0x11D44C65,
    0x0DB26158, 0x04B551CE, 0x1FBC0074, 0x16BB30E2, 0xF2DFA541, 0xFBD895D7, 0xE0D1C46D, 0xE9D6F4FB,
    0xF369E96A, 0xFA6ED9FC, 0xE1678846, 0xE860B8D0, 0x0C042D73, 0x05031DE5, 0x1E0A4C5F, 0x170D7CC9,
    0xF005713C, 0xF90241AA, 0xE20B1010, 0xEB0C2086, 0x0F68B525, 0x066F85B3, 0x1D66D409, 0x1461E49F,
    0x0EDEF90E, 0x07D9C998, 0x1CD09822, 0x15D7A8B4, 0xF1B33D17, 0xF8B40D81, 0xE3BD5C3B, 0xEABA6CAD,
    0xEDB88320, 0xE4BFB3B6, 0xFFB6E20C, 0xF6B1D29A, 0x12D54739, 0x1BD277AF, 0x00DB2615, 0x09DC1683,
    0x13630B12, 0x1A643B84, 0x016D6A3E, 0x086A5AA8, 0xEC0ECF0B, 0xE509FF9D, 0xFE00AE27, 0xF7079EB1,
    0x100F9344, 0x1908A3D2, 0x0201F268, 0x0B06C2FE, 0xEF62575D, 0xE66567CB, 0xFD6C3671, 0xF46B06E7,
    0xEED41B76, 0xE7D32BE0, 0xFCDA7A5A, 0xF5DD4ACC, 0x11B9DF6F, 0x18BEEFF9, 0x03B7BE43, 0x0AB08ED5,
    0x16D6A3E8, 0x1FD1937E, 0x04D8C2C4, 0x0DDFF252, 0xE9BB67F1, 0xE0BC5767, 0xFBB506DD, 0xF2B2364B,
    0xE80D2BDA, 0xE10A1B4C, 0xFA034AF6, 0xF3047A60, 0x1760EFC3, 0x1E67DF55, 0x056E8EEF, 0x0C69BE79,
    0xEB61B38C, 0xE266831A, 0xF96FD2A0, 0xF068E236, 0x140C7795, 0x1D0B4703, 0x060216B9, 0x0F05262F,
    0x15BA3BBE, 0x1CBD0B28, 0x07B45A92, 0x0EB36A04, 0xEAD7FFA7, 0xE3D0CF31, 0xF8D99E8B, 0xF1DEAE1D,
    0x1B64C2B0, 0x1263F226, 0x096AA39C, 0x006D930A, 0xE40906A9, 0xED0E363F, 0xF6076785, 0xFF005713,
    0xE5BF4A82, 0xECB87A14, 0xF7B12BAE, 0xFEB61B38, 0x1AD28E9B, 0x13D5BE0D, 0x08DCEFB7, 0x01DBDF21,
    0xE6D3D2D4, 0xEFD4E242, 0xF4DDB3F8, 0xFDDA836E, 0x19BE16CD, 0x10B9265B, 0x0BB077E1, 0x02B74777,
    0x18085AE6, 0x110F6A70, 0x0A063BCA, 0x03010B5C, 0xE7659EFF, 0xEE62AE69, 0xF56BFFD3, 0xFC6CCF45,
    0xE00AE278, 0xE90DD2EE, 0xF2048354, 0xFB03B3C2, 0x1F672661, 0x166016F7, 0x0D69474D, 0x046E77DB,
    0x1ED16A4A, 0x17D65ADC, 0x0CDF0B66, 0x05D83BF0, 0xE1BCAE53, 0xE8BB9EC5, 0xF3B2CF7F, 0xFAB5FFE9,
    0x1DBDF21C, 0x14BAC28A, 0x0FB39330, 0x06B4A3A6, 0xE2D03605, 0xEBD70693, 0xF0DE5729, 0xF9D967BF,
    0xE3667A2E, 0xEA614AB8, 0xF1681B02, 0xF86F2B94, 0x1C0BBE37, 0x150C8EA1, 0x0E05DF1B, 0x0702EF8D,
]


def fl_crc(name):
    '''Freelancer's model CRC (materials, VMeshLibrary ids): the game's CRC-32 of the lower-cased name.'''
    crc = 0xFFFFFFFF
    for b in name.lower().encode('latin1'):
        crc = (crc >> 8) ^ _TABLE[(crc ^ b) & 0xFF]
    return crc ^ 0xFFFFFFFF


# ------------------------------------------------------------------ UTF reading
class Node:
    def __init__(self, name, data=None):
        self.name, self.data, self.children = name, data, []

    def find(self, path):
        n = self
        for part in path.split('\\'):
            n = next((c for c in n.children if c.name.lower() == part.lower()), None)
            if n is None:
                return None
        return n

    def walk(self, prefix=''):
        p = (prefix + '\\' + self.name) if prefix else self.name
        yield p, self
        for c in self.children:
            yield from c.walk(p)


def read_utf(path):
    b = open(path, 'rb').read()
    if b[:4] != b'UTF ':
        raise ValueError('not a UTF file: ' + path)
    node_off, str_off, data_off = struct.unpack_from('<I', b, 8)[0], struct.unpack_from('<I', b, 24)[0], \
        struct.unpack_from('<I', b, 36)[0]

    def name_at(o):
        e = b.index(b'\0', str_off + o)
        return b[str_off + o:e].decode('latin1')

    def read_nodes(rel):
        out = []
        while True:
            n = node_off + rel
            peer, name_o, flags, _, child, _alloc, size = struct.unpack_from('<IIIIIII', b, n)
            node = Node(name_at(name_o))
            if flags & 0x10:
                node.children = read_nodes(child)
            else:
                node.data = b[data_off + child:data_off + child + size]
            out.append(node)
            if not peer:
                return out
            rel = peer

    roots = read_nodes(0)
    return roots[0]


# ------------------------------------------------------------------ UTF writing
def write_utf(root, path):
    names, name_pos = bytearray(), {}
    nodes, datas = [], bytearray()

    def name_of(s):
        if s not in name_pos:
            name_pos[s] = len(names)
            names.extend(s.encode('latin1') + b'\0')
        return name_pos[s]

    def emit(node):
        idx = len(nodes)
        nodes.append(None)
        if node.data is None:
            first = len(nodes) * 44 if node.children else 0
            nodes[idx] = [0, name_of(node.name), 0x10, 0, first, 0, 0, 0]
            for i, c in enumerate(node.children):
                ci = emit(c)
                if i + 1 < len(node.children):
                    nodes[ci][0] = len(nodes) * 44  # peer = the next sibling (emitted next)
        else:
            d = bytes(node.data)
            while len(datas) % 4:
                datas.append(0)
            off = len(datas)
            datas.extend(d)
            nodes[idx] = [0, name_of(node.name), 0x80, 0, off, len(d), len(d), len(d)]
        return idx

    emit(root)
    while len(names) % 4:
        names.append(0)
    now = int(time.time())
    header = 56
    node_block = bytearray()
    for n in nodes:
        node_block += struct.pack('<IIIIIIIIIII', n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], now, now, now)
    node_off = header
    str_off = node_off + len(node_block)
    data_off = str_off + len(names)
    hdr = struct.pack('<4sIIIIIIIIIIIII', b'UTF ', 0x101, node_off, 44, 0, 44, str_off, len(names), len(names),
                      data_off, 0, 0, now, now)
    hdr = hdr.ljust(header, b'\0')
    open(path, 'wb').write(hdr + node_block + names + datas)


# ------------------------------------------------------------------ inspect
def decode_ref(d):
    f = struct.unpack_from('<IIHHHHHH10f', d, 0)
    return ('headerSize=%d libId=%08X startVertex=%d vertexCount=%d startIndex=%d indexCount=%d startMesh=%d '
            'meshCount=%d bbox=(%.3f %.3f %.3f %.3f %.3f %.3f) centre=(%.3f %.3f %.3f) radius=%.3f' % f)


def decode_vmeshdata(d, want_lib=None):
    mesh_type, surf, mesh_count, index_count, fvf, vertex_count = struct.unpack_from('<IIHHHH', d, 0)
    out = ['meshType=%d surfaceType=%d meshes=%d indices=%d fvf=0x%X vertices=%d' % (mesh_type, surf, mesh_count,
                                                                                      index_count, fvf, vertex_count)]
    o = 16
    for i in range(mesh_count):
        crc, sv, ev, nref, pad = struct.unpack_from('<IHHHH', d, o)
        o += 12
        if i < 6 or want_lib:
            out.append('  mesh %d: material %08X vertices %d..%d refVertices %d pad %04X' % (i, crc, sv, ev, nref, pad))
    out.append('  ... indices at %d, vertices at %d (%d bytes each)' % (o, o + 2 * index_count,
                                                                       (len(d) - o - 2 * index_count) //
                                                                       max(1, vertex_count)))
    return '\n'.join(out)


def inspect(path):
    root = read_utf(path)
    for p, n in root.walk():
        if n.data is None:
            print(p + '\\')
            continue
        line = '%s  (%d bytes)' % (p, len(n.data))
        if n.name == 'VMeshRef' and len(n.data) >= 60:
            line += '\n    ' + decode_ref(n.data)
        elif n.name == 'VMeshData' and len(n.data) >= 16:
            line += '\n    ' + decode_vmeshdata(n.data).replace('\n', '\n    ')
        elif len(n.data) <= 64:
            txt = n.data.rstrip(b'\0')
            if all(32 <= c < 127 for c in txt) and txt:
                line += "  '%s'" % txt.decode('latin1')
            else:
                line += '  ' + n.data.hex(' ')
        print(line)
    for p, n in root.walk():
        if n.data is None and p.count('\\') >= 2 and 'VMeshLibrary' in p:
            print('crc(%s) = %08X' % (n.name, fl_crc(n.name)))


# ------------------------------------------------------------------ the icon: ring and chevron, white, coverage in alpha
def icon_pixels(size):
    px = []
    stroke, aa = 0.085, 1.6 / size

    def seg(x, y, ax, ay, bx, by):
        vx, vy, wx, wy = bx - ax, by - ay, x - ax, y - ay
        t = max(0.0, min(1.0, (wx * vx + wy * vy) / (vx * vx + vy * vy)))
        return math.hypot(x - (ax + t * vx), y - (ay + t * vy))

    for j in range(size):
        for i in range(size):
            x, y = (i + 0.5) / size * 2 - 1, (j + 0.5) / size * 2 - 1
            d = abs(math.hypot(x, y) - 0.74)
            d = min(d, seg(x, y, -0.33, 0.2, 0.0, -0.25), seg(x, y, 0.33, 0.2, 0.0, -0.25))
            cov = max(0.0, min(1.0, (stroke - d) / aa + 0.5))
            px.append(int(cov * 255 + 0.5))
    return px


def tga32(size, alpha, rgb=(255, 255, 255)):
    """An uncompressed 32-bit TGA, bottom-up, one colour with the given alpha per pixel (top-down list)."""
    hdr = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, size, size, 32, 8)
    body = bytearray()
    r, g, b = rgb
    for j in range(size - 1, -1, -1):
        for i in range(size):
            a = alpha[j * size + i]
            body += bytes((b, g, r, a))  # B G R A
    return hdr + bytes(body)


# The bar's icon blue (the game's MNVR icons' line colour, measured on the 4K HUD): lit, the game tints the mesh yellow.
ICON_RGB = (150, 205, 245)


def build(out, size=64, rgb=ICON_RGB):
    mat, tex, lib = 'FLvibed_autopilot', 'FLvibed_autopilot.tga', 'FLvibed_autopilot.vms'
    mat_crc, lib_crc = fl_crc(mat), fl_crc(lib)
    # one quad in the button's plane (the game's icons span about +-0.0155 around their origin at depth 0.5 - the
    # hit half extents 0.014 the HUD passes; the frame's slots are 0.04 apart)
    h = 0.0155
    verts = [(-h, h, 0), (h, h, 0), (h, -h, 0), (-h, -h, 0)]
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    vdata = bytearray()
    for (x, y, z), (u, v) in zip(verts, uvs):
        vdata += struct.pack('<3f3f2f', x, y, z, 0, 0, 1, u, v)
    indices = [0, 1, 2, 0, 2, 3]
    vmesh = struct.pack('<IIHHHH', 1, 4, 1, len(indices), 0x112, len(verts))
    vmesh += struct.pack('<IHHHH', mat_crc, 0, len(verts) - 1, len(indices), 0xCC)
    vmesh += b''.join(struct.pack('<H', i) for i in indices) + bytes(vdata)
    ref = struct.pack('<IIHHHHHH10f', 0x3C, lib_crc, 0, len(verts), 0, len(indices), 0, 1, h, -h, h, -h, 0, 0, 0, 0,
                      0, h * math.sqrt(2))
    wire = struct.pack('<IIHHHHHH', 16, lib_crc, 0, 4, 0, 8, 0, 0) + struct.pack('<8H', 0, 1, 1, 2, 2, 3, 3, 0)

    root = Node('\\')
    root.children.append(Node('Exporter Version', b'FLvibed mk3db\0'))
    vml = Node(lib)
    vml.children.append(Node('VMeshData', vmesh))
    lib_node = Node('VMeshLibrary')
    lib_node.children.append(vml)
    root.children.append(lib_node)
    part = Node('VMeshPart')
    part.children.append(Node('VMeshRef', ref))
    root.children.append(part)
    wireN = Node('VMeshWire')
    wireN.children.append(Node('VWireData', wire))
    root.children.append(wireN)
    mlib = Node('Material library')
    mlib.children.append(Node('Material count', struct.pack('<I', 1)))
    m = Node(mat)
    m.children.append(Node('Type', b'DcDtOcOt\0'))
    m.children.append(Node('Dt_name', tex.encode() + b'\0'))
    m.children.append(Node('Dt_flags', struct.pack('<I', 0x40)))
    m.children.append(Node('Dc', struct.pack('<3f', 1, 1, 1)))
    m.children.append(Node('Oc', struct.pack('<f', 1)))
    mlib.children.append(m)
    root.children.append(mlib)
    tlib = Node('Texture library')
    t = Node(tex)
    t.children.append(Node('MIP0', tga32(size, icon_pixels(size), rgb)))
    tlib.children.append(t)
    root.children.append(tlib)
    write_utf(root, out)
    print('wrote %s (material %s %08X, library %s %08X, %dx%d)' % (out, mat, mat_crc, lib, lib_crc, size, size))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd = sys.argv[1]
    if cmd == 'inspect':
        inspect(sys.argv[2])
    elif cmd == 'crc':
        print('%08X' % fl_crc(sys.argv[2]))
    elif cmd == 'build':
        size = 64
        if '--size' in sys.argv:
            size = int(sys.argv[sys.argv.index('--size') + 1])
        rgb = ICON_RGB
        if '--rgb' in sys.argv:
            rgb = tuple(int(v) for v in sys.argv[sys.argv.index('--rgb') + 1].split(','))
        build(sys.argv[2], size, rgb)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
