# Dongle DYA Studio 迁移 + Prospector/TPS/RGB 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `dongle` 分支迁移到 ZMK 4.1 + cormoran DYA fork（Level 2），并给 dongle 新增 Prospector 屏（自定义引脚）、TPS 触控板（自定义引脚）、WS2812 电量 RGB 灯。

**Architecture:** 用同用户工作仓库 `a741725193/sofle-trackball`（dongle 分支）作为模板，保留当前 `boards/shields/velvet/` 单目录+变体 overlay 结构。Prospector 引脚用 `config/` overlay 覆盖（优先级高于 `prospector_adapter` 的 board overlay）。轨迹球从 `pixart,pmw3610-alt` 迁移到 `cormoran,pmw3610`。

**Tech Stack:** ZMK `cormoran/zmk#main+dya` + Zephyr `cormoran/zephyr#v4.1.0+zmk-fixes+nrf-half-duplex-uart`；模块 `prospector-zmk-module`、`tps43-input-module`、`zmk-battery-rgb`（空仓库，需写代码）。

**Spec:** `docs/superpowers/specs/2026-10-18-dongle-dya-migration-design.md`

## Global Constraints

- 全程 GHA 验证，不本地 `west build`（技能硬规则）。
- `zmk` 必须 `cormoran/zmk#main+dya`；zephyr 必须 cormoran fork 的 `v4.1.0+zmk-fixes+nrf-half-duplex-uart`。
- 外围（left/right）必须 `CONFIG_ZMK_USB=n`，不得有任何 `*_STUDIO_RPC`。
- 中央（dongle）是唯一 Studio 端点，含全部 `*_STUDIO_RPC`。
- 所有 `//zmk` board 变体；`settings_reset` 也用 `//zmk`。
- 轨迹球 PMW3610 迁移：compatible `pixart,pmw3610-alt` → `cormoran,pmw3610`，`force-awake` 属性移除。
- 颜色映射：线性 HSV hue 0°红→120°绿，50%≈橙黄；<20% 慢闪。
- 引脚：Prospector {SCL=0.19, SDA=0.07, DC=0.14, RES=0.03, CS=0.17, BLK=0.23}；TPS {RST=0.30, INT=0.21, SCL=0.08, SDA=0.12}；RGB 数据脚占位 P0.13。

## Review Focus

1. **board 目标**：当前三件套都是 `nice_nano_v2`，参考仓库外围用 `eyelash_nano`。若 velvet 外围实际是 `eyelash_nano` 板，`nice_nano//zmk` 会构建失败或 NFC 引脚行为错误。→ Task 2 里加注释标记，需用户确认。
2. **Prospector 引脚覆盖不生效**：`config/` overlay 未命中（命名/作用域），屏不亮且 build 仍通过。→ Task 5 用不同 pinctrl 节点名 + 覆盖 `&spi3`/`mipi_dbi`/`&pwm1`，验证 GHA 日志里 `zephyr.dts` 的引脚。
3. **PMW3610 驱动名/compatible 不对**：技能给出 `cormoran,pmw3610` + `zmk-driver-pmw3610-with-custom-studio-rpc`，需在实现时核对 cormoran 仓库确切名。→ Task 7 覆盖。
4. **RGB 灯读电量 API 签名**：`zmk_split_ble_get_peripheral_battery_level()` 需核对 cormoran 源码签名。→ Task 9 覆盖。
5. **WS2812 数据脚冲突**：占位 P0.13 需确认不与 TPS/Prospector/轨迹球引脚冲突。→ Task 9 覆盖。

---

### Task 1: 迁移 west.yml 到 cormoran fork

**Files:**
- Modify: `config/west.yml`

**Interfaces:**
- Produces: west 工作区解析出 `cormoran/zmk#main+dya` + 所有 feature 模块 + `prospector-zmk-module` + `tps43-input-module` + `zmk-battery-rgb`。

- [ ] **Step 1: 重写 `config/west.yml`**

用参考仓库 `a741725193/sofle-trackball` dongle 的 `config/west.yml` 为模板，做三处改动：
1. 轨迹球模块 `zmk-driver-paw3222-with-custom-studio-rpc` → `zmk-driver-pmw3610-with-custom-studio-rpc`（velvet 是 PMW3610；默认 remote cormoran）。
2. 保留 `zmk-board-eyelash`（remote a741725193, revision v4.0）。
3. 追加两个新模块：
   ```yaml
   - name: tps43-input-module
     remote: tokyo2006
     revision: main
     import: true
   - name: zmk-battery-rgb
     remote: tokyo2006
     revision: main
   ```

