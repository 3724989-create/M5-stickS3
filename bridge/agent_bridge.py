#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""agent_bridge.py — Cline hooks 与 M5Stick agent-status 之间的桥。

用法:
    python agent_bridge.py <state>      # 推状态: idle | thinking | await_approval
                                        #   await_approval 可加 --timeout N（M5 倒计时秒数）
                                        #   和 --tool "命令摘要"（M5 底部显示）
    python agent_bridge.py --status     # 查询 M5 当前状态 (GET)
    python agent_bridge.py --wait-approval [--timeout N]
                                        # 轮询 M5 approved 标志等 KEY1
                                        # exit: 0=批准 1=超时未批准 2=M5离线
                                        # 超时默认 60s, 可用 M5_GATE_TIMEOUT 覆盖, 硬上限 65s
                                        # (需配 Cline 扩展超时补丁: SRu=3e4 -> SRu=8e4)

说明:
    - Cline hooks 触发时调用本脚本, 把 agent 的工作状态推到 M5 屏幕。
    - 目标地址默认 http://192.168.1.128/api/agent_status, 可用环境变量
      M5_URL 覆盖 (例如 M5_URL=http://192.168.1.200/api/agent_status)。
    - 每次调用追加一行日志到 %TEMP%\\agent_bridge.log, 方便排查 hooks 是否触发。
    - M5 关机/离线时静默失败 (只记日志), 绝不影响 Cline 任务本身。
"""
from __future__ import annotations

import json
import os
import sys
import time
import urllib.request

DEFAULT_URL = "http://192.168.1.128/api/agent_status"
VALID_STATES = ("idle", "thinking", "await_approval")


def log(msg: str) -> None:
    try:
        path = os.path.join(os.environ.get("TEMP", r"C:\Windows\Temp"), "agent_bridge.log")
        with open(path, "a", encoding="utf-8") as f:
            f.write("%s %s\n" % (time.strftime("%Y-%m-%d %H:%M:%S"), msg))
    except Exception:
        pass


def post_state(url: str, state: str, timeout=None, tool=None) -> tuple[int, str]:
    payload = {"state": state}
    if timeout is not None:
        payload["timeout"] = int(timeout)
    if tool:
        payload["tool"] = tool[:60]
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=3) as resp:
        body = resp.read().decode("utf-8")
        return resp.status, body


def get_state(url: str) -> tuple[int, str]:
    req = urllib.request.Request(url, method="GET")
    with urllib.request.urlopen(req, timeout=3) as resp:
        body = resp.read().decode("utf-8")
        return resp.status, body


def wait_approval(url: str, timeout: int) -> int:
    """Poll M5 until user presses KEY1 (approved=true) or timeout.

    Returns:
        0  approved (KEY1 pressed)
        1  M5 reachable but not approved within timeout
        2  M5 unreachable (caller should pass through, gate OFF)
    """
    deadline = time.time() + timeout
    while True:
        try:
            _status, body = get_state(url)
            data = json.loads(body)
            if isinstance(data, dict) and data.get("approved"):
                log("wait_approval approved after %.1fs" % (deadline - time.time()))
                return 0
        except Exception as exc:
            log("wait_approval GET failed: %r" % (exc,))
            return 2
        if time.time() >= deadline:
            log("wait_approval timeout (%ds), not approved" % timeout)
            return 1
        time.sleep(0.3)


def main() -> int:
    args = sys.argv[1:]
    url = os.environ.get("M5_URL", DEFAULT_URL)

    if args and args[0] == "--status":
        try:
            status, body = get_state(url)
        except Exception as exc:
            log("GET failed: %r" % (exc,))
            print("[agent_bridge] GET failed: %r" % (exc,))
            return 1
        print("[agent_bridge] GET -> %d %s" % (status, body))
        return 0

    if args and args[0] == "--wait-approval":
        timeout = 60
        try:
            i = args.index("--timeout")
            timeout = int(args[i + 1])
        except (ValueError, IndexError):
            pass
        try:
            timeout = int(os.environ.get("M5_GATE_TIMEOUT", timeout))
        except ValueError:
            pass
        timeout = max(1, min(timeout, 65))
        rc = wait_approval(url, timeout)
        if rc == 0:
            print("[agent_bridge] approval granted")
        elif rc == 2:
            print("[agent_bridge] M5 unreachable, gate OFF")
        else:
            print("[agent_bridge] approval timeout")
        return rc

    state = args[0] if args else "idle"
    if state not in VALID_STATES:
        log("invalid state %r" % (state,))
        print("[agent_bridge] invalid state: %r (expected %s)" % (state, list(VALID_STATES)))
        return 2

    timeout = None
    tool = None
    try:
        i = args.index("--timeout")
        timeout = int(args[i + 1])
    except (ValueError, IndexError):
        pass
    try:
        i = args.index("--tool")
        tool = args[i + 1]
    except (ValueError, IndexError):
        pass

    try:
        status, body = post_state(url, state, timeout=timeout, tool=tool)
    except Exception as exc:
        log("POST state=%s failed: %r" % (state, exc))
        print("[agent_bridge] POST state=%s failed: %r" % (state, exc))
        return 1

    log("POST state=%s -> %d %s" % (state, status, body))
    print("[agent_bridge] POST state=%s -> %d %s" % (state, status, body))
    return 0


if __name__ == "__main__":
    sys.exit(main())
