# 冰川 · A股行情监控 — 完整技术文档 v4.0

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
| 数据源 | 见下方「数据源一览」 |
| 持久化 | QSettings IniFormat → `config.ini` |
| 默认自选 | `stocks.json` 兜底 + 硬编码备选 |

### 数据源一览（v4.0）

| 用途 | 主源 | 备选 | 备注 |
|------|------|------|------|
| 自选行情 | 东财 `push2` ulist.np | 腾讯 `qt.gtimg.cn`（HTTP） | 东财失败自动切腾讯 |
| K 线 | 东财 `push2his` kline | 新浪 `money.finance.sina.com.cn` | 新浪不含成交额/换手率 |
| **分时图** | **腾讯 `web.ifzq.gtimg.cn`** | — | 东财 trends2 不稳，腾讯更可靠 |
| 主力资金流 | 东财 `push2his` fflow | — | 仅日/周/月线可用 |
| 公告 | 东财 `np-anotice-stock` | — | 风险扫描用 |
| 财务数据 | 东财 `datacenter-web` | — | 现金流/利润表 |
| **所属板块** | **东财 `push2` slist** | — | 含板块代码+实时涨幅 |

> ⚠️ 实践发现：东财 `push2` 域名下部分 API 路径在个别网络环境会被重置
> （`ulist.np`/`slist` 通常可用，`kline`/`clist` 可能失败）。
> 因此分时改用腾讯、K 线保留新浪备选。

### 文件清单（29 个源码文件 + 8 个测试）

| 文件 | 行数 | 职责 |
|------|------|------|
| `StockWatch.pro` | 13 | qmake 工程文件 |
| `main.cpp` | 20 | 应用入口（单实例检测 + 托盘初始化） |
| `mainwindow.h` | 128 | MainWindow 声明 + HoldingWidgets 结构体 |
| `mainwindow.cpp` | 1555 | 主窗口全功能实现（表格/预警/持仓/托盘/板块入口） |
| `klinechart.h` | 181 | KLineData/FlowPoint/KLineChart 声明 |
| `klinechart.cpp` | 1507 | 自绘K线图渲染引擎（含资金流副图、量能均线、十字光标浮标） |
| `stockchartwindow.h` | 54 | StockChartWindow 声明 |
| `stockchartwindow.cpp` | 571 | K线窗口（K线/分时切换、三源容错、窗口几何记忆） |
| `indicator.h` | 85 | IndicatorEngine 声明 |
| `indicator.cpp` | 413 | 技术指标计算引擎 |
| `reportgenerator.h` | 83 | ReportGenerator 声明 |
| `reportgenerator.cpp` | 494 | 每日文字报告生成器 |
| `riskscanner.h` | 93 | RiskScanner 声明（财务造假/监管风险扫描） |
| `riskscanner.cpp` | 593 | 公告关键词匹配 + 审计意见 + 盈利质量红旗 |
| `autoclosemsg.h` | 29 | AutoCloseMessageBox 声明 |
| `autoclosemsg.cpp` | 48 | 自动关闭 QMessageBox（倒计时关闭） |
| `holdingcalc.h` | 84 | 持仓账本计算（移动加权平均 + 费用模型） |
| `holdingcalc.cpp` | 138 | 已实现/浮动盈亏分离 |
| `tradingcalendar.h` | 52 | A股交易日历（含法定节假日） |
| `tradingcalendar.cpp` | 199 | 2025-2027 休市安排 |
| `pricelimit.h` | 76 | **A股涨跌停规则引擎** 声明 |
| `pricelimit.cpp` | 152 | 板块×风险状态 → 涨跌幅限制、涨停/跌停价 |
| `timesharedata.h` | 60 | **分时数据模型**（240格横轴映射） |
| `timesharedata.cpp` | 155 | 东财/腾讯两种分时格式解析 |
| `timesharechart.h` | 79 | **分时图控件** 声明 |
| `timesharechart.cpp` | 446 | 分时图自绘（价格线/均价线/量柱/整数百分比轴） |
| `sectordata.h` | 82 | **板块联动模型**（相对强度） |
| `sectordata.cpp` | 170 | 板块分类、主板块选择、强度判定 |
| `sectordialog.h` | 36 | **板块分析窗口** 声明 |
| `sectordialog.cpp` | 174 | 板块列表 + 相对强度结论展示 |

测试程序（不参与主工程构建）：