最终结构：`version: 1.2` + `defaults: {remote: cormoran, revision: main}` + remotes(cormoran/zmkfirmware/tokyo2006/a741725193/S7venYoung) + projects（zmk/zephyr + 全部 feature 模块 + zmk-board-eyelash + prospector-zmk-module + tps43-input-module + zmk-battery-rgb）+ `self: {path: config}`。zephyr `import.name-blocklist` 含 `chre`（不加 hal_silabs）。

- [ ] **Step 2: 提交**

```bash
git add config/west.yml
git commit -m "feat: migrate west.yml to cormoran ZMK 4.1 DYA fork"
```

（验证延后到 Task 2 之后的首次 push，见 Task 2 Step 2。）

---

### Task 2: 工作流 + build.yaml board 目标

**Files:**
- Modify: `.github/workflows/build.yml`
- Modify: `build.yaml`

**Interfaces:**
- Consumes: 无（独立）。
- Produces: GHA 跑 `cormoran/zmk` workflow；board 目标 `nice_nano@2.0.0//zmk`（dongle）+ `nice_nano//zmk`（外围）。

- [ ] **Step 1: 改 `.github/workflows/build.yml` 的 `uses:`**

`zmkfirmware/zmk/.github/workflows/build-user-config.yml@v0.3.0` → `cormoran/zmk/.github/workflows/build-user-config.yml@main+dya`。

- [ ] **Step 2: 改 `build.yaml`**

```yaml
include:
  - board: nice_nano@2.0.0//zmk
    shield: velvet_central_dongle prospector_adapter
    snippet: studio-rpc-usb-uart
    cmake-args: -DCONFIG_ZMK_STUDIO=y -DCONFIG_ZMK_STUDIO_LOCKING=n
    artifact-name: velvet_central_dongle
  - board: nice_nano//zmk
    shield: velvet_peripheral_left
    artifact-name: velvet_peripheral_left
  - board: nice_nano//zmk
    shield: velvet_peripheral_right
    artifact-name: velvet_peripheral_right
  - board: nice_nano//zmk
    shield: settings_reset
```

> **标记（需用户确认）**：外围若实际是 `eyelash_nano` 板（参考仓库即如此），把 left/right 的 board 改成 `eyelash_nano`。先按 `nice_nano//zmk` 提交，构建失败/行为异常再问用户。

- [ ] **Step 3: 提交并 push，读 GHA 日志确认 west update 通过**

```bash
git add .github/workflows/build.yml build.yaml
git commit -m "feat: switch build to cormoran ZMK 4.1 + //zmk board targets"
git push
```

Expected: GHA `West Update` 成功；`West Build` 阶段会因后续 shield/conf 尚未改而失败（预期，Task 3-7 逐个修）。

---

### Task 3: 清理 Kconfig.defconfig / Kconfig.shield / .zmk.yml

**Files:**
- Modify: `boards/shields/velvet/Kconfig.defconfig`
- Modify: `boards/shields/velvet/Kconfig.shield`
- Modify: `boards/shields/velvet/velvet.zmk.yml`

**Interfaces:**
- Consumes: 无。
- Produces: `SHIELD_VELVET_CENTRAL_DONGLE` 时 `ZMK_SPLIT_ROLE_CENTRAL=y`；三件套共享 `ZMK_SPLIT=y`；移除旧 SSD1306/OLED 显示配置。

- [ ] **Step 1: 重写 `Kconfig.defconfig`**

移除 `if ZMK_DISPLAY`/`if SSD1306` 整块（旧 OLED）。保留并整理：
```kconfig
if SHIELD_VELVET_CENTRAL_DONGLE
config ZMK_KEYBOARD_NAME
    default "Velvet"
config ZMK_SPLIT_ROLE_CENTRAL
    default y
endif

if SHIELD_VELVET_CENTRAL_DONGLE || SHIELD_VELVET_PERIPHERAL_LEFT || SHIELD_VELVET_PERIPHERAL_RIGHT
config ZMK_SPLIT
    default y
endif
```

- [ ] **Step 2: `Kconfig.shield` 保持三变体不变**（已正确）。

