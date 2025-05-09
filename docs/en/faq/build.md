## Build


### 1. Already pulled latest source code via `git pull` but still encounter compilation errors when building apps

TuyaOpen downloads supported platforms dynamically via submodules. Updating the main TuyaOpen repository won't automatically update platform submodules. If you have the latest TuyaOpen code but still face compilation issues, try these solutions:

- Run `tos update` in the TuyaOpen directory to update all downloaded platforms:
    ```sh
    $ tos update
    Updating platform ...
    Already up to date.
    T5AI success.
    Already up to date.
    ESP32 success.
    ```

- Manually navigate to the specific platform directory (e.g., `platform/T5AI`), switch to the `master` branch and pull updates:
    ```sh
    $ cd platform/T5AI
    $ git checkout master
    $ git pull
    ```

- Delete and redownload the platform folder (e.g., remove `T5AI` directory and let the system redownload it)
