# DOA2U Save Transfer

在不同主机之间迁移 **Dead or Alive Ultimate**（原版 Xbox，TitleID `54430006`）的档案存档 `ups.dat`。
Move Dead or Alive Ultimate (Original Xbox, TitleID `54430006`) profile saves (`ups.dat`) between consoles / xemu / Xbox 360 backward compatibility.

已实测：xemu 生成的档案 → Xbox 360（向下兼容）可以正常读取。

---

## 中文

### 已确认的事实

只重签（只改签名）不够，游戏会提示用户档案损坏。`ups.dat`（74056 字节）里我们已经确认的内容：

- **签名**（`0x00–0x13`）：非漫游签名，用标题密钥 + 目标主机的 XboxHDKey 计算。
- **加密**（`0x18–` 到文件末尾）：数据用以太网 MAC 地址加密。用错误的 MAC 解不出来，用正确的 MAC 解出来的末尾是 `Lightning Offering Guy\0`。算法从 `DOA2.xbe` 逆向得到，见下方。
- **明文里的 MAC**：解密后 `0xB1E9` 处有 6 字节，与创建该档案的主机 MAC 相同。

### 尚不确定的事

- 游戏读档时有没有检查上面那份明文 MAC，**没有验证过**。本工具转换时会把它一起改成目标主机的 MAC，这样和目标主机自己新建的档案一致，但不能说明它是必须的。
- 只替换 `ups.dat`、不带文件夹时读档失败过一次，整个文件夹放进去就正常。原因**未验证**：可能是文件夹名、`SaveMeta.xbx`、档案名之间需要对应，也可能是别的原因。所以建议一律拷贝整个存档文件夹。
- 游戏读到的 MAC 来自 `XNetGetTitleXnAddr()`，在 Xbox 360 上曾观察到和系统设置里显示的 MAC 不一样（用过某些网络伪装服务后）。所以不要直接相信设置里的 MAC，用目标主机自己新建的档案来搜索更可靠。

### 现成存档

[`saves/54430006/`](saves/54430006/) 里有两个做好的 Dead or Alive Ultimate 存档。两个档案的 `ups.dat` 都用 MAC `00:00:00:00:00:00` 和 HD Key `00000000000000000000000000000000` 加密和签名，不绑定任何人的主机。

- **`13639293CE5E`「All Unlocked」**：推荐优先使用。全解锁的干净初始档，包括全部角色（含天狗、Bayman）、全部服装、全部 rare item 和普通道具（每样记为拿过 1 次）、全部 System Voice。游玩时间为 0，survival 排行榜是默认记录，设置是默认值。
- **`17B41AC3258C`「Jeremy」**：作者纯手打的全解锁存档，带几小时游玩时间和 survival 排行榜记录。All Unlocked 有问题时再用这个。

用法：先用本工具把 `ups.dat` 转换到自己的主机，源 MAC 填 `00:00:00:00:00:00`，再填目标主机的 MAC 和 HD Key，例如：

```
python doau_transfer.py convert ups.dat new_ups.dat --src-mac 00:00:00:00:00:00 --dst-mac <目标MAC> --hdkey <目标HDKey>
```

也可以用 exe 的界面。转换后把**整个** `54430006` 文件夹放到目标的 `UDATA/` 下，不要只放 `ups.dat`。只需要一个档案的话，可以只保留对应的子文件夹。

已在 xemu 上实测：All Unlocked 用 xemu 自己的 MAC/HD Key 时可以正常读档，解锁完整。全 0 版本本身没有在实机上直接读过。

### 下载

- **Windows**：[`bin/DOA2U-Save-Transfer.exe`](bin/DOA2U-Save-Transfer.exe)，单文件带界面，支持高 DPI；中英文界面右下角切换，或用 `--en` / `--zh` 启动。
- **所有系统**：[`doau_transfer.py`](doau_transfer.py)，只需 Python 3，验证和转换（需要已知 MAC）。
- **所有系统，带界面**：`python doau_gui.py`（Python 3 + tkinter，无其他依赖；加 `--en` / `--zh` 选语言）。同样需要已知 MAC，不能搜 MAC。
- **macOS / Linux 搜 MAC**：编译 `src/cli.c`。

### 使用方法（exe）

需要准备：源存档整个文件夹；目标主机的 XboxHDKey；目标主机上新建的一个档案的 `ups.dat`（用来找出游戏读到的 MAC 并验证 HD Key）。

1. ① 选源 `ups.dat`。知道源主机 MAC 就填；不知道就点「从源存档搜索 MAC」（默认前缀 `00:50:F2` 是 Microsoft 的 OUI，我们的 xemu 存档实测是这个前缀；搜不到就换成源主机设置里显示的 MAC 的前 3 字节）。
2. ② 填目标 HD Key；选目标主机新建档案的 `ups.dat` 作参考，点「从参考存档搜索 MAC」。
3. 点「验证」，确认都是 ✓。
4. 点「转换并保存…」，覆盖到源存档文件夹里的 `ups.dat`。
5. 把整个存档文件夹放到目标主机的 `UDATA/54430006/`。

