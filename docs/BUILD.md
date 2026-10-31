# 相对路径构建与固件准备

## EFI 编译

Linux 或 WSL 中准备 LLVM clang / lld-link、Python 3 和 GCC。实测使用 LLVM 21、GCC 15；WSL 只用于编译和 RAM 测试，实际启动运行 EFI。

```sh
sh tools/build.sh usb
sh tools/build.sh auto
```

默认从 PATH 查找 `clang-21` / `clang` 与 `lld-link-21` / `lld-link`。可用 `CLANG`、`LLD_LINK` 指定可执行文件；自带运行库路径由调用者配置。只生成 `build/usb/UNLOCK.EFI` 和 `build/auto/AUTO.EFI`，不安装、不访问 GPU、不改启动顺序。

入口使用随仓库保留的**本机固定配置**。工具会验证 PE 和源文件指纹，并显示新镜像是否与已实测镜像逐字节相同。编译成功不代表通过了全部模拟或实机测试；新构建元数据明确记录该限制。未改源码且相同 LLVM 21 可复现已实测哈希，本包整理时已实际检查，见 `evidence/package-validation/`。

`source/research/2503-handoff/build_auto.sh` 等为原本机历史脚本，依赖原采集/缓存目录，不作为公开入口。保留它们以记录原完整构建门槛，未将缺失的私有证据伪装成已通过。

原源码和证据保留字节级换行格式，`.gitattributes` 阻止 Git 自动改写，以保持原构建哈希可核对。v10 manifest 中的 Sources 是 v11 增加条件分支前的快照；公开树是准确匹配 v11 的共享源码，其关闭自动分支后的 v10 复现结果单独记录，不能要求旧 v10 Sources 清单与新共享树逐文件相同。

## 固件输入

实际运行还需要五个文件，清单及 SHA-256 位于 `source/internal-boot/unlock/assets.json`。仓库不附这些文件。

```sh
python -m pip install -r requirements-firmware.txt
python tools/prepare_firmware.py --download
```

显式 `--download` 才从固定来源获取约 341 MB 的两个原始包，逐一检查固定 SHA-256，再调用保留的原提取/生成脚本。也可离线提供包：

```sh
python tools/prepare_firmware.py --cache /path/to/pinned-packages
```

默认输出 `local/firmware`，原始包在 `local/cache`，均在 .gitignore 中。输出包含 NVIDIA 许可文本，未把固件并入源码许可证。下载源、原始包和派生产物的哈希不一致即停止。不要为绕过验证而替换版本或哈希。

主机完整模拟另需本机的 PROM、AMI 驱动镜像和捕获日志；它们不随仓库发布。公开策略测试与完整 fixture 测试区分见 [TESTING.md](TESTING.md)。