| 文件 | 验证内容 | 用例数 |
|------|---------|--------|
| `test_holdingcalc.cpp` | 移动加权平均、费用模型、盈亏分离 | 43 |
| `test_tradingcalendar.cpp` | 节假日、交易日推算、盘中阶段 | 36 |
| `test_pricelimit.cpp` | 板块识别、涨跌停价、浮点舍入 | 44 |
| `test_timesharedata.cpp` | 槽位映射、双格式解析、累计量转换 | 54 |
| `test_sectordata.cpp` | 板块分类、主板块选择、相对强度 | 44 |
| `test_riskscanner.cpp` | 审计意见、扣非占比、误报防护 | 51 |
| `test_grab.cpp` | 分时图离屏渲染（视觉验证工具） | — |
| `test_sector_ui.cpp` | 板块窗口离屏渲染（视觉验证工具） | — |

> 合计 **272 项**单元测试。编译运行方式见第 19 节。

---

## 2. 系统架构

```
main.cpp → QApplication
  ├─ 单实例检测: QSharedMemory("StockWatch_SingleInstance_Key")
  ├─ setQuitOnLastWindowClosed(false): 关窗不退出（托盘常驻）
  └─ MainWindow (QMainWindow)
       ├─ 四个定时器:
       │    ├─ m_timer (5-120s): 定时拉取行情
       │    ├─ m_countdownTimer (1s): 倒计时显示
       │    ├─ m_reportTimer (60s): 午夜检测
       │    └─ QTimer::singleShot via connect: 网络回调
       ├─ QTableWidget (11列): 自选股行情一览
       ├─ 系统托盘 QSystemTrayIcon + 托盘菜单（显示主窗口 / 退出）
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

### 数据源容错链

**主窗口行情**（`fetchQuotes` → 失败 → `fetchTencentQuotes` → 再失败 → 重试退避）:

```
东方财富 (HTTPS/JSON, ulist.np/get)
  └─ 失败/SSL错误/超时
       └─ 腾讯 (HTTP, qt.gtimg.cn, 免 SSL 绕过 DNS 劫持)
            └─ 也失败 → 回退东方财富重试 (3s/8s/20s 退避, 最多3次)
```

**K线窗口**（`fetchKLine` → 失败 → `fetchSinaKLine`）:

```
东方财富 (push2his.kline/get)
  └─ 失败/空数据 → 新浪 (CN_MarketData.getKLineData, JSON)
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

**交易状态**: 由 `TradingCalendar::describe()` 判断（见第 13 节）：

| 条件 | 显示文字 |
|------|---------|
| 周末 | 休市(周末) · N 天后 M月d日 9:30 开盘 |
| 法定节假日 | 休市(春节/国庆/…) · N 天后 M月d日 9:30 开盘 |
| 9:15 前 | 未开盘 · 今日 9:15 集合竞价 |
| 9:15-9:25 | 集合竞价中 · 9:25 结束 |
| 9:25-9:30 | 集合竞价结束 · H:MM 后开盘 |
| 9:30-11:30 | 🟢 交易中 · H:MM 后午休 |
| 11:30-13:00 | ☕ 午间休市 · H:MM 后开盘 |
| 13:00-15:00 | 🟢 交易中 · H:MM 后收盘 |
| 15:00 后 | 🔴 已收盘 · 明日 M月d日 9:30 开盘 |

**休市日刷新策略**：非交易日当日只补刷一次（确认数据日期），之后不再轮询，
避免在休市日制造"行情正在跳动"的错觉。盘前/午休/盘后每 5 个周期才请求一次。

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

**腾讯源备选**（`fetchTencentQuotes` → `parseTencentReply`，东财失败后启用）:

- 接口: `http://qt.gtimg.cn/q=sz002555,sh000001,...`（HTTP，免 SSL）
- 代码前缀: 6开头 → `sh`，其余 → `sz`；指数 `000001` → `sh000001`，其余 → `sz`+代码
- 返回格式: `v_sz002555="51~名称~代码~最新价~昨收~今开~..."`
- 字段索引: `[1]名称 [2]代码 [3]最新价 [4]昨收 [5]今开 [31]涨跌额 [32]涨跌幅% [33]最高 [34]最低`
- 非交易时段兜底: 最新价 ≤0 时回退为昨收，涨跌归零

**重试退避**（`m_retryCount`）: 东方财富失败 → 1秒后切腾讯；腾讯也失败 → 3s/8s/20s 退避回东财，最多 3 次；成功后 `m_retryCount = 0`

### 4.9 价格预警

**设置**: 右键 → `setAlert()` → `QInputDialog` 两次（上破价/下破价，0=关闭）

