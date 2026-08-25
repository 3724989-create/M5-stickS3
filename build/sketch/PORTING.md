#line 1 "D:\\M5stick_proj\\self_workspace\\PORTING.md"
# 移植指南：Agent-Status 桥接系统换设备部署

> 目标：把「Cline ↔ M5Stick 状态同步 + KEY1 命令批准」这套系统搬到
> 另一台电脑 / 另一个 M5 / 全新环境上正常使用。
> 配套文档：`C:\Users\MECHREVO\Documents\Cline\Rules\agent-status-bridge.md`

---

## 1. 系统架构（3 层，缺一不可）

```
Cline hooks 事件 → PowerShell(.ps1) → agent_bridge.py → M5 WebServer POST /api/agent_status → 屏幕
```

| 层 | 文件 | 作用 | 可移植性 |
|---|---|---|---|
| M5 固件 | `self_workspace.ino` + `web.cpp/h` + `display.cpp/h` + `globals.h` | 屏幕显示状态、`/api/agent_status`、KEY1 批准 | 换硬件要改 |
| 桥脚本 | `agent_bridge.py`（纯标准库 urllib/json） | 推状态、`--wait-approval` 轮询 KEY1 | 任何 Python 3.x 都能跑 |
| Cline hooks | `Documents\Cline\Hooks\*.ps1`（12 个） | 事件→状态映射 + 命令闸门 | 换电脑/系统要改 |

**API 契约（固件侧，任何 ESP32 都通用）**：
- `POST /api/agent_status` body `{"state":"thinking|await_approval|idle"}` → `{"ok":true}`
- `GET  /api/agent_status` → `{"state":...,"approved":bool}`（approved 一次性，读走即清）
- KEY1 按钮 → `webApprove()` 置 approved=true

---

## 2. 全部硬编码点（移植时要改的东西）

| 项 | 位置 | 新设备怎么改 |
|---|---|---|
| **M5 IP** | `agent_bridge.py` 里 `DEFAULT_URL = "http://192.168.1.128/api/agent_status"` | 用环境变量 `M5_URL` 覆盖，或固件改静态 IP / mDNS |
| **Python 全路径** | **所有 12 个 .ps1** 里的 `C:\Users\MECHREVO\AppData\Local\Programs\Python\Python39\python.exe` | 换成新电脑的 Python 路径；`python` 是 Windows Store 假 stub，别直接用 |
| **桥脚本路径** | 所有 .ps1 里的 `C:\Users\MECHREVO\.clinerules\agent_bridge.py` | 换成新电脑上 agent_bridge.py 的实际位置 |
| **marker 路径** | 部分 .ps1 写死 `C:\Users\MECHREVO\AppData\Local\Temp\...` | 统一改成 `$env:TEMP`（有的脚本已经这么做了） |
| **WiFi 配置** | 固件 NVS（`config.h` 的 Preferences），通过串口命令配置 | 新 M5 要重新配 WiFi/密码 |
| **屏幕/按钮** | `display.cpp`（135x240 布局）+ `self_workspace.ino`（BtnA/BtnB） | 换 M5 型号要改 |

---

## 3. 场景 A：同一台 M5，换新电脑（最常见）

新电脑装 Cline 4.1.x + Python 3（任意 3.x，agent_bridge.py 只用标准库），然后：

1. **拷贝桥脚本**：`agent_bridge.py` → 新电脑任意目录（如 `%USERPROFILE%\.clinerules\`）
2. **拷贝 hooks**：把 `Documents\Cline\Hooks\` 里 12 个 `.ps1` 拷到新电脑的
   `%USERPROFILE%\Documents\Cline\Hooks\`（Cline 4.1.x 只认这个目录，不认 hooks.json）
3. **批量替换路径**：在所有 .ps1 里把
   - `C:\Users\MECHREVO\AppData\Local\Programs\Python\Python39\python.exe` → 新电脑 Python 全路径
   - `C:\Users\MECHREVO\.clinerules\agent_bridge.py` → 新电脑上桥脚本的路径
   （如果 M5 IP 变了 → 见第 4 条）
4. **M5 IP**：设环境变量覆盖（推荐）：
   ```powershell
   setx M5_URL "http://<M5的新IP>/api/agent_status"
   ```
   或确认 IP 没变就用默认值。IP 会变（DHCP）→ 最稳的是固件设静态 IP（见第 5 节）
5. **Cline 设置**：Auto-Approve 打开 "Run Commands"（否则 KEY1 过了 M5 闸门，Cline 还弹自己的确认框 = 双重批准）
6. **重载 VS Code 窗口**（改动 hooks 后缓存要刷新），新建任务测试

---

## 4. 场景 B：同一台电脑，换新 M5

1. **编译上传固件**：`self_workspace.ino`（注意型号：当前代码按 StickS3 / StickC Plus 的 135x240 屏写的）
2. **配 WiFi**：用串口命令把新 M5 的 SSID/密码写进 NVS（serial.cpp 的配置命令）
3. **查 IP**：串口输出里看 DHCP 分到的 IP，或路由器后台
4. **改 `M5_URL`**：环境变量指到新 IP（或用静态 IP，见下）

---

## 5. 换网络环境 / 防 IP 漂移（强烈建议）

M5 用 DHCP 每次开机 IP 可能变，桥脚本会找不到。三选一：
- **环境变量**：`setx M5_URL "http://<ip>/api/agent_status"`（桥脚本每次启动读）
- **固件静态 IP**：在 `wifi_mgr.cpp` 里固定 IP/网关/DNS
- **mDNS**：固件注册 `m5stick.local`，桥脚本用 `http://m5stick.local/api/agent_status`（最稳）

---

## 6. 场景 C：全新环境（新电脑 + 新 M5）

= 场景 A + 场景 B，顺序：先配好 M5（固件+WiFi+静态IP），再配电脑（拷贝+改路径+M5_URL+Cline 设置）。

---

## 7. 场景 D：非 Windows（Linux / macOS）

Cline 4.1.x 在 Unix 上**不认 `.ps1`**，改成扫 `<Event>` 可执行文件（无扩展名）：
- 同样的事件名：`PreToolUse`、`PostToolUse`、`UserPromptSubmit`、`TaskStart`、`TaskComplete` 等
- 把每个 .ps1 的**逻辑**改写成 shell 脚本（或直接一个 Python 包装器），chmod +x
- 桥脚本 agent_bridge.py 是跨平台的，不用改
- 命令闸门（PreToolUse 等 KEY1）逻辑照搬：推 await_approval → `agent_bridge.py --wait-approval` → 按 exit code 回 `{"cancel":true/false}`

---

## 8. 后续「零改动移植」建议（可让我实现）

把硬编码点改成环境变量/共享配置，以后换设备就不用改脚本：
1. **Python 路径**：.ps1 里读 `$env:M5_PYTHON`，没设则退回 `python`
2. **桥脚本路径**：读 `$env:M5_BRIDGE`，没设则用相对 hooks 目录的路径
3. **marker 路径**：全部统一用 `$env:TEMP`
4. **M5 静态 IP / mDNS**：固件侧一次改好
5. **最小化固件**：把峰谷电价/余额/宠物这些业务功能剥掉，只留 agent-status 核心
   （web.cpp/h + display.cpp/h + globals.h 那部分是通用的，任何 ESP32 都能跑）

需要的话我可以直接把第 1~3 条做成环境变量版，改一次以后移植就是「拷文件 + 设两个变量」。