- [ ] **Step 3: 修 `velvet.zmk.yml`**

`siblings:` 改成实际 shield 名：`velvet_peripheral_left`、`velvet_peripheral_right`（现写的是 `velvet_left`/`velvet_right`）。`exposes:` 是非法键，删除；保留 `features: [keys]`。

- [ ] **Step 4: 提交**

```bash
git add boards/shields/velvet/Kconfig.defconfig boards/shields/velvet/velvet.zmk.yml
git commit -m "refactor: clean Kconfig.defconfig, fix shield siblings"
```

---

### Task 4: Dongle 中央 conf（DYA Level 2 + Prospector + TPS + RGB）

**Files:**
- Modify: `boards/shields/velvet/velvet_central_dongle.conf`

**Interfaces:**
- Consumes: Task 1 的模块（prospector/tps43/battery-rgb 等）。
- Produces: dongle 侧 Kconfig 齐全（Studio + 各 RPC + Prospector 布局 + TPS + 电量 RGB）。

- [ ] **Step 1: 重写 `velvet_central_dongle.conf`**

以参考仓库 `sofle_trackball_dongle.conf` 为模板，追加三处：
1. Prospector 布局 `CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR=y`（沿用参考）。
2. TPS：`CONFIG_ZMK_TPS43=y` + `CONFIG_ZMK_TPS43_STUDIO_RPC=y`。
3. RGB：`CONFIG_ZMK_BATTERY_RGB=y`（对应 Task 9 模块的 Kconfig 符号）。
4. 移除参考里 `CONFIG_ZMK_BATTERY_HISTORY=n`/`_STUDIO_RPC=n` 若与你需求冲突——保持 `ZMK_BATTERY_HISTORY` 关闭（dongle 无自身电池），`ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` 已由 prospector_adapter 的 Kconfig 默认开启。

其余照抄参考：`ZMK_USB/ZMK_BLE/BT_MAX_CONN=6/BT_BAS=n/ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS=2`、`ZMK_POINTING`、`MAIN_STACK_SIZE=4096/LV_Z_MEM_POOL_SIZE=49152`、全部 `*_STUDIO_RPC`（ble-management/runtime-input-processor/settings/custom-settings/fast-keymap/device-info/watchdog/sensor-rotate/runtime-macro/runtime-combo）、stack/buffer 值。

- [ ] **Step 2: 提交**

```bash
git add boards/shields/velvet/velvet_central_dongle.conf
git commit -m "feat: dongle central conf with DYA Level 2 + Prospector + TPS + RGB"
```

---

### Task 5: Dongle 中央 overlay + Prospector 引脚覆盖

**Files:**
- Modify: `boards/shields/velvet/velvet_central_dongle.overlay`
- Create: `config/velvet_central_dongle.overlay`（Prospector 引脚覆盖 + TPS I2C + WS2812 节点）

**Interfaces:**
- Consumes: Task 1 模块、prospector_adapter 的 board overlay。
- Produces: dongle devicetree 有 mock kscan、trackball split input、TPS `iqs5xx@74`、WS2812 `battery_rgb`；Prospector 用自定义引脚。

- [ ] **Step 1: 重写 `velvet_central_dongle.overlay`**

以参考仓库 `sofle_trackball_dongle.overlay` 为模板：`#include "velvet.dtsi"`、`#include "split_input_common.dtsi"`、mock kscan、`split_inputs { trackball_split@0 }`（中央侧无 device）、`trackball_listener { device = <&trackball_split>; input-processors = <&mouse_runtime_input_processor>; }`、`&kscan0 { status = "disabled"; }`。
删除旧 `nice_view_spi: &spi0`、`&uart0`、`zephyr_udc0` 块、旧的 `spi0_default/spi0_sleep` pinctrl。

- [ ] **Step 2: 创建 `config/velvet_central_dongle.overlay`**（覆盖 Prospector 引脚 + TPS + WS2812）