**检查**: `checkAlerts()` 在每次行情刷新后运行，遍历 `m_alerts`，当 `price >= up` 或 `price <= down` 时：
- `QApplication::beep()` 响铃
- `AutoCloseMessageBox::warn(...)` 弹窗（5 秒自动关闭）
- 5分钟内同一方向不重复触发（`m_triggered` map 记录最后触发时间戳）

### 4.11 系统托盘与单实例

**单实例检测**（[main.cpp](main.cpp)）:

- 用 `QSharedMemory("StockWatch_SingleInstance_Key")` 检测，已运行则弹提示并退出
- `QApplication::setQuitOnLastWindowClosed(false)`：关闭主窗口不退出进程（托盘常驻）

**关闭行为**（`closeEvent`）: 点击窗口关闭按钮 → `hide()` + `ev->ignore()`（最小化到托盘，不退出）

**托盘图标**（`QSystemTrayIcon`）:

- 图标: `enter_icon.ico`（缺失时回退 `SP_ComputerIcon`）
- 右键菜单: 「显示主窗口」「退出」
- 双击托盘图标: 主窗口可见则隐藏，隐藏则显示并置顶

**托盘菜单样式**: 背景 `#131f33`、文字 `#e8eefc`、边框 `#22334d`、选中 `#2a5fd6`

### 4.12 自动关闭弹窗（AutoCloseMessageBox）

`QMessageBox` 子类，标题带倒计时（如「行情获取失败 — SSL错误 (5s)」）:

- `timeoutSeconds > 0`: `open()` 窗口级模态，1 秒递减，归零自动关闭
- `timeoutSeconds == 0`: `exec()` 同步阻塞，行为同普通 QMessageBox
- 静态方法 `info()`/`warn()` 默认 5 秒自动关闭
- 全项目统一用它替代裸 `QMessageBox`，避免阻塞事件循环（用于 SSL 错误、网络错误、超时、预警触发、买入/卖出/添加自选提示等）

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

### 5.2 盈亏计算（移动加权平均法）

> **重要变更（v3.0）**：旧版采用 `盈亏 = 当前市值 - 总买入成本 + 总卖出收入`，
> 该公式不做成本结转，导致"均价"显示的是历次买入的算术均价而非剩余持仓的
> 真实成本，减仓/T 之后无法回答"我现在成本价多少"。现已改为券商标准算法，
> 实现见 [`holdingcalc.h`](holdingcalc.h) / [`holdingcalc.cpp`](holdingcalc.cpp)。

**买入**（成本基础累加，含买入费用）：
```
成本基础 += 买入股数 × 买入价 + 买入费用
净持仓   += 买入股数
持仓成本价 = 成本基础 ÷ 净持仓
```

**卖出**（按卖出前成本价结转，成本价恒定）：
```
单位成本   = 成本基础 ÷ 卖出前净持仓
实现盈亏  += 卖出股数 × 卖出价 - 卖出股数 × 单位成本 - 卖出费用
成本基础  -= 卖出股数 × 单位成本
净持仓    -= 卖出股数
```

**汇总指标**：

| 指标 | 公式 |
|------|------|
| 持仓成本价 | 成本基础 ÷ 净持仓（含买入费用，卖出后保持不变） |
| 已实现盈亏 | Σ 卖出(收入 − 结转成本 − 卖出费用) |
| 浮动盈亏 | (现价 − 成本价) × 净持仓 |
| 累计盈亏 | 已实现 + 浮动 |
| 当前市值 | 净持仓 × 现价 |

**交易费用模型**（`FeeConfig`，默认值符合 A 股现行规则）：

| 项目 | 费率 | 方向 | 备注 |
|------|------|------|------|
| 佣金 | 万 2.5 | 买卖双向 | **最低 5 元** |
| 印花税 | 0.05% | **仅卖出** | 2023-08-28 起单边征收 |
| 过户费 | 0.001% | 买卖双向 | 沪深均收 |

**边界处理**：
- 记录按日期升序规整，同日**买入优先于卖出**（保证先建仓后减仓）
- 卖出股数超过净持仓时按净持仓截断，费用按比例折算，净持仓不会为负
- 股数 ≤0 或价格 ≤0 的非法记录被过滤
- 清仓后成本基础归零，浮动盈亏归零，累计盈亏 = 已实现盈亏

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
│ 绿色"买入"=买入 / 红色"卖出"=卖出              │
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
  lmt=500          (日K取500条, 分钟线取300条)
  fields2=f51~f61  (日期/开/收/高/低/量/额/振幅/涨跌幅/换手率)
  end=20500101
