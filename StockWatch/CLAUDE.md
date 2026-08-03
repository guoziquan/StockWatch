# 冰川 · A股行情监控 — 完整技术文档 v2.0

## 1. 项目概览

| 项目 | 值 |
|------|-----|
| 名称 | 冰川 · A股行情监控 (StockWatch) |
| 类型 | Qt 5.9.7 桌面应用 |
| 语言 | C++11 |
| 构建系统 | qmake + mingw32-make (gcc 5.3.0) |
| Qt 模块 | core, gui, network, concurrent, widgets |
| UI 方式 | 100% C++ 代码构建，无 .ui 文件 |
| 图表引擎 | QPainter 自绘（无 Qt Charts / QCustomPlot 依赖） |
| 数据源 | 东方财富公开 HTTPS/JSON API |
| 持久化 | QSettings IniFormat → `config.ini` |
| 默认自选 | `stocks.json` 兜底 + 硬编码备选 |

### 文件清单（11 个源码文件）

| 文件 | 行数 | 职责 |
|------|------|------|
| `StockWatch.pro` | 9 | qmake 工程文件 |
| `main.cpp` | 14 | 应用入口 |
| `mainwindow.h` | 108 | MainWindow 声明 + HoldingRecord/HoldingWidgets 结构体 |
| `mainwindow.cpp` | 1053 | 主窗口全功能实现 |
| `klinechart.h` | 191 | KLineData/KLineInfoPanel/KLineChart 声明 |
| `klinechart.cpp` | 1190 | 自绘K线图完整渲染引擎 |
| `stockchartwindow.h` | 44 | StockChartWindow 声明 |
| `stockchartwindow.cpp` | 264 | K线详情窗口 |
| `indicator.h` | 97 | IndicatorEngine 声明 |
| `indicator.cpp` | 487 | 技术指标计算引擎 |
| `reportgenerator.h` | 84 | ReportGenerator 声明 |
| `reportgenerator.cpp` | 524 | 每日文字报告生成器 |

---

## 2. 系统架构

```
main.cpp → QApplication
  └─ MainWindow (QMainWindow)
       ├─ 四个定时器:
       │    ├─ m_timer (5-120s): 定时拉取行情
       │    ├─ m_countdownTimer (1s): 倒计时显示
       │    ├─ m_reportTimer (60s): 午夜检测
       │    └─ QTimer::singleShot via connect: 网络回调
       ├─ QTableWidget (11列): 自选股行情一览
       ├─ StockChartWindow (QWidget + Qt::Window)
       │    ├─ KLineChart: QPainter 自绘
       │    │    ├─ 主图: K线蜡烛 + MA5/10/20/60 或 BOLL(20,2)
       │    │    ├─ 副图: 成交量 或 MACD(12,26,9) 或 KDJ(9,3,3)
       │    │    ├─ 坐标轴: 价格轴 / 副图轴 / 时间轴
       │    │    ├─ 交互: 滚轮缩放 / 拖拽平移 / 十字光标 / 分割手柄
       │    │    └─ 面板竖线: drawPanelBorder
       │    └─ KLineInfoPanel (190px): 行情数据面板
       ├─ ReportGenerator: QtConcurrent 多线程报告
       └─ IndicatorEngine: 静态纯计算
```

---

## 3. 配色体系（全局统一）

### 3.1 基础色板

| 符号 | 颜色值 | 用途 |
|------|--------|------|
| 背景主色 | `#0b1220` | MainWindow / 弹窗背景 |
| 图表背景 | `#0d1422` | KLineChart 背景 |
| 信息面板背景 | `#111a2e` | KLineInfoPanel 背景（略浅于图表） |
| 表格背景 | `#131f33` | QTableWidget 背景 |
| 表头背景 | `#1a2740` | QHeaderView 背景 |
| 输入框/按钮次背景 | `#131f33` | QLineEdit / QPushButton 次要样式 |
| 汇总头背景 | `#0f1829` | 持仓明细汇总框 |
| 边框色 | `#22334d` | 输入框/表格/面板边框 |
| 网格色 | `#162238` | K线图网格线 / 面板分隔竖线 |
| 分隔线色 | `#22334d` | 信息面板内部分割线 |
| 选中行背景 | `#3a5a8c` | 表格选中行（低饱和蓝） |
| 行间分隔 | `#1a2740` | 表格行底部 1px |

