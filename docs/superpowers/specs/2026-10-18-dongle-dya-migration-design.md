# Dongle DYA Studio 迁移 + Prospector / TPS / RGB 设计

日期：2026-10-18
分支：`dongle`

## 1. 意图与背景

把 `dongle` 分支从 ZMK v0.3.0（Zephyr 3.5）迁移到 ZMK 4.1 + cormoran DYA fork（Level 2），并给 dongle（中央，nRF52840）新增三样硬件支持：

1. **Prospector 屏幕**（ST7789V，240×280，SPI + MIPI-DBI），自定义引脚。
2. **TPS 触控板**（Azoteq IQS5xx/TPS43，I2C），自定义引脚，含 DYA Studio 触控板 tab。
3. **一颗 WS2812 RGB 灯**，展示左右键盘电量取低值：0–100% 映射红→绿（50% 橙色），低于 20% 慢闪。

## 2. 现状

- west.yml 指向 `zmkfirmware/zmk#v0.3.0`，board 用 `nice_nano_v2`。
- dongle 中央用 `zmk-dongle-display`（englmaxi v0.3，SSD1306/SH1106 OLED）+ `zmk-nice-oled`。
- 右侧外围有 PMW3610 轨迹球（`badjeff/zmk-pmw3610-driver`，compatible `pixart,pmw3610-alt`），经 `split_inputs` 转发给 dongle。
- 三件套 shield：`velvet_central_dongle` / `velvet_peripheral_left` / `velvet_peripheral_right`。
- 构建通过 `.github/workflows/build.yml` 引用 `zmkfirmware/zmk/.github/workflows/build-user-config.yml@v0.3.0`。

## 3. 目标 / 非目标 / 反目标

**目标**
- 三件套都能在 ZMK 4.1 + cormoran fork 上通过 GHA 构建。
- dongle 点亮 Prospector 状态屏（layer / 外设电量 / BLE / 修饰键）。
- TPS 触控板在 dongle 上移动光标，DYA Studio 触控板 tab 可调参。
- RGB 灯按「min(左右电量)」红→绿渐变，<20% 慢闪。
- DYA Studio Level 2：Studio RPC + 触控板 / 设置等 tab。

**非目标**
- 不改左右手 keymap 布局与键位（除非迁移强制要求）。
- 不做 Level 3 自定义 RPC（TPS 用现成的 tokyo2006 RPC）。
- 轨迹球不新增 DYA Studio Trackball tab（仅保持可用，见 §7）。

**反目标（这样就算失败）**
- 为迁就引脚去改 dongle 的硬件接线。
- 只让本地「看起来能 build」，而不是走 GHA 验证。
- 破坏左右外围现有功能（键位、轨迹球、编码器）。

## 4. 迁移路径（ZMK 4.1 + DYA fork）

按技能 Section 11 → 4.1 → 5.2 → 5.3：

- `config/west.yml`：单层扁平，`version: 1.2` + `defaults: { remote: cormoran, revision: main }`。
  - `zmk` → `cormoran/zmk#main+dya`（`import: app/west.yml`）。
  - `zephyr` → `cormoran/zephyr#v4.1.0+zmk-fixes+nrf-half-duplex-uart`，`name-blocklist` 含 `chre` 等（不加 hal_silabs）。
  - 删除 `zmk-nice-oled`、`zmk-dongle-display`、`zmk-pmw3610-driver`、`zmk-tog-io`（旧显示/旧轨迹球驱动）。
  - 新增：`prospector-zmk-module`、`tps43-input-module`（`import: true`）、RGB 自定义模块、轨迹球新驱动 `zmk-driver-pmw3610-with-custom-studio-rpc`。
- `build.yaml`：board `nice_nano_v2` → `nice_nano//zmk`；`settings_reset` 同步改。
- `.github/workflows/build.yml`：`uses:` 改 `cormoran/zmk/.github/workflows/build-user-config.yml@main+dya`。
- 三件套 conf/overlay 按技能 4.1/5.1/5.2/5.3/8.7 重写（中央含全部 `*_STUDIO_RPC`，外围最小化、`CONFIG_ZMK_USB=n`）。

## 5. Prospector（自定义引脚）— 需你定夺的分叉点

`prospector_adapter` shield 的 board overlay 写死了引脚，与你的引脚不符：

| 信号 | 你的引脚 | 模块写死引脚 |
|---|---|---|
| SCL (SCK) | P0.19 | P1.13 |
| SDA (MOSI) | P0.07 | P1.15 |
| CS | P0.17 | P1.06 |
| DC | P0.14 | P1.04 |
| RES | P0.03 | P0.11 |
| BLK | P0.23 | P1.11 |

两个方案：

- **方案 A（推荐）：fork `tokyo2006/prospector-zmk-module`**，把 `boards/shields/prospector_adapter/boards/nice_nano_nrf52840_zmk.overlay` 里的引脚改成你的，west.yml 指向你的 fork。无 devicetree 合并歧义，一次到位。你已经维护过 fork（zmk-board-eyelash、zmk-tog-io），风格一致。
- **方案 B：不 fork**，在 `velvet_central_dongle.overlay` 里覆盖 `&spi3` / `mipi_dbi` / `&pwm1` 的引脚（用不同的 pinctrl 节点名避免冲突）。零 fork，但依赖 devicetree overlay 合并顺序，有「build 通过但引脚不对、屏不亮」的静默风险，需靠 CI 迭代。