```

**新浪源备选**（`fetchSinaKLine` → `onSinaReplyFinished`，东财失败/空数据后启用）:

```
GET money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData
  symbol=sz002555  (6开头→sh, 其余→sz)
  scale=240        (日K; 周K=1200, 月K=7200, 分钟线新浪不支持)
  datalen=500      (周K取250, 月K取120)
```

- 返回 JSON 数组: `[{"day":"...","open":"...","close":"...","high":"...","low":"...","volume":"..."}]`
- `volume` 是股数，>100000 时除以 100 转手；无成交额/振幅/换手率字段（置 0）
- 涨跌幅由相邻收盘价自行计算（`chgPct` 循环回填）
- 新浪不支持前复权，周K/月K 无直接支持，仅用 scale 映射近似

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
| 单实例检测 | 启动时 | main.cpp → QSharedMemory |
| 关闭到托盘 | 点击关闭按钮 | closeEvent() → hide() |
| 托盘菜单 | 托盘右键 / 双击托盘 | QSystemTrayIcon |
| 添加自选 | 代码框回车 / 添加按钮 | addStock() |
| 删除自选 | 右键 → 删除 | removeStock() |
| 定时刷新行情 | 自动(可调5~120s) | m_timer → refresh → fetchQuotes |
| 手动刷新 | 刷新按钮 | refresh → fetchQuotes |
| 腾讯源行情备选 | 东方财富失败自动 | fetchTencentQuotes() / parseTencentReply() |
| 重试退避 | 双源都失败 | m_retryCount (3s/8s/20s, 最多3次) |
| 查看K线 | 双击表格行 / 代码/名称列 | onTableDoubleClicked → StockChartWindow |
| 新浪源K线备选 | 东方财富K线失败自动 | fetchSinaKLine() / onSinaReplyFinished() |
| 设置价格预警 | 右键 → 设置价格预警 | setAlert() |
| 预警触发 | 行情刷新后自动检测 | checkAlerts() → AutoCloseMessageBox + beep |
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
| 周期切换(分时/60分钟/日/周/月K) | K线窗口下拉 | onPeriodChanged |
| **真正的分时图** | **周期选「分时」** | **TimeShareChart（240格固定横轴）** |
| 分时十字光标 | 鼠标移动 | TimeShareChart::drawCrosshair |
| 主图指标切换(MA/BOLL/无) | K线窗口下拉 | onMainIndicatorChanged |
| 副图指标切换(成交量/MACD/KDJ/主力资金) | K线窗口下拉 | onSubIndicatorChanged |
| 成交量均线(MA5/10/20)+量比 | 成交量副图 | KLineChart::drawVolumeBars |
| K线窗口几何记忆 | 关闭时自动保存 | closeEvent → config.ini[chartWindow] |
| **涨停价/跌停价显示** | **主表第9/10列** | **PriceLimitCalc::compute** |
| **封板高亮** | **价格触及涨跌停** | 实心色块 |
| **板块联动分析** | **右键 → 🧩 板块联动** | openSectorAnalysis → SectorDialog |
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

---

## 12. 交易日历模块 (TradingCalendar) — v3.0 新增

### 12.1 解决的问题

原实现仅用 `dayOfWeek() >= 6` 判断休市，**完全忽略法定节假日**。后果：

> 春节/国庆长假期间，程序仍显示"🟢 交易中"并持续轮询，
> 用户看到"更新于 HH:MM:SS"正常跳动，误以为拿到实时行情，
> 实际是上一交易日收盘价。**这是误导性最强的缺陷。**

### 12.2 接口

```cpp
namespace TradingCalendar {
    bool isTradingDay(const QDate &d);            // 交易日（排除周末+节假日）
    bool isHoliday(const QDate &d);               // 休市（周末或节假日）
    QString holidayName(const QDate &d);          // "春节"/"国庆"…，无则空
    QDate nextTradingDay(const QDate &d);         // 下一交易日
    QDate prevTradingDay(const QDate &d);         // 上一交易日
    SessionPhase phaseAt(const QDateTime &t);     // 盘中阶段
    QString describe(const QDateTime &t);         // 中文状态描述
    bool coversYear(int year);                    // 日历数据是否覆盖
}
```

### 12.3 盘中阶段 (`SessionPhase`)

| 阶段 | 时间 |
|------|------|
| `PreOpen` | < 9:15 |
| `CallAuction` | 9:15 – 9:25 |
| `CallAuctionEnd` | 9:25 – 9:30 |
| `MorningTrading` | 9:30 – 11:30 |
| `LunchBreak` | 11:30 – 13:00 |
| `AfternoonTrading` | 13:00 – 15:00 |
| `AfterClose` | ≥ 15:00 |
| `Closed` | 非交易日任意时刻 |

### 12.4 数据覆盖与维护

- 当前覆盖 **2025 / 2026 / 2027** 三年休市安排
- 未覆盖年份**退化为"仅排除周末"**（宁可少报节假日，也不要把真实交易日
  误判为休市，后者会阻止正常刷新），并在顶部状态栏追加"（节假日库未更新）"
- **维护方式**：每年 12 月中下旬交易所发布次年休市安排后，在
  [`tradingcalendar.cpp`](tradingcalendar.cpp) 的 `g_ranges[]` 中追加即可，
  同时更新 `g_lastYear`

---

## 13. 主力资金流副图 — v3.0 新增

### 13.1 数据源

复用已有的东方财富资金流接口（`reportgenerator.cpp` 早已在用，此前仅用于报告）：

```
GET push2his.eastmoney.com/api/qt/stock/fflow/daykline/get
  secid=0.002555   klt=101   lmt=0
  fields2=f51~f65