### 3.2 文字颜色

| 颜色值 | 用途 |
|--------|------|
| `#e8eefc` | 表格内容文字（主白） |
| `#c8d6e5` | 信息面板正文 |
| `#8fa3c0` | 辅助文字（轴标签、状态栏、汇总头文字） |
| `#151a2a` | 表头文字（黑色） |
| `#4da3ff` | 标题蓝色高亮 |
| `#ffffff` | 按钮内文字 |

### 3.3 涨跌颜色（中国A股惯例）

| 颜色值 | 含义 |
|--------|------|
| `#ff4d5e` (红) | 上涨 / 阳线 / 买入 |
| `#2ee6a8` (绿) | 下跌 / 阴线 / 卖出 |
| `#8fa3c0` (灰) | 平盘 / 无变化 |

### 3.4 K线图指示器颜色

| 符号 | 颜色值 | 线条 |
|------|--------|------|
| MA5 | `#ffc107` 黄 | 1.2px |
| MA10 | `#e040fb` 紫 | 1.2px |
| MA20 | `#00e5ff` 青 | 1.2px |
| MA60 | `#ff9100` 橙 | 1.2px |
| BOLL-UP | `#a04d5e` 浅红 | 1.0px |
| BOLL-MID | `#ffc107` 黄 | 1.0px |
| BOLL-DOWN | `#2ea688` 浅绿 | 1.0px |
| MACD-DIF | `#ffffff` 白 | 1.2px |
| MACD-DEA | `#ff9100` 橙 | 1.2px |
| MACD-BAR-UP | `rgba(255,77,94,180)` 半透明红 | — |
| MACD-BAR-DOWN | `rgba(46,230,168,180)` 半透明绿 | — |
| KDJ-K | `#ffc107` 黄 | 1.0px |
| KDJ-D | `#e040fb` 紫 | 1.0px |
| KDJ-J | `#00e5ff` 青 | 1.0px |
| 十字光标 | `#4d6b99` | 1px 虚线 |

---

## 4. MainWindow — 主窗口（详细设计）

### 4.1 窗口属性

| 属性 | 值 |
|------|-----|
| 标题 | `冰川 · A股行情监控` |
| 默认尺寸 | 1080 × 640 |
| 最小尺寸 | 940 × 540 |
| 背景色 | `#0b1220` |
| 布局间距 | 6px |
| 布局边距 | 14, 10, 14, 8 |

### 4.2 四区布局

```
┌──────────────────────────────────────────────────────┐
│ ① 顶部摘要栏 (40px)                                  │
│   冰川 · A股行情监控  上证 3332.26 +0.72%  深证 ... │
│                                         🟢 交易中    │
├──────────────────────────────────────────────────────┤
│ ② 操作工具栏 (44px)                                  │
│   [输入框] [添加] [刷新]  刷新间隔:[10秒▼] 下次刷新: 08秒 │
├──────────────────────────────────────────────────────┤
│ ③ 表格区域 (fill)                                    │
│   代码 │名称│最新价│涨跌幅│涨跌额│今开│最高│最低│昨收│预警│持仓 │
│   002555│三七互娱│19.91│+3.27%│+0.63│...│...│...│...│↑20│[持仓数据] │
│   002558│巨人网络│29.28│+5.78%│+1.60│...│...│...│...│—  │[＋买入]     │
├──────────────────────────────────────────────────────┤
│ ④ 底部状态栏 (24px)                                  │
│   8只自选 | 上涨5只 | 下跌3只 | 更新于 14:32:05 | ... │
└──────────────────────────────────────────────────────┘
```

### 4.3 区域① — 顶部摘要栏

```
┌────────────────────────────────────────────────────────────┐
│ [冰川 · A股行情监控]  上证 3332.26 +0.72%  深证 13578 +2.21% │
│  17px bold #4da3ff    12px RichText 标签             交易状态 │
└────────────────────────────────────────────────────────────┘
```

