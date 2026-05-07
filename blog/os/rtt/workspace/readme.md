# 工作区构建

- rtthread-sdk-5.2.2

## 工程前期准备

- 导出工程到工作区
  
```bash
scons --dist --target=vsc --project-path="D:\Desktop\workspace\code\git_project\5.project\at32f403a\lt_qboot_v1" --cmsispack="D:/tools/pack/Keil5_AT32MCU_AddOn_V2.5.0/ArteryTek.AT32F403A_407_DFP.2.2.3.pack"
```

- 添加.gitignore
```bash
rt-thread/          # 不提交rtt的sdk
build/              # 不提交编译文件
__pycache__/        # 不提交python缓存文件
```

- 删除无用文件,template.*文件
- 更新后下载at32 hal sdk的代码

```bash
pkgs --update
scons -j16
```

- 替换rtthread内文件
  - 运行 .\script\prepare.bat

- 修改launch.json
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Cortex Debug",
            "cwd": "${workspaceFolder}",
            "executable": "build/rtthread.elf",  //elf路径
            "request": "launch",
            "type": "cortex-debug",
            "runToEntryPoint": "Reset_Handler",
            "servertype": "pyocd",
            "armToolchainPath": "D:/tools/rtt_env/env-windows-v2.0.0/env-windows/tools/bin/../../tools/gnu_gcc/arm_gcc/mingw/bin",
            "toolchainPrefix": "arm-none-eabi",
            "targetId": "_at32f403avgt7"
        }
    ]
}
```

- 修改task.json
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "type": "shell",
            "label": "Build target files",
            "command": "scons",
            "args": [
                "-j12"
            ],
            "problemMatcher": [
                "$gcc"
            ],
            "group": "build"
        },
        {
            "type": "shell",
            "label": "Download code to flash memory",
            "command": "python",
            "args": [
                "-m",
                "pyocd",
                "flash",
                "--erase",
                "chip",
                "--target",
                "_at32f403avgt7",
                "build/rtthread.elf"    //elf路径
            ],
            "problemMatcher": [
                "$gcc"
            ],
            "group": "build"
        },
        {
            "type": "shell",
            "label": "Build and Download",
            "command": "python",
            "args": [
                "-m",
                "pyocd",
                "flash",
                "--erase",
                "chip",
                "--target",
                "_at32f403avgt7",
                "build/rtthread.elf"    //elf路径
            ],
            "problemMatcher": [
                "$gcc"
            ],
            "group": "build",
            "dependsOn": "Build target files"
        }
    ]
}
```

- 指定烧写地址
```json
{
    "type": "shell",
    "label": "Download code to flash memory",
    "command": "python",
    "args": [
        "-m",
        "pyocd",
        "flash",
        "--erase",
        "sector",
        "--target",
        "_at32f403avgt7",
        "--base-address",
        "0x08020000",
        "build/rtthread.elf"
    ],
    "problemMatcher": [
        "$gcc"
    ],
    "group": "build"
},
```





