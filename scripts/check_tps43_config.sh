#!/usr/bin/env bash
set -euo pipefail
grep -q '^CONFIG_ZMK_TPS43_STUDIO_RPC=y' build/velvet_central_dongle/zephyr/.config
grep -q '^CONFIG_ZMK_TPS43_SPLIT_RPC_RELAY=y' build/velvet_central_dongle/zephyr/.config
! grep -q '^CONFIG_INPUT_AZOTEQ_IQS5XX=y' build/velvet_central_dongle/zephyr/.config
grep -q '^CONFIG_ZMK_TPS43=y' build/velvet_peripheral_right/zephyr/.config
grep -q '^CONFIG_ZMK_TPS43_CUSTOM_SETTINGS=y' build/velvet_peripheral_right/zephyr/.config
! grep -q '^CONFIG_ZMK_TPS43=y' build/velvet_peripheral_left/zephyr/.config