```

- 返回 `data.klines[]`，逗号分隔：`[0]日期 [1]主力净流入 [2]小单 [3]中单
  [4]大单 [5]超大单 [6]主力净占比 …`
- 主力净流入 = 超大单 + 大单

### 13.2 交互入口

K 线窗口副图下拉框新增**「主力资金」**选项（第 4 项）：

| 索引 | 副图 |
|------|------|
| 0 | 成交量 |
| 1 | MACD |
| 2 | KDJ |
| **3** | **主力资金** |

### 13.3 渲染要点

- 副图下拉选中`主力资金`时，`KLineChart::drawMoneyFlow()` 绘制红绿柱
- **红** = 主力净流入，**绿** = 主力净流出，零轴虚线
- 左上角图例显示**可见区间主力净流入合计**（亿/万单位）与统计天数
- Y 轴单独用 `drawFlowAxisContent()`（万/亿中文单位），不与 MACD/KDJ 混用
- **分钟线周期（分时/60分钟）不请求资金流**，副图提示"暂无数据"
- 通过 `setMoneyFlow(QVector<FlowPoint>)` 按日期对齐到 K 线：
  建立 `日期 → 索引` 哈希表，缺失日期按 0 处理；若完全无法匹配则清空（不显示误导性空图）
- 资金流与 K 线**并行请求**，谁先返回由另一方触发对齐渲染

---

## 14. 风险扫描器（v3.1 强化）

### 14.1 三类判定

| 类别 | 方法 | 触发条件 |
|------|------|---------|
| 公告关键词 | `matchAnnouncementKeywords` | 高危词（立案调查/行政处罚/虚假记载/退市…）或关注词（问询函/违规担保/资金占用…） |
| 审计意见 | `checkAuditOpinion` | 保留 / 否定 / 无法表示 / 带强调事项段 |
| 盈利质量 | `checkEarningsQuality` | 扣非占比过低、现金流覆盖不足 |
| 现金流红旗 | `checkCashFlowRedFlags` | 连续3期为负、最新期大额流出、营收背离 |

### 14.2 审计意见分类（`classifyAuditOpinion`）

严重程度：`Disclaimer` > `Adverse` > `Qualified` > `Emphasis` > `Standard` > `Unknown`

> **关键陷阱（已修复）**：「标准无保留意见」**包含子串**「保留意见」。
> 若按严重程度顺序直接判定，会把最健康的审计意见误判为最严重的问题。
> 因此必须**先排除「无保留意见」**，再按严重程度判定。

### 14.3 盈利质量判定

```
扣非净利润占比 = 扣非净利润 / 净利润
  < 0    → 红旗：利润依赖非经常性损益（仅在净利润>0 时判断）
  < 50%  → 红旗：主业盈利能力弱

现金流覆盖 = Σ经营现金流 / Σ净利润（最近3期，跳过亏损期）
  < 0    → 红旗：利润未转化为现金
  < 50%  → 红旗：盈利质量存疑