**指数显示**: QLabel RichText 格式，标签名 `#8fa3c0` + 数值 `#e8eefc` + 涨跌幅动态颜色

**交易状态**: `updateMarketStatus()` 根据当前时间判断：

| 条件 | 显示文字 |
|------|---------|
| 周末 | 休市(周末) · X天后周一 9:30 开盘 |
| 9:15-9:30 | 集合竞价 · 9:30 开盘 |
| 9:30-11:30 / 13:00-15:00 | 🟢 交易中 · H:MM后收盘 |
| 11:30-13:00 | ☕ 午间休市 · H:MM后开盘 |
| 15:00后 | 🔴 已收盘 · X天后 9:30 开盘 |

### 4.4 区域② — 操作工具栏

```
┌────────────────────────────────────────────────────────────────┐
│ [输入6位代码] [添加] [刷新]  刷新间隔:[10秒▼] 下次刷新: 08秒  │
│  180px        蓝色   深色     下拉可选      1秒倒计时           │
│  #131f33      #3d7bff #131f33  5/10/30/60/120秒               │
└────────────────────────────────────────────────────────────────┘
```

**按钮样式**:

| 按钮 | 背景 | 前景 | 边框 | Hover |
|------|------|------|------|-------|
| 添加 | `#3d7bff` | `#fff` | none | `#2a5fd6` |
| 刷新 | `#131f33` | `#fff` | `#22334d` 1px | `#182742` + `border:#3d7bff` |

**倒计时**: `m_countdownTimer` 每 1 秒递减 `m_countdownValue`，刷新时重置为 `m_currentIntervalSecs`

### 4.5 区域③ — 行情表格

**列定义（11 列）**:

| 索引 | 表头 | 宽度规则 | 内容格式 | 颜色 |
|------|------|---------|---------|------|
| 0 | 代码 | ResizeToContents | 6位数字 | 居中 |
| 1 | 名称 | Stretch | 股票名称 | 居中 |
| 2 | 最新价 | ResizeToContents | 右对齐 | 涨跌色 |
| 3 | 涨跌幅 | ResizeToContents | +X.XX% | 涨跌色 |
| 4 | 涨跌额 | ResizeToContents | +X.XX | 涨跌色 |
| 5 | 今开 | ResizeToContents | — | — |
| 6 | 最高 | ResizeToContents | — | — |
| 7 | 最低 | ResizeToContents | — | — |
| 8 | 昨收 | ResizeToContents | — | — |
| 9 | 预警 | ResizeToContents | ↑18.00 ↓10.00 或 — | 灰色 |
| 10 | 持仓 | StretchLastSection | QPushButton | 深色按钮 |

**表格样式**:

| 属性 | 值 |
|------|-----|
| 行高 | 32px |
| 背景 | `#131f33` |
| 表头背景 | `#1a2740` |
| 表头文字颜色 | `#151a2a`（黑色，bold） |
| 行底边框 | `1px solid #1a2740` |
| 选中行 | `background:#3a5a8c; color:#fff` |
| 网格线 | 隐藏 |

**预警列格式**: `↑18.00` (上破价) `↓10.00` (下破价)，均未设置显示 `—`

**持仓按钮**:
- 有净持仓 → 文字 `持仓数据`，颜色 `#c8d6e5`
- 无净持仓 → 文字 `＋买入`
- 样式: `background:#131f33; border:1px solid #22334d; border-radius:4px; padding:3px 10px; font-size:11px`
- Hover: `background:#182742; border-color:#3d7bff; color:#fff`

**手型光标**: 通过 `eventFilter` 安装于 `m_table->viewport()`，MouseMove 时检查 column 0/1 → `PointingHandCursor`，否则 `ArrowCursor`

### 4.6 区域④ — 底部状态栏

```
8只自选 | 上涨5只 | 下跌3只 | 更新于 14:32:05 | 双击股票查看K线，右键设置预警或删除
```

颜色: `#8fa3c0`，字号: 11px，固定高度: 24px

