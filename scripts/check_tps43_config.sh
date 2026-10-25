#!/usr/bin/env bash
set -euo pipefail
for config in build/velvet_central_dongle/zephyr/.config \
              build/velvet_peripheral_right/zephyr/.config \
              build/velvet_peripheral_left/zephyr/.config; do
    if [[ ! -f "$config" ]]; then
        echo "missing $config; build the three Velvet shields first" >&2
        exit 1
    fi
done
grep -q '^CONFIG_ZMK_TPS43_STUDIO_RPC=y' build/velvet_central_dongle/zephyr/.config
grep -q '^CONFIG_ZMK_TPS43_SPLIT_RPC_RELAY=y' build/velvet_central_dongle/zephyr/.config
grep -q '^CONFIG_ZMK_SPLIT_RELAY_EVENT_DATA_LEN=240$' build/velvet_central_dongle/zephyr/.config
! grep -q '^CONFIG_INPUT_AZOTEQ_IQS5XX=y$' build/velvet_central_dongle/zephyr/.config
! grep -q '^CONFIG_ZMK_TPS43=y$' build/velvet_central_dongle/zephyr/.config
grep -q '^CONFIG_ZMK_TPS43=y' build/velvet_peripheral_right/zephyr/.config
grep -q '^CONFIG_ZMK_TPS43_CUSTOM_SETTINGS=y' build/velvet_peripheral_right/zephyr/.config
grep -q '^CONFIG_ZMK_TPS43_SPLIT_RPC_RELAY=y' build/velvet_peripheral_right/zephyr/.config
grep -q '^CONFIG_ZMK_SPLIT_RELAY_EVENT_DATA_LEN=240$' build/velvet_peripheral_right/zephyr/.config
! grep -q '^CONFIG_ZMK_TPS43=y$' build/velvet_peripheral_left/zephyr/.config