覆盖 prospector_adapter board overlay 的固定引脚（用不同 pinctrl 节点名避免冲突）：
```devicetree
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/dt-bindings/pwm/pwm.h>
#include <zephyr/dt-bindings/led/led.h>

/* Prospector 自定义引脚覆盖 */
&pinctrl {
    velvet_spi3_default: velvet_spi3_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 0, 19)>,
                    <NRF_PSEL(SPIM_MOSI, 0, 7)>;
        };
    };
    velvet_spi3_sleep: velvet_spi3_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 0, 19)>,
                    <NRF_PSEL(SPIM_MOSI, 0, 7)>;
            low-power-enable;
        };
    };
    velvet_pwm1_default: velvet_pwm1_default {
        group1 {
            psels = <NRF_PSEL(PWM_OUT0, 0, 23)>;
            nordic,invert;
        };
    };
    velvet_pwm1_sleep: velvet_pwm1_sleep {
        group1 {
            psels = <NRF_PSEL(PWM_OUT0, 0, 23)>;
            low-power-enable;
        };
    };
};

disp_spi: &spi3 {
    pinctrl-0 = <&velvet_spi3_default>;
    pinctrl-1 = <&velvet_spi3_sleep>;
    cs-gpios = <&gpio0 17 GPIO_ACTIVE_LOW>;
};

&pwm1 {
    pinctrl-0 = <&velvet_pwm1_default>;
    pinctrl-1 = <&velvet_pwm1_sleep>;
};

/ {
    mipi_dbi {
        dc-gpios = <&gpio0 14 GPIO_ACTIVE_HIGH>;
        reset-gpios = <&gpio0 3 GPIO_ACTIVE_LOW>;
    };

    /* TPS 触控板 I2C（SCL=0.08, SDA=0.12） */
    /* &i2cX 节点 + iqs5xx@74，见下 */

    /* WS2812 电量灯（占位 P0.13） */
    battery_rgb: battery_rgb {
        compatible = "worldsemi,ws2812-gpio";
        chain-length = <1>;
        color-mapping = <LED_COLOR_ID_GREEN LED_COLOR_ID_RED LED_COLOR_ID_BLUE>;
        gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>;
    };
};
```

TPS I2C 总线用 `i2c0`（或与 nice_nano 上未占用的 TWIM 实例一致），SCL=0.08/SDA=0.12 pinctrl 自定，节点 `iqs5xx@74`（compatible `azoteq,iqs5xx`，`reset-gpios=<&gpio0 30 GPIO_ACTIVE_LOW>`，`rdy-gpios=<&gpio0 21 GPIO_ACTIVE_HIGH>`，`one-finger-tap; two-finger-tap; scroll;`），加 `/ { tps43_input { compatible="zmk,input-listener"; device=<&tps43>; } }`。

> **实现时核对**：`config/` overlay 的命名/作用域（是否命中 `velvet_central_dongle`）；若 ZMK 只按 `.zmk.yml` `id: velvet` 匹配全局，则改为 `config/velvet.overlay` 并确保 `&spi3`/`mipi_dbi`/`&pwm1` 只存在于 dongle 构建（外围不含这些节点，全局应用无害）。用 GHA 日志的 `zephyr.dts` 段确认引脚。

- [ ] **Step 3: 提交并 push，读 GHA 日志确认 dongle 的 devicetree 引脚正确**

```bash
git add boards/shields/velvet/velvet_central_dongle.overlay config/velvet_central_dongle.overlay
git commit -m "feat: dongle overlay + Prospector custom pins + TPS + WS2812"
git push
```

Expected: GHA 日志 `zephyr.dts` 中 `spi3` pinctrl 是 P0.19/P0.07，`mipi_dbi` dc=P0.14 reset=P0.03，`cs-gpios`=P0.17，`pwm1`=P0.23。

---

### Task 6: 外围 conf（left/right 最小化）

**Files:**
- Modify: `boards/shields/velvet/velvet_peripheral_left.conf`
- Modify: `boards/shields/velvet/velvet_peripheral_right.conf`

**Interfaces:**
- Consumes: Task 1 模块。
- Produces: 外围无 `*_STUDIO_RPC`、`ZMK_USB=n`；right 侧含 PMW3610 驱动。

- [ ] **Step 1: 重写 `velvet_peripheral_left.conf`**

```kconfig
CONFIG_ZMK_SPLIT=y
CONFIG_ZMK_USB=n
CONFIG_ZMK_BLE=y
CONFIG_ZMK_SETTINGS_RPC=y
CONFIG_ZMK_SPLIT_RELAY_EVENT=y
CONFIG_SETTINGS=y
CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE=10000
CONFIG_EC11=y
CONFIG_EC11_TRIGGER_GLOBAL_THREAD=y
CONFIG_BT_CTLR_TX_PWR_PLUS_8=y
```