### 4.7 右键菜单

```
━━━━━━━━━━━━━━━━━━
🔔 设置价格预警
📊 生成今日报告
━━━━━━━━━━━━━━━━━━
💰 买入记录
━━━━━━━━━━━━━━━━━━
🗑 删除该股票
```

### 4.8 数据流

```
refresh() → fetchQuotes()
  └─ GET push2.eastmoney.com/api/qt/ulist.np/get
       params: fltt=2, secids=1.002555,0.399001,...,
               fields=f2,f3,f4,f12,f14,f15,f16,f17,f18
  └─ onReplyFinished(reply)
       ├─ 解析 JSON: data.diff[] → m_quotes
       ├─ 更新三大指数 (fmtIdx)
       ├─ renderTable()
       ├─ refreshHoldingDialogs()
       ├─ updateStatusBar()
       ├─ checkAlerts()
       └─ updateMarketStatus()
```

**东方财富字段映射**:

| 字段 | 含义 |
|------|------|
| f12 | 股票代码 |
| f14 | 股票名称 |
| f2 | 最新价 |
| f3 | 涨跌幅(%) |
| f4 | 涨跌额 |
| f17 | 今开 |
| f15 | 最高 |
| f16 | 最低 |
| f18 | 昨收 |

**secId() 规则**: 6开头 → `1.XXXXXX` (上证), 其余 → `0.XXXXXX` (深证/创业板)

### 4.9 价格预警

**设置**: 右键 → `setAlert()` → `QInputDialog` 两次（上破价/下破价，0=关闭）

**检查**: `checkAlerts()` 在每次行情刷新后运行，遍历 `m_alerts`，当 `price >= up` 或 `price <= down` 时：
- `QApplication::beep()` 响铃
- `QMessageBox::warning` 弹窗
- 5分钟内同一方向不重复触发（`m_triggered` map 记录最后触发时间戳）

### 4.10 配置持久化

**config.ini** (QSettings IniFormat):

```ini
[stocks]
1\code=002555
2\code=002558

[alerts]
[alerts\002555]
up=20.00
down=15.00

[holdings]
size=3
[holdings\1]
code=002555
isBuy=true
shares=500
price=19.20
date=2026-07-25
```

**加载降级链**: config.ini → stocks.json → 硬编码列表

---

## 5. 持仓功能

### 5.1 数据结构

```cpp
struct HoldingRecord {
    QString code;      // 股票代码
    bool    isBuy;     // true=买入, false=卖出
    int     shares;    // 股数（始终 >0，买入和卖出独立记录）
    double  price;     // 买入价或卖出价
    QDate   date;      // 交易日
};
QMap<QString, QVector<HoldingRecord>> m_holdings;
```

### 5.2 盈亏计算

```
净持仓    = Σ(买入股数) - Σ(卖出股数)
总买入成本 = Σ(买入股数 × 买入价)
总卖出收入 = Σ(卖出股数 × 卖出价)
当前市值  = 净持仓 × 当前最新价
盈亏      = 当前市值 - 总买入成本 + 总卖出收入
均价      = 总买入成本 ÷ 总买入股数
```

### 5.3 交互入口

有三种触发方式：

| 入口 | 触发 | 行为 |
|------|------|------|
| 表格按钮列 | 点击"持仓数据"/"＋买入"按钮 | `openHoldingDetail(code)` 打开/聚焦明细窗口 |
| 右键菜单 | "💰 买入记录" | `buyStock(code, name)` 直接买入 |
| 明细窗口内 | "＋买入"/"- 卖出"/"删除"按钮 | 买入/卖出/删除操作 |

### 5.4 买入流程

```
buyStock(code, name)
  ├─ QInputDialog: 买入股数 (默认100, 最小1)
  ├─ QInputDialog: 每股价格 (默认当前最新价)
  ├─ m_holdings[code].append({code, true, shares, price, today})
  ├─ saveConfig()
  ├─ renderTable()           // 更新主表按钮文字
  └─ refreshHoldingDialog()  // 如果明细窗口开着，就地刷新
```

### 5.5 卖出流程

