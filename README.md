# brlan-rate

OpenWrt 自定义包：通过 netlink 读取接口（默认 `br-lan`）的实时 RX/TX 速率（Mbps）。

## 功能

- 通过 netlink `RTM_GETLINK` + `IFLA_STATS64` 从内核读取接口 64 位字节计数（**不使用** `/proc`、`/sys` 或命令解析）。
- 两次采样差分 ÷ 时间差，计算实时速率。
- 接口不存在 / 获取失败时输出明确错误（内核返回 `ENODEV`）。

## 用法

```sh
brlan-rate              # 默认 br-lan、1s 间隔
brlan-rate br-lan 0.5   # 指定接口 + 采样间隔（秒）
brlan-rate no-such-if   # 报明确错误
```

输出示例：

```
iface=br-lan rx=1.23 Mbps tx=0.45 Mbps
iface=br-lan rx=1.18 Mbps tx=0.51 Mbps
```

## 实现原理

1. `socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)` 打开 rtnetlink socket。
2. 构造 `RTM_GETLINK` 请求，附加 `IFLA_IFNAME` 属性指定接口名。
3. 解析内核回包，取 `IFLA_STATS64` 的 `rx_bytes` / `tx_bytes`（64 位计数器，不绕回）。
4. 速率 = `(字节差分) × 8 ÷ 时间差 ÷ 1e6`，用 `CLOCK_MONOTONIC` 计时防系统时间跳变。

**为什么用 netlink 而不是解析 `/proc/net/dev` 或 `ifconfig`：**

- netlink 是内核 ABI，返回结构化数据，有明确的错误语义（接口不存在返回 `ENODEV`）；
- 不依赖 `ifconfig` / `ip` 等命令是否存在（裁剪设备上可能没有）；
- `/proc`、`/sys` 是文本接口，格式不稳定、无类型、无错误语义。

## 目录

```
brlan-rate/
├── Makefile          # OpenWrt package Makefile（四段式）
└── src/
    └── brlan-rate.c  # 单文件 C 程序
```

## 本地验证（任意 Linux，先于 OpenWrt）

```sh
gcc -O2 -Wall -o brlan-rate src/brlan-rate.c
./brlan-rate ens33 1      # 用真实网卡验证
# 另开终端：ping -f <网关IP> 造流量，观察速率变化
./brlan-rate no-such-if   # 验证错误输出
```

## 接入 OpenWrt 编译

```sh
cp -r brlan-rate <openwrt>/package/
cd <openwrt>
make menuconfig
#   Target System -> x86, Subtarget -> generic (x86_64)
#   Utilities      -> brlan-rate 设为 <*>（内置，不是 <M> 模块）
#   Target Images  -> 勾选 VMDK（或 VDI）
make -j$(nproc)
```

产物：`bin/targets/x86/64/openwrt-x86-64-generic-squashfs-combined.vmdk`
