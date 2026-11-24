# 待办事项 / TODO

## 待定
- [ ] 重新编译 Windows exe（`bin/DOA2U-Save-Transfer.exe` 是 10 月 4 日的旧版，不含 foldername 功能；GUI 在 Verify 后显示计算出的文件夹名需要新 exe）。编译命令见 README「自己编译 Windows 版」，本机已装 mingw-w64。编译后需确认能启动、foldername 正常。
- [ ] 发布 release（目前仓库没有任何 tag / release）。先确定版本号：之前提交里有 "GUI v1.1"，若要发 v1.1 需先重新编译 exe。
- [ ] 一键改名（可选新功能）：目前改存档名要手动同时改三处（`ups.dat` 里的名字、`SaveMeta.xbx` 的 `Name=`、文件夹名），foldername 只负责算出文件夹名。

## 已完成
- [x] 合并 `add-foldername` 分支到 `main` 并推送，已删除远程分支（2026-11-24）