```
sellStock(code, name)
  ├─ 检查 netShares(code) > 0, 否则提示"无法卖出"
  ├─ QInputDialog: 卖出股数 (默认=当前净持仓, 范围1~净持仓)
  ├─ QInputDialog: 卖出价格 (默认当前最新价)
  ├─ m_holdings[code].append({code, false, shares, price, today})
  ├─ saveConfig() + renderTable() + refreshHoldingDialog()
```

### 5.6 持仓明细窗口

**窗口属性**:

| 属性 | 值 |
|------|-----|
| 标题 | `XXX (002555) — 持仓明细` |
| 默认尺寸 | 620 × 460 |
| 最小尺寸 | 520 × 360 |
| 背景 | `#0b1220` |
| WA_DeleteOnClose | 是 |

**布局结构**:

```
┌──────────────────────────────────────────────┐
│ 汇总头部 (QFrame, 52px)                       │
│ ┌──────────────────────────────────────────┐ │
│ │ 净持仓: 600 股 │ 均价: 19.05 │ ...       │ │
│ │ 盈亏  +706.00          (14px bold 红/绿) │ │
│ └──────────────────────────────────────────┘ │
├──────────────────────────────────────────────┤
│ 交易记录表格 (QTableWidget, 5列)              │
│ 日期 │操作│股数│价格│金额                      │
│ 25-07-26│买入│500│19.20│+9,600.00             │
│ 蓝色"买入"=买入 / 红色"买入"=卖出 (实际已修复) │
├──────────────────────────────────────────────┤
│ [＋ 买入]  [- 卖出]                [删除]     │
│  蓝色      深色                   红色文字    │
└──────────────────────────────────────────────┘
```

**表格列**:

| 索引 | 表头 | 宽度 | 对齐 | 颜色 |
|------|------|------|------|------|
| 0 | 日期 | 110px (Interactive) | 居中 | 灰色 `#8fa3c0` |
| 1 | 操作 | 50px (Interactive) | 居中 | 买入绿/卖出红 |
| 2 | 股数 | 60px (Interactive) | 右对齐 | 买入绿/卖出红 |
| 3 | 价格 | 70px (Interactive) | 右对齐 | 买入绿/卖出红 |
| 4 | 金额 | Stretch | 右对齐 | 买入绿(+前缀)/卖出红(-前缀) |

**删除逻辑**: 选中行 → 点"删除" → 确认框 → `removeAt(row)` → 若为空则 `remove(code)` → saveConfig → renderTable → refreshHoldingDialog。**窗口不关闭**。

**刷新机制**: 明细窗口创建一次，后续所有数据变化通过 `refreshHoldingDialog(code)` 就地更新 QLabel 文字和 QTableWidget 内容，不关闭窗口也不闪烁。

**窗口位置保持**: 更新时不需要保持位置（因为窗口没有被关闭再重建，始终在原位）。

---

## 6. StockChartWindow — K线详情窗口

### 6.1 窗口属性

| 属性 | 值 |
|------|-----|
| 类型 | QWidget + Qt::Window 标志 |
| 标题 | `XXX(002555) - K线图` |
| 默认尺寸 | 1120 × 700 |
| 最小尺寸 | 860 × 500 |
| 背景 | `#0d1422` |
| WA_DeleteOnClose | 是 |

### 6.2 布局结构

```
┌──────────────────────────────────────────────────┐
│ 顶部工具栏 (QWidget, fixed 42px)                  │
│ [XXX (002555)]  [分时▼] [MA▼] [成交量▼] [刷新]   │
│  bold #4da3ff    72px    62px   72px              │
├──────────────────────────────┬───────────────────┤
│ KLineChart (stretch=1)       │ KLineInfoPanel    │
│  ┌─ 主图 (K线+均线/BOLL) ─┐ │  (fixed 190px)   │
│  │                         │ │  bg:#111a2e       │
│  ├── 分割手柄 (6px) ──────┤ │                   │
│  │ 副图(成交量/MACD/KDJ)   │ │  行情数据         │
│  │                         │ │  ──────           │
│  ├── 时间轴 (26px) ───────┤ │  日期 / 收盘 /    │
│  └─ 面板竖线贯穿全高 ─────┘ │  涨跌 / 涨幅 /    │
│                             │  开盘 / 最高 /    │
│                             │  最低 / 昨收 /    │
│                             │  成交量 / 成交额   │
├──────────────────────────────┴───────────────────┤
│ 底部状态栏 (24px): 数据量:500条 | ...             │
└──────────────────────────────────────────────────┘
```

