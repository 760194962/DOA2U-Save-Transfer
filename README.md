# DOA2U Save Transfer

在不同主机之间迁移 **Dead or Alive Ultimate / 生死格斗 Ultimate**（原版 Xbox，TitleID `54430006`）的档案存档 `ups.dat`。支持原版 Xbox、xemu 模拟器和 Xbox 360 向下兼容之间互相迁移。

Transfer Dead or Alive Ultimate (Original Xbox, TitleID `54430006`) profile saves (`ups.dat`) between original Xbox consoles, xemu and Xbox 360 backward compatibility. [English below](#english).

实测：xemu 上生成的档案 → Xbox 360（向下兼容），可以正常读取。

---

## 为什么普通的重签工具不够

常见的存档重签工具只处理签名。但 DOA Ultimate 的 `ups.dat` 绑定了主机的三样东西，只改签名的话，游戏会提示「用户档案损坏」（ユーザープロファイルが破損しているようで読み込めません）。

| # | 位置 | 绑定内容 | 说明 |
|---|---|---|---|
| 1 | `0x00–0x13` | **XboxHDKey** | 标准 XCalculateSignature（非漫游） |
| 2 | `0x18–` 整个数据区 | **以太网 MAC 地址** | 用 MAC 加密（见下文） |
| 3 | 解密后 `0xB1E9` | **以太网 MAC 地址** | 档案里明文存了一份创建者主机的 MAC，后接 8 字节创建时间 |

这个工具会把三处都换成目标主机的值。

### 加密方式（从 `DOA2.xbe` 逆向）

```
seed   = ups.dat[0x14..0x17]                        (小端)
mt     = MT19937.init_by_array([seed, mac[0..3], mac[4..5]])
bfkey  = mt 的前 14 个输出（56 字节）
明文   = Blowfish-ECB-Decrypt(bfkey, 密文 XOR mt 后续输出)   // 每 8 字节块按两个小端 uint32 处理
明文结尾 23 字节 = "Lightning Offering Guy\0"          // 用来判断解密是否成功
```

MAC 地址是游戏通过 `XNetGetTitleXnAddr()` 取到的 `abEnet`，**不一定等于系统设置里显示的 MAC**。例如在 Xbox 360 上开关某些网络伪装服务、或者设置替代 MAC 后，游戏读到的值会变。所以最稳妥的做法是：在目标主机上新建一个档案，用它的 `ups.dat` 找出游戏实际读到的 MAC。

---

## 下载

- **Windows**：[`bin/DOA2U-Save-Transfer.exe`](bin/DOA2U-Save-Transfer.exe)。单文件，带界面，不用安装。
- **所有系统**：[`doau_transfer.py`](doau_transfer.py)。只需要 Python 3，负责验证和转换（需要已知 MAC）。
- **macOS / Linux 搜 MAC**：用 C 源码编译命令行工具（见下文）。

## 需要准备

1. **源存档**：要迁移的整个存档文件夹（`ups.dat`、`SaveMeta.xbx`、`SaveImage.xbx`）。
2. **目标主机的 XboxHDKey**
   - 原版 Xbox / xemu：用 dashboard 或 EEPROM 工具读取。
   - Xbox 360：由主板序列号和主机序列号拼出来，参考 [feudalnate 的说明](https://github.com/feudalnate/Original-Xbox-Gamesave-Resigners/blob/master/XboxHDKey.md)。
3. **目标主机上新建的一个档案的 `ups.dat`**（推荐）：用来找出游戏实际读到的 MAC，同时验证 HD Key 对不对。

## 使用方法（Windows exe）

1. **① 源存档**：选择要迁移的 `ups.dat`。
   - 知道源主机 MAC 就直接填。
   - 不知道就点「从源存档搜索 MAC」。前缀默认 `00:50:F2`，原版 Xbox 和 xemu 基本都是这个。
2. **② 目标主机**：填 HD Key。
   - 在「参考 ups.dat」选目标主机上新建档案的 `ups.dat`，点「从参考存档搜索 MAC」。前缀留空时会用「目标主机 MAC」一栏的前 3 字节，可以先填系统设置里显示的 MAC。
3. 点「**验证**」。确认 HD Key 和两个 MAC 都显示 ✓。
4. 点「**转换并保存…**」，把新的 `ups.dat` 覆盖到源存档文件夹里。
5. 把**整个存档文件夹**放到目标主机的 `UDATA/54430006/` 下。

> ⚠ **不要只把 `ups.dat` 放进目标主机上别的存档文件夹里**。存档文件夹名和 `SaveMeta.xbx` 里的档案名是对应的，混用会导致读取失败。

搜索一个前缀要试 1677 万个 MAC，在普通电脑上大约需要几分钟。

## 使用方法（Python，全平台）

```
python doau_transfer.py verify  ups.dat --mac 00:25:AE:DD:EE:FF --hdkey TARGET_HDKEY
python doau_transfer.py convert src/ups.dat new_ups.dat \
       --src-mac 00:50:F2:AA:BB:CC --dst-mac 00:25:AE:DD:EE:FF --hdkey TARGET_HDKEY
```

Python 版不做 MAC 搜索，因为纯 Python 太慢（要数百小时）。需要搜索的话，用 exe 或下面的 C 命令行版。

## macOS / Linux：命令行搜 MAC

```
cc -O3 -pthread -o doau-search src/cli.c src/doau_core.c src/bf_tables.c
./doau-search ups.dat 00:50:F2
```

## 自己编译 Windows 版

```
x86_64-w64-mingw32-windres src/app.rc -O coff -o src/app.res
x86_64-w64-mingw32-gcc -O2 -municode -mwindows -static -o DOA2U-Save-Transfer.exe \
    src/gui.c src/doau_core.c src/bf_tables.c src/app.res -lcomctl32 -lcomdlg32
```

## 其他存档文件

- `doasave.sav`、`br.dat` 使用「漫游」签名，不绑定主机，可以直接拷贝，不需要这个工具。
- 本工具只处理 `ups.dat`（74056 字节）。

## 致谢

- 签名算法和 HD Key 说明参考了 [feudalnate/Original-Xbox-Gamesave-Resigners](https://github.com/feudalnate/Original-Xbox-Gamesave-Resigners)。
- `ups.dat` 的加密和内嵌 MAC 是本项目从游戏程序逆向得到的。

使用前请先备份存档。本工具与 Microsoft、KOEI TECMO 无关。

---

## English

Most save resigners only fix the signature. DOA Ultimate's `ups.dat` is bound to the console in **three** places, so a resigned-only save shows "user profile is corrupted":

1. `0x00–0x13`: non-roamable XCalculateSignature (title key + **XboxHDKey**).
2. `0x18–`: payload encrypted with the console's **Ethernet MAC**. The MAC is seeded, together with the 4-byte seed at `0x14`, into MT19937 via `init_by_array([seed, mac[0..3], mac[4..5]])`. The first 14 outputs form a 56-byte Blowfish key; the rest are XORed with the data, then it is Blowfish-ECB decrypted (blocks as two little-endian uint32). The plaintext ends with `"Lightning Offering Guy\0"`.
3. Decrypted offset `0xB1E9`: the creator's **MAC** again, in plain text, followed by an 8-byte creation timestamp.

The MAC is whatever the game gets from `XNetGetTitleXnAddr()`. On Xbox 360 this can differ from the MAC shown in system settings, so the reliable way is to create a new profile on the target console and brute-force the MAC from its `ups.dat` (16.7M candidates per OUI prefix, a few minutes in C).

**Usage:** Windows GUI `bin/DOA2U-Save-Transfer.exe`, cross-platform `doau_transfer.py` (verify/convert with known MACs), or build `src/cli.c` for MAC search on macOS/Linux. Always copy the **whole** save folder into `UDATA/54430006/`, not just `ups.dat`.

Tested: xemu → Xbox 360 backward compatibility.
