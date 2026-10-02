# ps5-native-app-boilerplate - Host regression tests for the executable writer.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Links two tiny PIEs with the host Clang/lld and converts them with
# `ps5-native-tool link`: one whose RELRO region begins with .data.rel.ro and
# one without any relocated read-only data, where lld drops that section and
# RELRO begins at the GOT. Both must convert, and every mapped load segment
# must keep its 16 KiB file/address congruence. Skips without the toolchain.

import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "tooling" / "native"
PT_LOAD = 1
PT_GNU_RELRO = 0x6474E552
PT_SCE_PROCPARAM = 0x61000001

STUB_SOURCE = "int sceKernelUsleep(unsigned int microseconds) { (void)microseconds; return 0; }\n"
PLAIN_SOURCE = """
int sceKernelUsleep(unsigned int microseconds);
void _start(void);
void _start(void) { (void)sceKernelUsleep(1); }
"""
RELOCATED_SOURCE = """
int sceKernelUsleep(unsigned int microseconds);
void _start(void);
static int first(void) { return 1; }
static int second(void) { return 2; }
/* A const table of pointers is relocated read-only data: lld places it in
 * .data.rel.ro and that section begins the RELRO region. */
int (*const table[])(void) = {first, second, first, second};
void _start(void) { (void)sceKernelUsleep((unsigned int)table[1]()); }
"""


def find_tool(*names):
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    return None


def program_headers(data):
    phoff = struct.unpack_from("<Q", data, 0x20)[0]
    phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
    headers = []
    for index in range(phnum):
        p_type, p_flags, p_offset, p_vaddr, _, p_filesz, p_memsz, p_align = struct.unpack_from(
            "<IIQQQQQQ", data, phoff + index * phentsize
        )
        headers.append(dict(type=p_type, flags=p_flags, offset=p_offset, vaddr=p_vaddr,
                            filesz=p_filesz, memsz=p_memsz, align=p_align))
    return headers


def section_names(data):
    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    raw = []
    for index in range(shnum):
        name, _, _, _, offset, size = struct.unpack_from("<IIQQQQ", data, shoff + index * shentsize)
        raw.append((name, offset, size))
    strings = raw[shstrndx][1]
    names = []
    for name, _, _ in raw:
        start = strings + name
        names.append(data[start:data.index(b"\0", start)].decode("ascii"))
    return names


class ExecutableWriterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.clang = find_tool("clang-18", "clang")
        cls.lld = find_tool("ld.lld-18", "ld.lld")
        cls.cxx = find_tool("clang++-18", "clang++", "c++")
        if not (cls.clang and cls.lld and cls.cxx):
            raise unittest.SkipTest("host clang, lld and a C++20 compiler are required")
        cls.work = Path(tempfile.mkdtemp(prefix="ps5-executable-writer-"))
        cls.tool = cls.work / "ps5-native-tool"
        sources = [NATIVE / "native_app_builder.cpp", NATIVE / "self_container.cpp",
                   NATIVE / "elf_object.cpp", NATIVE / "sce_module_writer.cpp"]
        command = [cls.cxx, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror",
                   *map(str, sources), "-o", str(cls.tool)]
        zlib_root = ROOT / ".deps" / "native" / "zlib" / "root"
        archives = list(zlib_root.rglob("libz.a")) if zlib_root.exists() else []
        if archives:
            command[-2:-2] = ["-I", str(zlib_root / "usr" / "include"), str(archives[0])]
        else:
            command.insert(-2, "-lz")
        subprocess.run(command, check=True, capture_output=True, text=True)
        cls.stub = cls._link("libkernel", STUB_SOURCE, shared=True, soname="libkernel.prx",
                             exports="{ global: sceKernelUsleep; local: *; };\n")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "work"):
            shutil.rmtree(cls.work, ignore_errors=True)

    @classmethod
    def _link(cls, name, source, shared=False, soname=None, exports=None, inputs=()):
        directory = cls.work / name
        directory.mkdir(exist_ok=True)
        (directory / "main.c").write_text(source, encoding="utf-8")
        subprocess.run(
            [cls.clang, "-target", "x86_64-sie-ps5", "-fvisibility-nodllstorageclass=default",
             "-std=c11", "-O2", "-fPIC" if shared else "-fPIE", "-fno-plt",
             "-fno-stack-protector", "-fasynchronous-unwind-tables", "-nostdlib",
             "-c", str(directory / "main.c"), "-o", str(directory / "main.o")],
            check=True, capture_output=True, text=True,
        )
        output = directory / (f"{name}.so" if shared else f"{name}.elf")
        command = [cls.lld, "-m", "elf_x86_64", "-z", "max-page-size=0x4000", "--hash-style=gnu",
                   "--eh-frame-hdr", "-T", str(NATIVE / "ps5-pie.ld"), "-o", str(output),
                   str(directory / "main.o"), "--as-needed", *map(str, inputs)]
        if shared:
            command[3:3] = ["--shared", "-Bsymbolic", "-soname", soname]
            (directory / "exports.map").write_text(exports, encoding="utf-8")
            command += ["--version-script", str(directory / "exports.map")]
        else:
            command[3:3] = ["-pie", "-e", "_start"]
        subprocess.run(command, check=True, capture_output=True, text=True)
        return output

    def _convert(self, pie):
        output = pie.with_suffix(".ps5.elf")
        result = subprocess.run(
            [str(self.tool), "link", "--in", str(pie), "--out", str(output), "--stub",
             str(self.stub), "--file-name", "eboot.elf"],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return output.read_bytes()

    def _check_layout(self, converted, pie):
        headers = program_headers(converted)
        loads = [h for h in headers if h["type"] == PT_LOAD and h["flags"] != 0]
        for header in loads:
            self.assertEqual(header["offset"] % header["align"], header["vaddr"] % header["align"])
        relro = next(h for h in headers if h["type"] == PT_GNU_RELRO)
        relro_load = next(h for h in loads if h["vaddr"] == relro["vaddr"])
        self.assertEqual(relro_load["offset"], relro["offset"])
        param = next(h for h in headers if h["type"] == PT_SCE_PROCPARAM)
        self.assertTrue(relro["vaddr"] <= param["vaddr"] < relro["vaddr"] + relro["memsz"])
        # The RELRO file origin must be the lld file offset of the section that
        # starts the region, so bytes copied from the PIE land where the loader
        # maps them.
        source = pie.read_bytes()
        shoff = struct.unpack_from("<Q", source, 0x28)[0]
        shentsize, shnum, _ = struct.unpack_from("<HHH", source, 0x3A)
        names = section_names(source)
        origins = []
        for index in range(shnum):
            _, _, flags, addr, offset = struct.unpack_from("<IIQQQ", source, shoff + index * shentsize)
            if flags & 0x2 and addr == relro["vaddr"] and names[index] in (".data.rel.ro", ".got",
                                                                            ".got.plt", ".init_array"):
                origins.append(offset)
        self.assertIn(relro_load["offset"], origins)

    def test_program_with_relocated_read_only_data_converts(self):
        pie = self._link("relocated", RELOCATED_SOURCE, inputs=[self.stub])
        self.assertIn(".data.rel.ro", section_names(pie.read_bytes()))
        self._check_layout(self._convert(pie), pie)

    def test_program_without_data_rel_ro_converts(self):
        pie = self._link("plain", PLAIN_SOURCE, inputs=[self.stub])
        self.assertNotIn(".data.rel.ro", section_names(pie.read_bytes()),
                         "test premise: lld drops the empty section")
        self._check_layout(self._convert(pie), pie)


if __name__ == "__main__":
    unittest.main()