### 6.3 工具栏控件

| 控件 | 选项 | 默认索引 |
|------|------|---------|
| 周期 | 分时 | 60分钟 | **日K** | 周K | 月K | 2 |
| 主图指标 | **MA** | BOLL | 无 | 0 |
| 副图指标 | **成交量** | MACD | KDJ | 0 |

- 周期切换触发 `fetchKLine()` 重新拉取数据
- 指标切换调用 `m_chart->setMainIndicator/setSubIndicator()` 仅重绘

### 6.4 K线数据 API

```
GET push2his.eastmoney.com/api/qt/stock/kline/get
  secid=0.002555  (6开头→1., 其余→0.)
  klt=101          (5=5分钟, 60=60分钟, 101=日K, 102=周K, 103=月K)
  fqt=1            (前复权)
  lmt=500          (日K取500条)
  fields2=f51~f61  (日期/开/收/高/低/量/额/振幅/涨跌幅/换手率)
  end=20500101
```

---

## 7. KLineChart — 自绘K线图表

### 7.1 布局常量与区域

| 常量 | 值 | 说明 |
|------|-----|------|
| L_MARGIN | 10 | 左侧留白 |
| R_PADDING | 12 | 轴标签右侧到面板竖线间距 |
| T_MARGIN | 8 | 顶部留白（均线图例） |
| B_MARGIN | 4 | 底部留白 |
| AXIS_W | 56 | 价格/量坐标轴宽度 |
| TIME_H | 26 | 时间轴高度 |
| SPLIT_HANDLE_H | 6 | 分割手柄拖拽热区 |
| MIN_MAIN_H | 220 | 主图最低高度 |
| MIN_SUB_H | 120 | 副图最低高度 |
| K_MIN_W | 3.0 | 最小K线宽度 |
| K_MAX_W | 14.0 | 最大K线宽度 |

**布局区域图**:

```
┌─── L_MARGIN ─┬──────────── chartW ───────────┬─ R_PADDING ─┬─ AXIS_W ─┐
│              │                               │            │          │
│              │  mainPlotRect (K线+均线/BOLL)  │ priceAxis  │          │
│  T_MARGIN    │                               │   Rect     │          │
│              ├── m_splitHandleRect (6px) ────┤            │          │
│              │  subPlotRect (成交量/MACD/KDJ) │ subAxis    │          │
│              │                               │   Rect     │          │
│              ├───────────────────────────────┤            │          │
│              │  timeAxisRect (26px)           │ panelBorder│          │
│  B_MARGIN    │                               │  (贯穿竖线) │          │
└──────────────┴───────────────────────────────┴────────────┴──────────┘
```

**chartW 计算公式**: `width() - L_MARGIN - R_PADDING - AXIS_W`

**主图高度**: `qMax(MIN_MAIN_H, totalChartH × m_splitRatio)`，副图 = totalChartH - mainH

**面板竖线** (drawPanelBorder): `x = priceAxisRect.right()`, `y = mainPlotRect.top() ~ timeAxisRect.bottom()`, 颜色 C_GRID(`#162238`)，1px

### 7.2 绘制调度顺序

```
drawChart():
  1. drawMainGrid()        // 主图虚线网格
  2. drawCandles()         // K线蜡烛
  3. drawMovingAverages()  // 或 drawBollinger()
  4. drawVolumeBars()      // 或 drawMACD() 或 drawKDJ()
  5. drawTimeAxis()        // 底部时间标签
  6. drawSepLine()         // 主/副/时间区水平分隔线
  7. drawPanelBorder()     // 面板右侧贯穿竖线
  8. drawCrosshair()       // 十字光标（仅在悬停时）
  9. drawPriceAxis()       // 价格轴文字（最后绘制, 避免被覆盖）
  10. drawSubAxis()        // 副图轴文字（最后绘制）
```