```

**设计要点**：亏损期不参与覆盖计算（占比无意义），净利润为 0 时不判扣非占比，
避免对亏损公司误报。

### 14.4 数据可得性（重要限制）

经真实接口核实：

| 数据 | 可得性 |
|------|--------|
| 净利润 `PARENT_NETPROFIT` | ✅ 利润表接口提供 |
| 扣非净利润 `DEDUCT_PARENT_NETPROFIT` | ✅ 利润表接口提供 |
| 经营现金流 `NETCASH_OPERATE` | ✅ 现金流接口提供 |
| **审计意见类型** | ❌ **免费接口不提供** |

审计意见的处理策略：

- 探明东财不存在 `RPT_AUDIT_OPINION` 类报表；公告标题通常只写
  「20XX年度审计报告」，**不含意见类型**，需下载 PDF 解析才能获知
- 因此 `extractAuditOpinion()` **只采纳标题中明确写出意见类型**的公告，
  并严格排除「法律意见」「核查意见」「内部控制」「审计委员会」等噪声
- **拿不到时返回空，绝不臆测为「标准无保留意见」** —— 把未知当安全是最危险的
- `RiskResult.dataGaps` 会如实告知"审计意见未获取"，UI 在无风险时也会提示

### 14.5 公告关键词误报防护（v3.2 重要）

**曾经踩过的坑**：早期直接用「标题 + 分类名」做关键词匹配，导致所有健康公司
都被判为"存在资金占用风险"。原因是东财公告的 `column_name` 是**栏目分类**
而非内容判定 ——《非经营性资金占用及其他关联资金往来情况汇总表》是每家公司
半年报/年报的**例行合规披露**，恰恰是"没有资金占用"的证明文件。

现在采用三层防护：

| 层次 | 规则 |
|------|------|
| ① 例行公告标题 | 含「汇总表/专项说明/自查报告/履职情况/法律意见书/会议决议/业绩说明会」→ 整条跳过 |
| ② 例行分类名 | `NOISE_CATEGORIES` 中的栏目（资金占用、保荐/核查意见、法律意见书…）不参与判定 |
| ③ 标题优先 | 标题命中才可信；仅由分类命中的「关注」级一律丢弃 |

关键词也做了收紧：移除过宽的 `违规`（会命中"不存在违规情形"），
`资金占用` 改为更精确的 `非经营性资金占用` / `资金被占用`。

### 14.6 告警阈值设计原则

**宁可漏报，不可误报**。误报会让用户对告警麻木，最终忽略真正的风险信号。

| 判定 | 阈值 | 理由 |
|------|------|------|
| 营收断崖 | 同比 < **-50%** | 主业真正崩塌 |
| 营收连续下滑 | 连续 3 期下降 **且** 最新 < -20% | 单期小幅下滑是正常经营波动（游戏/周期行业尤甚） |
| 现金流覆盖 | Σ现金流/Σ净利润 < 50% | 利润未转化为现金 |
| 扣非占比 | < 50% 或为负 | 利润依赖非经常性损益 |

> 实测验证：三七互娱 -14.3%、冰川网络小幅波动属正常行业周期，**不报警**；
> ST佳缘营收从 3.2 亿骤降至 0.57 亿、连续 6 期现金流为负，**正确报警**。

### 14.7 数据结构

```cpp
struct CashFlowItem {
    QString reportDate;
    double  netCashOperate;    // 经营现金流净额
    double  totalOperateIncome;// 营业总收入
    double  incomeYoY;         // 营收同比
    double  netProfit;         // 归母净利润      (v3.1)
    double  deductNetProfit;   // 扣非归母净利润  (v3.1)
    QString auditOpinion;      // 审计意见原文    (v3.1)
};

