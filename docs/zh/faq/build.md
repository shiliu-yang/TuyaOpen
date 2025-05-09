## 编译

### 1. 已经通过 `git pull` 拉取 TuyaOpen 仓库 `master` 分支最新源代码，但在编译具体 apps 下项目时候编译依然出错

TuyaOpen 支持的 platform 通过子仓库动态下载，更新 TuyaOpen 仓库不会主动更新子仓库，如遇到 TuyaOpen 已经是最新代码但编译依然出错，可通过以下方法排除：

- 在 TuyaOpen 目录下运行 `tos update` 命令更新 TuyaOpen 当前已经下载的 platform
    ```sh
    $ tos update
    Updating platform ...
    Already up to date.
    T5AI success.
    Already up to date.
    ESP32 success.
    ```

- 手工切换目录至 platform 文件夹下对应的芯片目录下，如 `platform/T5AI`，将分支切换到 `master` 使用 `git pull` 命令更新
    ```sh
    $ cd platform/T5AI
    $ git checkout master
    $ git pull
    ```

- 删除 platform 文件夹下对应目录，如 `T5AI`后再次下载