- [ ] **Step 2: 重写 `velvet_peripheral_right.conf`**

left 的全部 + 轨迹球 PMW3610：
```kconfig
CONFIG_ZMK_POINTING=y
CONFIG_INPUT=y
CONFIG_PMW3610=y
CONFIG_ZMK_PMW3610_CUSTOM_SETTINGS=y
```

（PMW3610 的 `_STUDIO_RPC` 只在中央，外围不加。）

- [ ] **Step 3: 提交**

```bash
git add boards/shields/velvet/velvet_peripheral_left.conf boards/shields/velvet/velvet_peripheral_right.conf
git commit -m "feat: peripheral minimal confs (USB=n, PMW3610 on right)"
```

---

### Task 7: 外围 overlay + 轨迹球迁移到 cormoran,pmw3610

**Files:**
- Modify: `boards/shields/velvet/trackball.dtsi`
- Modify: `boards/shields/velvet/split_input_common.dtsi`
- Modify: `boards/shields/velvet/velvet_peripheral_right.overlay`
- Modify: `boards/shields/velvet/velvet_peripheral_left.overlay`

**Interfaces:**
- Consumes: Task 1 的 `zmk-driver-pmw3610-with-custom-studio-rpc` + `zmk-module-runtime-input-processor`。
- Produces: right 侧轨迹球 `cormoran,pmw3610`，split input 转发；left 侧 kscan/编码器正常。

- [ ] **Step 1: 迁移 `trackball.dtsi`**

compatible `pixart,pmw3610-alt` → `cormoran,pmw3610`；移除 `#define INPUT_EV_*` 宏（改用 `<zephyr/dt-bindings/input/input-event-codes.h>`）；删 `force-awake`（如存在）；保留现有 SPI 引脚（velvet 的 spi1 pinctrl）。核对 cormoran 驱动的确切 compatible 与属性（实现时查 cormoran 仓库）。

- [ ] **Step 2: 更新 `split_input_common.dtsi`**

`trackball_listener` 的 `input-processors` 改用 `mouse_runtime_input_processor`（来自 `runtime-input-processor.dtsi`），保留 scroll 逻辑（`zip_xy_to_scroll_mapper`）。

- [ ] **Step 3: 更新 `velvet_peripheral_right.overlay`**

保留 `#include "trackball.dtsi"` + `#include "split_input_common.dtsi"`；`&trackball_split { device = <&trackball>; }` 保留；kscan 引脚保留。

- [ ] **Step 4: 更新 `velvet_peripheral_left.overlay`**

保留 `#include "velvet.dtsi"` + kscan + `&left_encoder { status="okay"; }`。移除旧 `&pwm0`（如无 backlight）。

- [ ] **Step 5: 提交并 push，读 GHA 确认 right 侧轨迹球节点正确**

```bash
git add boards/shields/velvet/trackball.dtsi boards/shields/velvet/split_input_common.dtsi boards/shields/velvet/velvet_peripheral_right.overlay boards/shields/velvet/velvet_peripheral_left.overlay
git commit -m "feat: migrate trackball to cormoran,pmw3610 + runtime input processor"
git push
```

---

### Task 8: Keymap 适配 DYA（studio_unlock / display-name / runtime combo）

**Files:**
- Modify: `boards/shields/velvet/velvet.keymap`
- Modify: `config/velvet.conf`（共享 conf）

**Interfaces:**
- Consumes: Task 1 模块（runtime-macro/runtime-combo 等）。
- Produces: keymap 有 `&studio_unlock`、各层 `display-name`；现有 `zmk,combos` 迁移到 `cormoran,runtime-combo-defaults`。

- [ ] **Step 1: 更新 `config/velvet.conf`**

移除 `CONFIG_PMW3610_ALT_SWAP_XY=y`（旧驱动符号）、`CONFIG_ZMK_RUNTIME_SENSOR_ROTATE=y` 等中央专属（已移入 dongle conf）。保留全局：`CONFIG_BT_CTLR_TX_PWR_PLUS_8=y`、消抖、`CONFIG_ZMK_POINTING=y`、`CONFIG_EC11=y`。

- [ ] **Step 2: 更新 `velvet.keymap`**