struct RiskResult {
    bool        hasRisk;
    QStringList hits;       // 公告命中
    QStringList redFlags;   // 财务红旗
    QStringList dataGaps;   // 数据缺口提示    (v3.1)
    bool        auditChecked;// 审计意见是否获取 (v3.1)
};
```

---

## 15. 涨跌停规则引擎 (PriceLimit) — v4.0 新增

### 15.1 为什么需要

看盘时若不知道涨停价/跌停价，就无法判断"现在几板"、"离涨停还有多少空间"。
而 A 股涨跌幅规则**按板块和风险警示状态差异化**，容易记错。

### 15.2 规则表

| 板块 | 代码特征 | 正常 | ST / *ST |
|------|---------|------|----------|
| 沪市主板 | 600/601/603/605 | ±10% | **±5%** |
| 深市主板 | 000/001 | ±10% | **±5%** |
| 创业板 | 300/301 | ±20% | **±20%** |
| 科创板 | 688/689 | ±20% | **±20%** |
| 北交所 | 43/83/87/920 | ±30% | ±30% |

> **最容易搞错的一条**：只有**主板**的 ST 股才缩窄到 ±5%。
> 创业板/科创板的 ST 股仍是 ±20%。
>
> 实例：ST佳缘(301117)，昨收 12.00 → 涨停 **14.40** / 跌停 **9.60**
> （12.00 × 1.2 / 0.8），已与腾讯官方数据核对一致。
> 若按"ST=5%"去算会得到 12.60/11.40，完全错误。

### 15.3 浮点舍入陷阱（已修复）

涨跌停价必须精确到分，用浮点乘法会出错：

```
16.15 × 0.9 = 14.535
IEEE754 实际存储 → 14.534999999999999964...
floor(×100 + 0.5) = 1453  →  14.53   ❌ 少一分钱
正确值 = 14.54
```

一分钱之差会直接导致"是否封板"误判。现改为**整数分运算**：

```cpp
int64_t limitCents(int64_t prevCloseCents, int limitPct, int sign) {
    const int64_t numerator = prevCloseCents * (100 + sign * limitPct);
    return (numerator + 50) / 100;   // 四舍五入到分
}
```

### 15.4 UI 呈现

主表新增 **涨停价 / 跌停价** 两列：

- 正常：涨红跌绿文字
- **封板时：实心色块**（涨停红底白字、跌停绿底白字），一眼可见
- 悬停显示规则说明（如"创业板 · ST · 涨跌幅限制 ±20%"）

---

## 16. 分时图 (TimeShare) — v4.0 新增

### 16.1 之前的问题

「分时」下拉项实际调用 `klt=5`（5 分钟 K 线），**并非真正的分时图**。

### 16.2 与 K 线的本质区别

| 维度 | K 线图 | 分时图 |
|------|--------|--------|
| 横轴 | 按数据条数 | **固定 240 格**（全天交易分钟） |
| 数据不足时 | 压缩显示 | **右侧留白**，直观看出"走到一天哪个位置" |
| 午休 | 不存在 | **不占横轴**（11:30 与 13:00 共用 slot 120） |
| 价格轴 | 按最高最低自适应 | **以昨收为对称中心**，0% 恒在正中 |

### 16.3 槽位映射

```
09:30 → slot 0      11:30 → slot 120
13:00 → slot 120    15:00 → slot 240
午休/盘前/盘后 → -1（不绘制）
```

### 16.4 绘制要素

- 价格线（白） + 均价线（黄）
- 昨收基准虚线（0.00% 位置）
- 涨跌填充（红涨绿跌，半透明）
- 成交量柱（按分钟涨跌着色）
- **左轴价格 + 右轴整数百分比**（+2% / +1% / 0% / -1% / -2%）
- 十字光标带价格/时间/数据浮标

### 16.5 两种数据源格式

| 源 | 格式 | 处理 |
|----|------|------|
| 东财 trends2 | `时间,开,收,高,低,量,额,均价` | 直接解析 |
| **腾讯 minute**（主用） | `HHmm 价格 累计量 累计额` | 需做差求每分钟量；均价 = 累计额/(累计量×100) |

> 腾讯"量"单位是手(100股)，"额"是元，算均价时量要 ×100。

### 16.6 价格范围对齐

价格范围按**整数百分比**对齐（±1%/±2%/±5%/±10%），使刻度恰好落在绘图区边缘：
既不留白、也不裁切曲线。早期版本未对齐，曾出现曲线超出绘图区被裁掉。

---

## 17. 板块联动与相对强度 (Sector) — v4.0 新增

### 17.1 为什么这是最重要的维度

A 股个股涨跌约 60% 由板块决定。同样的 +3%，含义可能完全不同：

| 情形 | 板块 | 个股 | 解读 |
|------|------|------|------|
| **领涨** | +0.5% | +6% | 有独立逻辑/资金，持续性更好 |
| **同步** | +2.6% | +3.1% | 走势由板块驱动 |
| **拖累** | +5% | +1% | 板块涨它不涨，警惕个股自身问题 |

### 17.2 入口与数据

右键 → **🧩 板块联动**，数据来自东财 `slist` 接口：

```
GET push2.eastmoney.com/api/qt/slist/get
  spt=3  secid=0.002555  fields=f12,f13,f14,f3
