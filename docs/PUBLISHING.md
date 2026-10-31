# 上传 GitHub

本目录已经按公开仓库整理，未包含原工作目录的 `.git`、个人账户路径、USB 卷序列号、原始启动日志、Windows BCD/引导备份、PROM、AMI 镜像、固件文件或下载缓存。硬件地址、哈希及固定 ESP GUID 是本机实现的必要配置，保留并明确不通用。

1. 在 GitHub 新建空仓库，不要另加冲突的 README/许可证。
2. 在此目录运行 `python tools/verify_package.py`。
3. 可用 GitHub Desktop 添加此目录创建本地仓库后发布，或使用 git 初始化、提交、添加你自己的远端并推送。
4. 保留 `source`、`LICENSE`、`LICENSES`、第三方声明与固定来源，不要只上传 EFI。
5. 仓库描述建议：`Experimental Windows EFI tensor-compute unlock for one captured RTX 3060 GA106 2503 / X99 system; source, recovery, tests and measured results.`

该目录包含两份很小的 EFI，可正常随 Git 上传，不需要 Git LFS。完整源码的 EDK2 头文件数量较多，建议 GitHub Desktop/git 整体上传。固件准备及构建之后出现的 `local`、`build` 不应上传；.gitignore 会排除。网页拖拽不会替你执行 .gitignore，使用网页时务必排除这两个目录。

整理操作没有登录 GitHub、创建远端、推送、重装当前机器或修改其已工作的启动设置。