### 7.3 K线绘制细节

- 半宽: `max(0.5, candleW × 0.38)`
- 实体宽: `candleW × 0.76`
- 影线: 1px 实线，QPen
- 实体: fillRect，十字星最低高度 1px
- 颜色: close≥open → C_UP(`#ff4d5e`)，否则 C_DOWN(`#2ee6a8`)

### 7.4 指示器绘制

**MA（均线）**: 缓存序列 m_ma5/m_ma10/m_ma20/m_ma60，QPainterPath 折线，左上角图例

**BOLL**: 三线（上轨浅红 / 中轨黄 / 下轨浅绿），价格范围扩展考虑布林带极值

**MACD**: DIF 白线 + DEA 橙线 + 半透明红绿柱，零线参考虚线，Y轴自适应

**KDJ**: K 黄 / D 紫 / J 青 三线，20/50/80 参考虚线，Y轴默认 0~100 自适应

**成交量**: 半透明柱(alpha=140)，涨跌色跟随K线

### 7.5 坐标轴

| 轴 | 区域 | 裁剪区扩展 | 格式 |
|----|------|-----------|------|
| PriceAxis | priceAxisRect | ±halfH (font) | f,2 两位小数 |
| VolumeAxis | subAxisRect | ±halfH | 万/亿 中文 |
| IndicatorAxis | subAxisRect | ±halfH | absRange<1→f,3 / <10→f,2 / else→f,1 |

### 7.6 鼠标交互

| 操作 | 事件 | 行为 |
|------|------|------|
| 滚轮缩放 | wheelEvent | candleW ± 0.5, clamp[3.0, 14.0] |
| 拖拽平移 | mousePress + mouseMove | 修改 m_scrollOffset |
| 分割拖拽 | mousePress on handle | m_splitDragging → 调 m_splitRatio(0.2~0.85) → emit splitRatioChanged |
| 十字光标 | mouseMove in chart | m_hoverIdx → emit hoveredChanged |
| 悬停手柄 | mouseMove | setCursor(SplitVCursor) |
| 离开 | leaveEvent | 重置光标到最新K线 |

### 7.7 刻度算法 (niceTicks)

生成 1/2/5×10ⁿ 的美观刻度:

```
range = hi - lo
rough = range / maxTicks
mag = 10^⌊ln(rough)/ln(10)⌋
step = 首个 ≥rough 的 {mag, 2mag, 5mag, 10mag}
start = ⌊lo/step⌋ × step
逐 step 递增直至 hi + step×0.5
```

---

## 8. KLineInfoPanel — 行情信息面板

### 8.1 属性

| 属性 | 值 |
|------|-----|
| 宽度 | 190px (固定) |
| 背景 | `#111a2e` |
| 文字色 | `#8fa3c0` |
| 字体 | Microsoft YaHei, 9px |
| 行高 | font.height() + 6 |
| 水平内边距 | 14px |

### 8.2 布局结构

```
行情数据                     ← 蓝色加粗 4da3ff
──────────                  ← 分隔线 #22334d

日期                 2026-07-31
收盘                    18.65  ← 涨跌色
涨跌                    +0.42  ← 涨跌色
涨幅                   +2.31%  ← 涨跌色

──────────                  ← 分隔线 #22334d

开盘：18.20                 ← 灰色标签
最高：18.90
最低：18.10
昨收：18.23

──────────                  ← 分隔线 #22334d

成交量：171.20万
成交额：3.12亿
换手率：2.35%
振  幅：3.89%
```

### 8.3 绘制要点

- 所有文字使用 `QRect(x, y, w, textH)` + `Qt::AlignVCenter`（y=行顶，非基线）
- 标签左对齐 `xL=14`，数值右对齐 `width()-vw-14`
- 分隔线在"上一行底 + 4px"位置绘制，下一行从"分隔线 + 8px"开始
- 收盘/涨跌/涨幅根据涨跌动态上色，其余统一 `C_TEXT`

