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

### 下载

- **Windows**：[`bin/DOA2U-Save-Transfer.exe`](bin/DOA2U-Save-Transfer.exe)，单文件带界面，支持高 DPI；中英文界面右下角切换，或用 `--en` / `--zh` 启动。
- **所有系统**：[`doau_transfer.py`](doau_transfer.py)，只需 Python 3，验证和转换（需要已知 MAC）。
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

### 存档文件夹名

存档文件夹名（如 `1210DD1B1F0C`）由存档名（`SaveMeta.xbx` 里 `Name=` 后面的文字）算出来，算法来自 `DOA2.xbe`（XDK 5849，函数地址 `0x2b9d02`）：

```
h = 0
对名字的每个 UTF-16 字符 c：  h = (h × 0x10000 + c) mod (2^48 − 59)
文件夹名 = h 的 12 位大写十六进制
```

`SaveMeta.xbx` 里的档案名末尾带一个零宽空格 U+200B，它也参与计算。已用 4 个游戏自己生成的 DOA 档案核对（`プレイヤー1`、`プレイヤー2`、`Xbox360Jeremy`、`プレイヤー1Ö`），文件夹名全部一致。算法只用到存档名，和 HD Key、MAC 无关。

```
python doau_transfer.py foldername "Xbox360Jeremy" --zwsp
python doau_transfer.py foldername 存档文件夹/SaveMeta.xbx      # 同时检查文件夹名是否一致
```

**已实测**（Xbox 360 向下兼容）：
- 只改 `ups.dat` 里的档案名、不改 `SaveMeta.xbx`，游戏读档会报错：`ups.dat` 里的档案名要和 `SaveMeta.xbx` 的 `Name=` 一致。检查过的那个档案里，`ups.dat` 内的名字末尾没有 U+200B，`SaveMeta.xbx` 里有。
- 给 DOA 档案改名（`ups.dat` 里的名字、`SaveMeta.xbx` 的 `Name=`、按上面算法算出的文件夹名三处一起改，`ups.dat` 用原 MAC 重新加密、用目标 HD Key 重签），游戏能读出来（新 KV 下测试）。
- 文件夹名和存档名对不上时（把改了名的 `SaveMeta.xbx` 和 `ups.dat` 放进别的名字的文件夹），游戏的档案列表里仍会显示新名字。

**未测试：** 改名后的存档能否正常再保存；两个存档同名时游戏的行为；文件夹名对不上时能否读档、存档。

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

**Save folder name**

The save folder name (e.g. `1210DD1B1F0C`) is derived from the save name (the text after `Name=` in `SaveMeta.xbx`). Algorithm, read from `DOA2.xbe` (XDK 5849, function at `0x2b9d02`):

```
h = 0
for each UTF-16 code unit c of the name:  h = (h * 0x10000 + c) mod (2^48 - 59)
folder = 12 uppercase hex digits of h
```

The profile name in `SaveMeta.xbx` ends with U+200B (zero-width space), which is part of the hash. Checked against 4 profiles created by the game (`プレイヤー1`, `プレイヤー2`, `Xbox360Jeremy`, `プレイヤー1Ö`); the folder names all match. The algorithm uses only the name, not the HD key or MAC.

```
python doau_transfer.py foldername "Xbox360Jeremy" --zwsp
python doau_transfer.py foldername path/to/SaveMeta.xbx     # also checks the folder name
```

Tested (Xbox 360 backward compatibility):
- Changing only the profile name inside `ups.dat` (leaving `SaveMeta.xbx` as is) makes the game report an error: the name inside `ups.dat` must match `Name=` in `SaveMeta.xbx`. In the profile checked, the name inside `ups.dat` has no trailing U+200B while `SaveMeta.xbx` does.
- Renaming a DOA profile, changing all three places together (name inside `ups.dat`, `Name=` in `SaveMeta.xbx`, folder name from the algorithm above; `ups.dat` re-encrypted with the original MAC and re-signed with the target HD key), the game reads it (tested with the new KV).
- With a folder name that does not match the save name (renamed `SaveMeta.xbx` and `ups.dat` placed in a folder with another name), the game still shows the new name in its profile list.

Not tested: saving again after renaming; what the game does with two saves of the same name; loading or saving when the folder name does not match.

**Notes**

- Signing alone is not enough: the payload is encrypted with the console MAC (see the Chinese section for the algorithm), and the signature uses the target HD key.
- The tool also rewrites the plaintext MAC found at decrypted offset `0xB1E9` so the file matches a natively created one. Whether the game actually checks it is untested.
- Replacing only `ups.dat` failed once; copying the whole folder worked. The cause is unverified.
- On Xbox 360 the MAC the game sees can differ from the one in system settings, so search it from a reference `ups.dat` made on that console.

Tested: xemu → Xbox 360 backward compatibility. Back up your saves first.
