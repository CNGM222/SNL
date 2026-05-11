# SNL Compiler

一个 SNL 语言编译器，包含以下模块：

1. 词法分析：输入 SNL 源文件，输出 Token 序列。
2. 语法分析（递归下降）：输入 Token 序列，输出语法错误和语法树（AST 形式）。
3. 语法分析（LL(1)）：输入 Token 序列，输出语法错误和 LL(1) 语法树。
4. 语义分析：对标识符、作用域、类型、过程调用参数等进行检查，输出语义错误。
5. 目标代码生成：生成 MIPS 汇编，目标是可在 `Mars for Compile 2022.jar` 中运行。

## 目录

- `src/app/main.cpp`：程序入口（main）
- `src/common/shared.hpp`：共享数据结构（Token/AST/诊断等）
- `src/lexical/`：词法分析模块（Token 扫描与 Token 序列输出）
- `src/syntax/`：语法分析模块（递归下降 + LL(1)）
- `src/semantic/`：语义分析（符号表、类型检查、作用域）
- `src/codegen/`：目标代码生成（MIPS）
- `src/driver/`：命令行参数解析与整体编译流程编排
- `CMakeLists.txt`：CMake 构建脚本

## 用法

```bash
snlc --input test.snl --output test.asm --mode all
```

`--mode` 可选值：

- `lex`：仅词法分析
- `rd`：词法 + 递归下降语法分析
- `ll1`：词法 + LL(1) 语法分析
- `sem`：词法 + 递归下降 + 语义分析
- `all`：全部流程，并尝试生成 MIPS

## 输出文件

假设输入文件是 `demo.snl`，会生成：

- `demo.tokens.txt`：Token 序列
- `demo.rd_tree.txt`：递归下降语法树
- `demo.ll1_tree.txt`：LL(1) 语法树
- `demo.symbols.txt`：语义分析阶段导出的符号表
- `demo.errors.txt`：词法/语法/语义/代码生成错误信息
- `demo.asm`：MIPS 汇编（仅在 `all` 且无阻塞错误时生成）

## 说明

- 过程调用支持值参和变参（`var`）
- 支持过程递归、互递归、嵌套递归调用
- 支持数组和记录类型的语义检查与地址计算
- 递归下降与 LL(1) 都会输出独立语法树和错误信息
- 若有错误，优先看 `*.errors.txt`

## 递归测试

仓库内置了递归相关用例：

- `tests/recursive_direct.snl`：直接递归
- `tests/recursive_mutual.snl`：互递归
- `tests/recursive_nested.snl`：嵌套递归（验证静态链）

执行测试脚本：

```powershell
.\tests\run_recursive_tests.ps1
```

如需指定编译器路径：

```powershell
.\tests\run_recursive_tests.ps1 -CompilerPath ".\build\Release\snlc.exe"
```

## 当前支持的数据结构（SNL）

当前实现支持以下类型/结构：

1. 基本类型：`integer`、`char`
2. 数组：`array[low..high] of <baseType>`
3. 记录：`record ... end`
4. 类型别名：`type`（可为上述类型起别名）

支持的复合访问形式：

1. 数组下标访问：`a[i]`
2. 记录字段访问：`r.field`
3. 记录中的数组字段访问：`r.field[i]`

当前不支持 `list`、`map`、`set` 等 STL 风格容器。
