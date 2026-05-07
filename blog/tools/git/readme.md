# git

## Git 提交规范

本项目采用语义化 Git 提交信息，提交信息格式为：`<标签>: <描述>`

### 常用提交标签

| 标签 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat: 添加状态表自动转换功能` |
| `fix` | 修复 Bug | `fix: 修复状态切换时的内存泄漏问题` |
| `docs` | 文档更新 | `docs: 更新 README 使用示例` |
| `style` | 代码格式调整 | `style: 统一代码缩进格式` |
| `refactor` | 重构代码 | `refactor: 优化状态机管理器接口设计` |
| `perf` | 性能优化 | `perf: 减少状态切换时的内存分配次数` |
| `test` | 测试相关 | `test: 添加表驱动状态机单元测试` |
| `chore` | 构建/工具链相关 | `chore: 更新 SConscript 编译配置` |
| `ci` | CI/CD 配置 | `ci: 添加 GitHub Actions 自动化测试` |
| `revert` | 回滚提交 | `revert: 回滚 feat: 添加状态表自动转换功能` |

### 提交示例

```bash
# 添加文件到git仓库
git add . 
# 提交更改并添加提交信息
git commit -m "add ebus submodule and docs" 
# 切换到main分支
git branch -M main 
 # 添加远程仓库
git remote add origin git@github.com:letiannn/doc.git
# 推送到远程仓库
git push -u origin main 
#或者
git push --set-upstream origin main
```

## 拉去更新子仓库的

```bash
#添加子模块
git submodule add <repository_url>
#要拉取仓库的子模块
git clone --recursive <repository_url>
#如果已经克隆了主仓库但没有克隆子模块，可以使用以下命令去初始化子模块：
git submodule update --init --recursive
#所有子模块更新到最新版本
git submodule update --remote
```

## 关于子模块的使用

- 新建工程后如何添加子模块？

```bash
# 初始化git仓库
git init 
# git submodule add https://github.com/letiannn/mmgr.git blog/components/mmgr
git submodule add <url> <directory> 

```

- 修改子模块后如何更新同步到主子仓库？

```bash
#在主子模块都执行如下内容
git add .

git commit -m "update ebus"

git push
```


## 代码恢复

```bash
git reflog  # 查看git操作记录

git reset --soft HEAD~1 # 撤销一个commit (保留修改)
git reset --hard HEAD~1 # 撤销一个commit (丢弃修改)
```


## 其他文件

[.gitxxxx文件介绍](https://geek-docs.com/git/git-questions/154_git_what_are_all_available_git_special_files_that_can_be_committed_to_a_repository.html)

- .gitattributes
文件用于配置特定文件或路径的属性，如换行符的处理、文件形式的修改等。该文件中可以定义一系列的属性，以控制 Git 在提交和合并文件时进行特定的操作。

```
*.c linguist-language=C
*.C linguist-language=C
*.h linguist-language=C
*.H linguist-language=C

* text=auto

*.S text
*.asm text
*.c text
*.cc text
*.cpp text
*.cxx text
*.h text
*.htm text
*.html text
*.in text
*.ld text
*.m4 text
*.mak text
*.mk text
*.py text
*.rb text
*.s text
*.sct text
*.sh text
*.txt text
*.xml text
SConscript text
Makefile text
AUTHORS text
COPYING text

*.LZO -text
*.Opt -text
*.Uv2 -text
*.ewp -text
*.eww -text
*.vcproj -text
*.bat -text
*.dos -text
*.icf -text
*.inf -text
*.ini -text
*.sct -text
*.xsd -text
Jamfile -text

```

- .gitignore

```
# Prerequisites
*.d

# Object files
*.o
*.ko
*.obj
*.elf

# Linker output
*.ilk
*.map
*.exp

# Precompiled Headers
*.gch
*.pch

# Libraries
*.lib
*.a
*.la
*.lo

# Shared objects (inc. Windows DLLs)
*.dll
*.so
*.so.*
*.dylib

# Executables
*.exe
*.out
*.app
*.i*86
*.x86_64
*.hex

# Debug files
*.dSYM/
*.su
*.idb
*.pdb

# Kernel Module Compile Results
*.mod*
*.cmd
.tmp_versions/
modules.order
Module.symvers
Mkfile.old
dkms.conf

# debug information files
*.dwo

# ignore rtt sdk
/rt-thread/
/build/
__pycache__/
.sconsign.dblite

```