1. 在可到达层加 `&studio_unlock` 绑定（中央）。
2. 每层加 `display-name = "..."`。
3. 现有 `combos { compatible="zmk,combos"; ... }` 的 11 个 combo 迁移到 `runtime_combo_defaults { compatible="cormoran,runtime-combo-defaults"; ... }`，每个加唯一 `slot`（0-10），并 `CONFIG_ZMK_RUNTIME_COMBO_MAX_COMBOS` 提到 ≥11（放 dongle conf 或此处）。
4. 若用 `&rmacro`，加 `#include <behaviors/runtime_macro.dtsi>` 并绑 `&rmacro 0`。

- [ ] **Step 3: 提交**

```bash
git add boards/shields/velvet/velvet.keymap config/velvet.conf
git commit -m "feat: keymap DYA adaptation (studio_unlock, display-name, runtime combos)"
```

---

### Task 9: 编写 zmk-battery-rgb 模块（给用户 push 到 tokyo2006/zmk-battery-rgb）

**Files（写入本地独立目录 `~/project/zmk-battery-rgb/`，由用户 push）：**
- Create: `zephyr/module.yml`
- Create: `Kconfig`
- Create: `CMakeLists.txt`
- Create: `src/battery_rgb.c`

**Interfaces:**
- Consumes: `zmk/split/bluetooth/central.h` 的 `zmk_split_ble_get_peripheral_battery_level()`（实现时核对签名）；DT `battery_rgb` 节点（Task 5）。
- Produces: `CONFIG_ZMK_BATTERY_RGB=y` 时，低优先级线程周期读 min(左右电量) → HSV(0-120°)→RGB → `led_strip_update_rgb`，<20% 慢闪。

- [ ] **Step 1: `zephyr/module.yml`**

```yaml
name: zmk-battery-rgb
build:
  cmake: .
  kconfig: Kconfig
```

- [ ] **Step 2: `Kconfig`**

```kconfig
config ZMK_BATTERY_RGB
    bool "Battery RGB LED indicator"
    select LED_STRIP
    select WS2812_STRIP
```

- [ ] **Step 3: `CMakeLists.txt`**

```cmake
zephyr_library()
zephyr_library_sources(src/battery_rgb.c)
```

- [ ] **Step 4: `src/battery_rgb.c`**

逻辑：`DEVICE_DT_GET(DT_NODELABEL(battery_rgb))`；500ms 循环线程；每 10 tick 读 `left=zmk_split_ble_get_peripheral_battery_level(0)`、`right=...(1)`、`battery=MIN(left,right)`；HSV(hue=battery*120/100, S=1, V=1)→RGB；`battery<20` 时每 tick 翻转 on/off，否则恒亮；`led_strip_update_rgb()`。

- [ ] **Step 5: 交用户 push + 在 Task 1 的 west.yml 已引用**

写完后告诉用户：`cd ~/project/zmk-battery-rgb && git init && git add . && git commit && git remote add origin https://github.com/tokyo2006/zmk-battery-rgb.git && git push -u origin main`。提醒换真实 RGB 数据脚（占位 P0.13）。

---

### Task 10: 全量构建验证

**Files:** 无新文件。

- [ ] **Step 1: push 全部改动，读 GHA 四个 artifact 全绿**

```bash
git push
```

Expected: `velvet_central_dongle`、`velvet_peripheral_left`、`velvet_peripheral_right`、`settings_reset` 四个 `.uf2` 都产出。

- [ ] **Step 2: 逐项对照 Review Focus 检查 GHA 日志**

`zephyr.dts` 里 Prospector 引脚、TPS I2C、WS2812 节点、PMW3610 compatible 均正确。

- [ ] **Step 3: 提交最终整理（README 更新等，可选）**

---

## 已确认（2026-10-18）

1. **外围 board = `eyelash_nano`**（dongle 用 `nice_nano@2.0.0//zmk`）。`zmk-board-eyelash` revision 从 `main` 改 `v4.0`。
2. **权威 keymap = `config/velvet.keymap`**（13 层），删除 `boards/shields/velvet/velvet.keymap`（6 层残留）。因此 **保留 `zmk-tog-io`**（提供 toggle_io/on_io/off_io）。`zmk-tog-io` 需核对 4.1 兼容分支。
3. **执行方式 = Native**。
4. RGB 数据脚仍占位 P0.13，待替换。