返回: [{f12:"BK1046", f14:"游戏Ⅱ", f3:2.62}, ...]
```

### 17.3 主板块选择规则

1. **优先行业板块**（真正代表赛道）；多个时取更细分的（游戏Ⅲ 优于 游戏Ⅱ）
2. 无行业板块 → 选名称带「概念」的概念板块
3. **地域板块永不作为主板块**（它不决定个股走势）

### 17.4 相对强度判定

相对强度 = 个股涨幅 − 主板块涨幅（百分点）

| 差值 | 判定 |
|------|------|
| ≥ +3 | 领涨 |
| +1 ~ +3 | 略强 |
| -1 ~ +1 | 同步 |
| -3 ~ -1 | 略弱 |
| ≤ -3 | 拖累 |

### 17.5 板块类型识别

| 类型 | 识别规则 |
|------|---------|
| 地域 | 名称以「板块」结尾且 ≤8 字（安徽板块） |
| 行业 | 带罗马数字后缀（游戏Ⅱ/Ⅲ）+ 白名单（传媒/电力/证券…） |
| 概念 | 其余（网络游戏、元宇宙概念…） |

> 东财部分行业板块名称不带罗马数字（传媒、电力），因此维护了行业白名单。

### 17.6 窗口设计

`SectorDialog` 独立成类（而非塞在 MainWindow 内），好处：

- MainWindow 已 1555 行，继续堆积难维护
- 可脱离主窗口**单独离屏渲染**（`test_sector_ui.cpp`），便于视觉验证

---

## 18. 算法验证（离线测试）

六个纯逻辑测试套件（**不参与主工程构建**），合计 **272 项**：

| 文件 | 验证内容 | 用例数 |
|------|---------|--------|
| `test_holdingcalc.cpp` | 移动加权平均、费用模型、盈亏分离、超卖保护 | 43 |
| `test_tradingcalendar.cpp` | 节假日判断、前后交易日推算、盘中阶段边界 | 36 |
| `test_pricelimit.cpp` | 板块识别、涨跌停价、浮点舍入边界 | 44 |
| `test_timesharedata.cpp` | 槽位映射、双格式解析、累计量转每分钟 | 54 |
| `test_sectordata.cpp` | 板块分类、主板块选择、相对强度判定 | 44 |
| `test_riskscanner.cpp` | 审计意见分类与误判防护、扣非占比、现金流覆盖 | 51 |

两个离屏渲染工具（视觉验证，非断言测试）：

| 文件 | 用途 |
|------|------|
| `test_grab.cpp` | 渲染分时图 → `_grab.png` |
| `test_sector_ui.cpp` | 渲染板块窗口 → `_sector_ui.png` |

> **为什么需要离屏渲染工具**：`PrintWindow` 对 Qt 窗口常返回全黑或旧帧，
> 无法可靠验证绘制结果。用 `QWidget::grab()` 配合 `QT_QPA_PLATFORM=offscreen`
> 才能稳定拿到真实渲染图。

**编译运行**（在工程根目录）：

```bat
set QT=F:\Qt\Qt5.9.7\5.9.7\mingw53_32
g++ -std=gnu++11 -O1 -I. -I%QT%/include/QtCore -I%QT%/include ^
    -I%QT%/mkspecs/win32-g++ ^
    test_holdingcalc.cpp holdingcalc.cpp -o t.exe -L%QT%/lib -lQt5Core
t.exe
```

需要 moc 的套件（风险扫描、板块窗口）：

```bat
moc riskscanner.h  -o release\moc_riskscanner.cpp
moc sectordialog.h -o release\moc_sectordialog.cpp
```

离屏渲染工具还需要：

```bat
set QT_QPA_PLATFORM=offscreen
set QT_QPA_FONTDIR=C:\Windows\Fonts    :: 否则文字不渲染
set QT_PLUGIN_PATH=%QT%\plugins
```

> 修改 `holdingcalc.cpp` / `tradingcalendar.cpp` / `pricelimit.cpp` /
> `timesharedata.cpp` / `sectordata.cpp` / `riskscanner.cpp` 后**务必重跑**，
> 这些文件的正确性直接决定账本数字、休市判断、涨停价、分时图与风险预警是否可信。

---

## 19. 构建与打包注意事项

### 19.1 `repackage.bat` 编码要求（重要）

该脚本是 **GBK 编码 + CRLF 换行**：

| 错误做法 | 后果 |
|---------|------|
| UTF-8 保存 | `cmd` 按 GBK 解析，中文命令被打散，脚本全程报"命令不存在" |
| LF 换行 | `cmd` 无法正确分行，整个脚本失效 |

编辑器若将其改成 UTF-8/LF，**必须改回 GBK+CRLF** 才能运行。
脚本内使用 `chcp 936`（而非 65001）。

### 19.2 OpenSSL DLL 来源

`windeployqt` 不会打包 OpenSSL。脚本按以下顺序查找：

1. `release\libeay32.dll` / `ssleay32.dll`（手工放置）
2. `%QT_DIR%\Tools\mingw530_32\opt\bin\`（**Qt 安装自带，版本匹配，推荐**）
3. `%QT_DIR%\Tools\QtCreator\bin\`

> 注意：不要从其他第三方软件目录（如 Sangfor、FlashFXP）拷贝
> `libeay32.dll`，版本与 Qt 5.9.7 不匹配会导致 HTTPS 崩溃或握手失败。
> 若确实缺失，程序会自动降级到腾讯 HTTP 源（见数据源容错链）。
