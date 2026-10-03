#!/usr/bin/env python3
"""
DOA2U Save Transfer - cross-platform GUI (Python 3 + tkinter, no other dependencies).

Needs the source MAC to be known (e.g. 00:00:00:00:00:00 for the ready-made
saves in saves/). Searching for an unknown MAC is not supported here; use the
Windows exe or src/cli.c for that.
"""
import os, sys, tkinter as tk
from tkinter import ttk, filedialog, messagebox
import doau_transfer as T

TXT = {
    "en": dict(
        title="DOA2U Save Transfer", src="Source save", src_ups="Source ups.dat:", src_mac="Source MAC:",
        dst="Target console", hd="Target HD Key:", dst_mac="Target MAC:", ref="Reference ups.dat (optional):",
        browse="Browse...", verify="Verify", convert="Convert & Save...", lang="中文",
        help=("1. Pick the source ups.dat and enter its MAC (ready-made saves: 00:00:00:00:00:00).\n"
              "2. Enter the target console's HD Key and MAC. Optionally pick a ups.dat created on the target as\n"
              "   Reference to check the HD Key.\n"
              "3. Click Verify until everything shows OK, then Convert & Save.\n"
              "4. Copy the WHOLE save folder (with SaveMeta.xbx, SaveImage.xbx) into UDATA/54430006/ - not just ups.dat.\n"),
        pick="Select ups.dat", out="Save converted ups.dat",
        v_src_ok="Source: MAC OK, profile \"%s\", embedded MAC %s", v_src_bad="Source: this MAC cannot decrypt the save",
        v_ref_ok="Reference: signature matches this HD Key", v_ref_bad="Reference: signature does NOT match this HD Key",
        done="Wrote %s\n(profile \"%s\", embedded MAC %s -> %s)\nCopy the WHOLE save folder to UDATA/54430006/.",
    ),
    "zh": dict(
        title="DOA2U 存档转换", src="源存档", src_ups="源 ups.dat：", src_mac="源 MAC：",
        dst="目标主机", hd="目标 HD Key：", dst_mac="目标 MAC：", ref="参考 ups.dat（可选）：",
        browse="浏览...", verify="验证", convert="转换并保存...", lang="English",
        help=("1. 选择源 ups.dat 并填入它的 MAC（现成存档：00:00:00:00:00:00）。\n"
              "2. 填入目标主机的 HD Key 和 MAC。可选：选一个在目标主机上新建的 ups.dat 作为参考，用来检查 HD Key。\n"
              "3. 点“验证”，全部显示 OK 后点“转换并保存”。\n"
              "4. 把整个存档文件夹（含 SaveMeta.xbx、SaveImage.xbx）放进 UDATA/54430006/，不要只放 ups.dat。\n"),
        pick="选择 ups.dat", out="保存转换后的 ups.dat",
        v_src_ok="源存档：MAC 正确，档案名“%s”，内嵌 MAC %s", v_src_bad="源存档：这个 MAC 解不开存档",
        v_ref_ok="参考：签名与此 HD Key 匹配", v_ref_bad="参考：签名与此 HD Key 不匹配",
        done="已写入 %s\n（档案名“%s”，内嵌 MAC %s → %s）\n请把整个存档文件夹放进 UDATA/54430006/。",
    ),
}


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.lang = "zh" if "--zh" in sys.argv or (
            "--en" not in sys.argv and (os.environ.get("LANG", "") + os.environ.get("LC_ALL", "")).startswith("zh")) else "en"
        self.v = {k: tk.StringVar() for k in ("src", "smac", "hd", "dmac", "ref")}
        self.v["smac"].set("00:00:00:00:00:00")
        self.widgets = []
        self.build()
        self.relabel()

    def build(self):
        pad = dict(padx=6, pady=3)
        self.f1 = ttk.LabelFrame(self); self.f1.pack(fill="x", padx=10, pady=(10, 4))
        self.f2 = ttk.LabelFrame(self); self.f2.pack(fill="x", padx=10, pady=4)
        for f in (self.f1, self.f2):
            f.columnconfigure(1, weight=1)
        self.row(self.f1, 0, "src_ups", "src", browse=True)
        self.row(self.f1, 1, "src_mac", "smac")
        self.row(self.f2, 0, "hd", "hd")
        self.row(self.f2, 1, "dst_mac", "dmac")
        self.row(self.f2, 2, "ref", "ref", browse=True)
        bar = ttk.Frame(self); bar.pack(fill="x", padx=10, pady=4)
        self.b_verify = ttk.Button(bar, command=self.verify); self.b_verify.pack(side="left", padx=3)
        self.b_conv = ttk.Button(bar, command=self.convert); self.b_conv.pack(side="left", padx=3)
        self.b_lang = ttk.Button(bar, command=self.toggle); self.b_lang.pack(side="right", padx=3)
        self.log = tk.Text(self, height=12, width=86, wrap="word")
        self.log.pack(fill="both", expand=True, padx=10, pady=(4, 10))

    def row(self, parent, r, label_key, var, browse=False):
        lab = ttk.Label(parent); lab.grid(row=r, column=0, sticky="w", padx=6, pady=3)
        ttk.Entry(parent, textvariable=self.v[var]).grid(row=r, column=1, sticky="ew", padx=6, pady=3)
        self.widgets.append((lab, label_key))
        if browse:
            b = ttk.Button(parent, command=lambda: self.pick(var)); b.grid(row=r, column=2, padx=6, pady=3)
            self.widgets.append((b, "browse"))

    def relabel(self):
        t = TXT[self.lang]
        self.title(t["title"])
        self.f1["text"], self.f2["text"] = t["src"], t["dst"]
        for w, k in self.widgets:
            w["text"] = t[k]
        self.b_verify["text"], self.b_conv["text"], self.b_lang["text"] = t["verify"], t["convert"], t["lang"]
        self.say(t["help"], clear=True)

    def toggle(self):
        self.lang = "zh" if self.lang == "en" else "en"
        self.relabel()

    def say(self, s, clear=False):
        if clear:
            self.log.delete("1.0", "end")
        self.log.insert("end", s if s.endswith("\n") else s + "\n")
        self.log.see("end")

    def pick(self, var):
        p = filedialog.askopenfilename(title=TXT[self.lang]["pick"])
        if p:
            self.v[var].set(p)

    def hexval(self, key, n, what):
        try:
            return T.parse_hex(self.v[key].get(), n, what)
        except SystemExit as e:
            raise ValueError(str(e))

    def load(self, key):
        try:
            return T.load(self.v[key].get().strip().strip('"'))
        except SystemExit as e:
            raise ValueError(str(e))
        except OSError as e:
            raise ValueError("✗ %s" % e)

    def verify(self):
        t = TXT[self.lang]
        try:
            b = self.load("src")
            p = T.decrypt(b, self.hexval("smac", 6, "source MAC"))
            if T.plain_ok(p):
                name, emb = T.profile_info(p)
                self.say("✓ " + t["v_src_ok"] % (name, T.fmt(emb)))
            else:
                self.say("✗ " + t["v_src_bad"])
            if self.v["hd"].get().strip() and self.v["ref"].get().strip():
                r = self.load("ref")
                ok = T.sign(r[20:], self.hexval("hd", 16, "HD key")) == r[:20]
                self.say(("✓ " + t["v_ref_ok"]) if ok else ("✗ " + t["v_ref_bad"]))
        except ValueError as e:
            self.say(str(e))

    def convert(self):
        t = TXT[self.lang]
        try:
            src = self.load("src")
            smac = self.hexval("smac", 6, "source MAC")
            dmac = self.hexval("dmac", 6, "target MAC")
            hd = self.hexval("hd", 16, "HD key")
            p = bytearray(T.decrypt(src, smac))
            if not T.plain_ok(p):
                self.say("✗ " + t["v_src_bad"]); return
            out_path = filedialog.asksaveasfilename(title=t["out"], initialfile="ups.dat")
            if not out_path:
                return
            name, emb = T.profile_info(p)
            p[T.MAC_OFFSET:T.MAC_OFFSET + 6] = dmac
            out = bytearray(T.encrypt(p, dmac))
            out[:20] = T.sign(bytes(out[20:]), hd)
            with open(out_path, "wb") as f:
                f.write(out)
            self.say("✓ " + t["done"] % (out_path, name, T.fmt(emb), T.fmt(dmac)))
        except ValueError as e:
            self.say(str(e))
        except OSError as e:
            messagebox.showerror(t["title"], str(e))


if __name__ == "__main__":
    App().mainloop()