屏本身用 Zephyr 内建 `sitronix,st7789v`（MIPI-DBI-SPI，写只读无 MISO），BLK 走 PWM 背光。dongle 侧沿用 `prospector_adapter` 的 `Kconfig.defconfig`（ZMK_DISPLAY + STATUS_SCREEN_CUSTOM + ST7789V_RGB565 + LVGL 16bit + 专用线程），并保留其 `MAIN_STACK_SIZE=4096` + `LV_Z_MEM_POOL_SIZE` 等稳定配置。

## 6. TPS 触控板（I2C，自定义引脚）

按技能 Section 5.4.3：

- `tps43-input-module`（`tokyo2006`，`main`，`import: true`）。
- `&i2cX`（SCL=P0.08 / SDA=P0.12，pinctrl 自定）下挂 `iqs5xx@74`，compatible `azoteq,iqs5xx`：
  - `reset-gpios = <&gpio0 30 GPIO_ACTIVE_LOW>`（RST）
  - `rdy-gpios = <&gpio0 21 GPIO_ACTIVE_HIGH>`（INT）
  - `one-finger-tap; two-finger-tap; scroll;`
- `/ { tps43_input { compatible = "zmk,input-listener"; device = <&tps43>; } }` 路由到鼠标。
- 中央 conf：`CONFIG_ZMK_TPS43=y` + `CONFIG_ZMK_TPS43_STUDIO_RPC=y` + `CONFIG_ZMK_POINTING=y`。
- 注意：dongle 已有轨迹球经 split_inputs 路由；TPS 在 dongle 本地，二者共存，都用 mouse runtime input processor。

## 7. RGB 灯（自写 ZMK 模块）

**硬件**：单颗 WS2812，数据脚占位 `P0.13`（等你给真实脚）。

**行为**：
- 读 `min(left, right)` 电量（中央经 `ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING` 取两个外设电量）。
- 颜色映射：HSV 色相 `hue = 120° × (battery/100)`，0% 红、100% 绿、50% 橙黄；线性均匀。
- `< 20%` 慢闪（周期开关），`≥ 20%` 恒亮。
- 满亮度/呼吸等细节：默认恒亮满亮，可后续调。

**实现**：一个小 ZMK/Zephyr 模块（`zmk-battery-rgb`）：
- DT：`worldsemi,ws2812-gpio` 节点（`chain-length = <1>`，`gpios = <&gpio0 13 ...>`），放在 dongle overlay。
- C：一个低优先级线程周期性读电量 → 计算 min → HSV→RGB → `led_strip_update_rgb()`；`<20%` 时按周期闪烁。
- Kconfig：`CONFIG_LED_STRIP=y` + `CONFIG_WS2812_STRIP=y`（或复用 ZMK underglow 的驱动）。
- 打包方式待定：独立小仓库（跟你 fork 风格一致）或 vendor 进 config 仓库 root（扩展 `zephyr/module.yml`）。实现时定，倾向独立小模块。

**需实现时确认**：cormoran fork 里读外设电量的确切 API（`zmk/split/bluetooth/central.h` 的 `zmk_split_ble_get_peripheral_battery_level()`）。

## 8. 轨迹球（迁移连带项）

右侧外围 PMW3610 轨迹球驱动是 v0.3 时代的（`badjeff`、`pixart,pmw3610-alt`），迁移到 4.1 会断。作为 ZMK 4.1 迁移的一部分，把驱动换成 `cormoran/zmk-driver-pmw3610-with-custom-studio-rpc`，compatible `pixart,pmw3610-alt` → `cormoran,pmw3610`，`force-awake` 属性移除。**只保证轨迹球可用，不额外开 Studio Trackball tab**（除非你要）。

## 9. 验证方式

全程 GHA（技能硬规则，不本地 build）：提交→push→读 Actions 日志，产出 `.uf2`。

## 10. 已确认的决定

1. **Prospector 引脚**：方案 B（本地 overlay 覆盖）。经查证 Zephyr 优先级 `EXTRA_DTC_OVERLAY_FILE > DTC_OVERLAY_FILE > shield_dts_files`，`config/*.overlay` 属于 DTC_OVERLAY_FILE，高于 `prospector_adapter` 的 shield board overlay，故把自定义引脚覆盖放进 `config/` overlay 即可可靠生效，无需 fork。
2. **RGB 数据脚**：占位 P0.13，待用户给真实引脚后替换。
3. **RGB 打包**：独立仓库 `tokyo2006/zmk-battery-rgb`（当前为空，需把模块代码写出来给用户 push）。
4. **轨迹球**：也开 Trackball tab（Level 2，含 `zmk-module-runtime-input-processor` + `ZMK_PMW3610_STUDIO_RPC`）。
5. **颜色映射**：线性 hue 0°红→120°绿，50%≈橙黄（60°）。

## 11. 实现备注（研究结论）

- config 仓库根是 Zephyr 模块（`zephyr/module.yml` board_root + workflow 传 `-DZMK_EXTRA_MODULES=${GITHUB_WORKSPACE}`），shield 在根 `boards/shields/velvet/`，config 目录经 `-DZMK_CONFIG=config` 传入。
- `prospector_adapter` 的 board overlay（`nice_nano_nrf52840_zmk.overlay`）写死引脚；方案 B 用 `config/` overlay 覆盖其 `&spi3`/`mipi_dbi`/`&pwm1` 引脚（用不同的 pinctrl 节点名避免冲突）。
- `tps43-input-module` 需要 `import: true`；compatible `azoteq,iqs5xx`；`CONFIG_ZMK_TPS43=y` + `CONFIG_ZMK_TPS43_STUDIO_RPC=y`。
- 读外设电量 API：`zmk_split_ble_get_peripheral_battery_level(index)`（`zmk/split/bluetooth/central.h`），实现时核对确切签名。
