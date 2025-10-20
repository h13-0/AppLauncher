# App Launcher

一个将目标程序强制以后台方式启动的通用小工具。它固定读取与可执行文件位于同一目录的 `AppLauncher.yaml` 配置文件，解析目标可执行文件以及命令行参数，并在没有控制台窗口的情况下拉起目标进程。项目使用 C++ 实现，可静态链接运行库，在全新 Windows 环境下也能直接运行。

## 功能特点
- 通过 YAML 描述目标程序、启动参数以及可选的工作目录
- 支持相对路径，默认相对于配置文件所在目录解析
- 以隐藏窗口方式启动，避免闪过命令行窗口
- 启动失败时给出清晰的错误提示

## 配置方法
无论最终发布的程序名为何，只需在同一目录提供 `AppLauncher.yaml`。示例：

```yaml
executable: tools\my-service.exe
workingDirectory: tools
arguments:
  - --port
  - "8080"
  - --log-level=info
```

- `executable`：[必选]可执行文件路径。可以填写绝对路径，也可以填写相对路径（相对于 YAML 文件所在目录）
- `workingDirectory`：[可选]执行目标程序时所在路径。未配置时保持目标程序所在目录
- `arguments`：[可选]执行时的参数列表。支持行内写法 `[--flag, value]` 或逐行 `-` 形式

保存配置后，直接运行 `AppLauncher.exe`（或任何重命名后的可执行文件），工具都会读取 `AppLauncher.yaml` 并以后台方式启动目标程序。若找不到配置文件、可执行文件不存在或其他异常情况，会弹出消息框并退出。

### 使用 >> 记录日志
由于本程序未直接支持重定向输出日志，因此可使用如下的方法间接重定向目录。

```yaml
executable: C:\Windows\System32\cmd.exe
arguments:
 - /c
 - C:\path\target.exe >> C:\path\target.log
```

## 常见问题
- **如何快速测试？** 可将 `executable` 指向系统自带的 `notepad.exe`，添加需要的文件路径作为参数
- **需要管理员权限吗？** 不需要，只要对目标程序和工作目录有执行访问权限即可
- **可以放在任何位置吗？** 可以，把可执行文件和 `AppLauncher.yaml` 放在同一目录即可，配置中的相对路径会以该目录为起点解析

## 构建
本项目使用 Visual Studio (C++/Win32) 工程，默认生成无控制台窗口的可执行文件：

1. 打开 `AppLauncher.sln`
2. 在“生成”菜单中选择所需的目标（建议 Win32）
3. 为了在无运行库的环境下运行，工程默认使用静态 CRT (`/MT`)，编译出的 exe 可直接分发