---

## 9. 技术指标引擎 (IndicatorEngine)

### 9.1 单点计算 (compute)

| 方法 | 参数 | 输出 |
|------|------|------|
| calcMA | 5/10/20/60 | ma5, ma10, ma20, ma60, maArrangement |
| calcMACD | 12/26/9 | dif, dea, macdBar, goldenCross, deathCross |
| calcKDJ | 9/3/3 | k, d, j, kdjOverbought/oversold |
| calcRSI | 14 | rsi14, overbought/oversold |
| calcBollinger | 20/2 | bollMid/Up/Down, bandwidth |
| calcVolume | 5 | volMa5, volRatio, volumeDesc |
| calcSupport | 20 | support/resistance |

### 9.2 序列计算 (供图表绘制)

| 方法 | 输出 | 前缀处理 |
|------|------|---------|
| bollSeries | up/mid/down | i<19 → 0 |
| macdSeries | dif/dea/bar | SMA初始化EMA，bar[0]=0 |
| kdjSeries | k/d/j | i<8 → 0, 初始K=D=50 |

---

## 10. 业务功能清单

| 功能 | 触发方式 | 实现位置 |
|------|---------|---------|
| 添加自选 | 代码框回车 / 添加按钮 | addStock() |
| 删除自选 | 右键 → 删除 | removeStock() |
| 定时刷新行情 | 自动(可调5~120s) | m_timer → refresh → fetchQuotes |
| 手动刷新 | 刷新按钮 | refresh → fetchQuotes |
| 查看K线 | 双击表格行 / 代码/名称列 | onTableDoubleClicked → StockChartWindow |
| 设置价格预警 | 右键 → 设置价格预警 | setAlert() |
| 预警触发 | 行情刷新后自动检测 | checkAlerts() → QMessageBox + beep |
| 买入记录 | 右键 / 按钮 / 明细窗口 | buyStock() |
| 卖出记录 | 明细窗口按钮 | sellStock() |
| 持仓明细 | 表格"持仓数据"按钮 | openHoldingDetail() |
| 删除持仓 | 明细窗口选中行 → 删除 | lambda in openHoldingDetail |
| 持仓窗口实时刷新 | 行情刷新/操作后自动 | refreshHoldingDialog() |
| 生成每日报告 | 右键 / 午夜自动 | generateReports() → ReportGenerator |
| 配置持久化 | 操作后自动 | saveConfig() |
| 配置恢复 | 启动时 | loadConfig() |
| K线缩放 | 滚轮 | wheelEvent |
| K线平移 | 拖拽 | mousePressEvent + mouseMoveEvent |
| K线十字光标 | 鼠标移动 | mouseMoveEvent → hoveredChanged |
| 主副图比例调节 | 拖拽手柄 | m_splitDragging |
| 周期切换(日/周/月K) | K线窗口下拉 | onPeriodChanged |
| 主图指标切换(MA/BOLL/无) | K线窗口下拉 | onMainIndicatorChanged |
| 副图指标切换(成交量/MACD/KDJ) | K线窗口下拉 | onSubIndicatorChanged |
| 表格手型光标 | 悬停代码/名称列 | eventFilter |
| 刷新倒计时 | 1秒自动 | m_countdownTimer |

---

## 11. 字体一览

| 场景 | 字体 | 大小 |
|------|------|------|
| K线图轴 | Consolas | 8px |
| K线图时间轴 | Consolas | 7px |
| K线图图例 | Microsoft YaHei | 7px |
| 信息面板 | Microsoft YaHei | 9px |
| 信息面板标题 | Microsoft YaHei | 9px bold |
| 表格内容 | 系统默认 | 13px |
| 表头 | 系统默认 | 12px bold |
| 持仓明细表 | 系统默认 | 12px |
| 持仓明细表头 | 系统默认 | 11px |
| 状态栏 | 系统默认 | 11px |
| 顶部标题 | 系统默认 | 17px bold |
| 指数标签 | 系统默认 | 12px |
| 按钮 | 系统默认 | 13px / 12px / 11px |