搜一个前缀要试 1677 万个 MAC，普通电脑几分钟。

### Python / 命令行

```
python doau_transfer.py verify  ups.dat --mac 00:25:AE:DD:EE:FF --hdkey TARGET_HDKEY
python doau_transfer.py convert src/ups.dat new_ups.dat \
       --src-mac 00:50:F2:AA:BB:CC --dst-mac 00:25:AE:DD:EE:FF --hdkey TARGET_HDKEY

cc -O3 -pthread -o doau-search src/cli.c src/doau_core.c src/bf_tables.c
./doau-search ups.dat 00:50:F2
```

Python 版不搜 MAC（纯 Python 太慢）。

### 加密方式

```
seed   = ups.dat[0x14..0x17]                (小端)
mt     = MT19937.init_by_array([seed, mac[0..3], mac[4..5]])
bfkey  = mt 前 14 个输出（56 字节）
明文   = Blowfish-ECB-Decrypt(bfkey, 密文 XOR mt 后续输出)   // 8 字节块按两个小端 uint32
```

### 自己编译 Windows 版

```
x86_64-w64-mingw32-windres src/app.rc -O coff -o src/app.res
x86_64-w64-mingw32-gcc -O2 -municode -mwindows -static -o DOA2U-Save-Transfer.exe \
    src/gui.c src/doau_core.c src/bf_tables.c src/app.res -lcomctl32 -lcomdlg32
```

### 其他

- `doasave.sav`、`br.dat` 用漫游签名，不绑定主机，可直接拷贝。
- 签名算法和 HD Key 说明参考 [feudalnate/Original-Xbox-Gamesave-Resigners](https://github.com/feudalnate/Original-Xbox-Gamesave-Resigners)。
- 使用前请备份存档。与 Microsoft、KOEI TECMO 无关。

---

## English

**Usage**

1. Pick the source `ups.dat`. Enter its console MAC, or click *Find MAC from source* (default prefix `00:50:F2` is Microsoft's OUI and matched our xemu save; if nothing is found, try the first 3 bytes of the MAC shown in the source console's settings).
2. Enter the target console's XboxHDKey. Create a new profile on the target console, pick its `ups.dat` as *Reference*, and click *Find MAC from reference*.
3. Click *Verify* until everything shows ✓, then *Convert & Save…*.
4. Copy the **whole** save folder into `UDATA/54430006/` on the target.

Cross-platform: `python doau_transfer.py convert src out --src-mac .. --dst-mac .. --hdkey ..`. MAC search on macOS/Linux: build `src/cli.c`.
Cross-platform GUI: `python doau_gui.py` (Python 3 + tkinter, `--en` / `--zh`). It needs a known MAC and cannot search for one.

**Notes**

- Signing alone is not enough: the payload is encrypted with the console MAC (see the Chinese section for the algorithm), and the signature uses the target HD key.
- The tool also rewrites the plaintext MAC found at decrypted offset `0xB1E9` so the file matches a natively created one. Whether the game actually checks it is untested.
- Replacing only `ups.dat` failed once; copying the whole folder worked. The cause is unverified.
- On Xbox 360 the MAC the game sees can differ from the one in system settings, so search it from a reference `ups.dat` made on that console.

**Ready-made saves**

[`saves/54430006/`](saves/54430006/) contains two ready-made Dead or Alive Ultimate saves. Both `ups.dat` files are encrypted and signed with MAC `00:00:00:00:00:00` and HD Key `00000000000000000000000000000000`, so they are not tied to anyone's console.

- **`13639293CE5E` "All Unlocked"** (recommended first): a clean, fully unlocked starting save: all characters (including Tengu and Bayman), all costumes, all rare items and normal items (each counted as obtained once), all System Voices. Play time is 0, survival rankings are default, settings are default.
- **`17B41AC3258C` "Jeremy"**: a fully unlocked save the author built entirely by hand, with several hours of play time and survival ranking records. Use it if All Unlocked gives problems.

Usage: first convert `ups.dat` to your own console with this tool, using source MAC `00:00:00:00:00:00` plus the target's MAC and HD Key, e.g.:

```
python doau_transfer.py convert ups.dat new_ups.dat --src-mac 00:00:00:00:00:00 --dst-mac <target MAC> --hdkey <target HD key>
```

or use the exe GUI. Then put the **whole** `54430006` folder under the target's `UDATA/`, not just `ups.dat`. If you only need one save, keep just that subfolder.

Tested on xemu: All Unlocked loads correctly with xemu's own MAC/HD Key, fully unlocked. The all-zero version itself has not been loaded directly on real hardware.

Tested: xemu → Xbox 360 backward compatibility. Back up your saves first.
