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
# 添加新功能
git commit -m "feat: 支持嵌套状态机"

# 修复问题
git commit -m "fix: 修复状态机销毁时内存池未释放的问题"

# 更新文档
git commit -m "docs: 补充 API 使用说明"

# 代码重构
git commit -m "refactor: 将 QsmMgr 移至独立文件"

# 性能优化
git commit -m "perf: 使用静态内存池替代动态分配"
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