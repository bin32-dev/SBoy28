#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage:
  build_efi_iso.sh [options]

Options:
  -b, --build-dir DIR      Build directory (default: ./build)
  -o, --output ISO         Output ISO path (default: <build-dir>/SBoy28-efi.iso)
  -k, --kernel ELF         Kernel ELF path (default: <build-dir>/kernel.elf)
  -r, --raw-image IMG      Optional raw image to embed at /data/images/
  -a, --apps-dir DIR       Apps directory to copy into /apps (default: ./apps/OS)
      --extra-apps DIR     Extra app directory to merge into /apps (repeatable)
      --volume-id LABEL    ISO volume label (default: SBOY28_EFI)
      --timeout SEC        GRUB timeout in seconds (default: 2)
      --keep-staging       Keep staging tree under <build-dir>/iso_root
  -h, --help               Show this help

Examples:
  ./tools/build_efi_iso.sh
  ./tools/build_efi_iso.sh -b build -o build/SBoy28-efi.iso -r build/SBoy28.img
  ./tools/build_efi_iso.sh --apps-dir ./apps/OS --extra-apps ./apps/custom
USAGE
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_ISO=""
KERNEL_ELF=""
RAW_IMG=""
APPS_DIR="${ROOT_DIR}/apps/OS"
VOLUME_ID="SBOY28_EFI"
GRUB_TIMEOUT="2"
KEEP_STAGING=0

EXTRA_APPS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build-dir)
            BUILD_DIR="$2"; shift 2 ;;
        -o|--output)
            OUTPUT_ISO="$2"; shift 2 ;;
        -k|--kernel)
            KERNEL_ELF="$2"; shift 2 ;;
        -r|--raw-image)
            RAW_IMG="$2"; shift 2 ;;
        -a|--apps-dir)
            APPS_DIR="$2"; shift 2 ;;
        --extra-apps)
            EXTRA_APPS+=("$2"); shift 2 ;;
        --volume-id)
            VOLUME_ID="$2"; shift 2 ;;
        --timeout)
            GRUB_TIMEOUT="$2"; shift 2 ;;
        --keep-staging)
            KEEP_STAGING=1; shift ;;
        -h|--help)
            usage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1 ;;
    esac
done

if [[ -z "${OUTPUT_ISO}" ]]; then
    OUTPUT_ISO="${BUILD_DIR}/SBoy28-efi.iso"
fi
if [[ -z "${KERNEL_ELF}" ]]; then
    KERNEL_ELF="${BUILD_DIR}/kernel.elf"
fi

ISO_ROOT="${BUILD_DIR}/iso_root"
EFI_BOOT_DIR="${ISO_ROOT}/EFI/BOOT"
GRUB_DIR="${ISO_ROOT}/boot/grub"
GRUB_CFG="${GRUB_DIR}/grub.cfg"
INIT_CFG="${ISO_ROOT}/boot/os-init.cfg"
MOUNT_CFG="${ISO_ROOT}/boot/mounts.conf"
APPS_CFG="${ISO_ROOT}/boot/apps.list"

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "Missing required command: $1" >&2
        exit 1
    }
}

require_cmd grub-mkstandalone
require_cmd grub-mkrescue
require_cmd xorriso

if [[ ! -f "${KERNEL_ELF}" ]]; then
    echo "Kernel not found: ${KERNEL_ELF}" >&2
    echo "Build first (e.g. cmake -S . -B build && cmake --build build)" >&2
    exit 1
fi

if [[ -n "${RAW_IMG}" && ! -f "${RAW_IMG}" ]]; then
    echo "Raw image not found: ${RAW_IMG}" >&2
    exit 1
fi

if [[ ! -d "${APPS_DIR}" ]]; then
    echo "Apps directory not found: ${APPS_DIR}" >&2
    exit 1
fi

rm -rf "${ISO_ROOT}"
mkdir -p "${GRUB_DIR}" "${EFI_BOOT_DIR}" "${ISO_ROOT}/apps" "${ISO_ROOT}/data"

cp "${KERNEL_ELF}" "${ISO_ROOT}/boot/kernel.elf"
cp -a "${APPS_DIR}/." "${ISO_ROOT}/apps/"

for extra in "${EXTRA_APPS[@]}"; do
    if [[ ! -d "${extra}" ]]; then
        echo "Extra apps directory not found: ${extra}" >&2
        exit 1
    fi
    cp -a "${extra}/." "${ISO_ROOT}/apps/"
done

if [[ -n "${RAW_IMG}" ]]; then
    mkdir -p "${ISO_ROOT}/data/images"
    cp "${RAW_IMG}" "${ISO_ROOT}/data/images/$(basename "${RAW_IMG}")"
fi

cat > "${GRUB_CFG}" <<GRUBEOF
set timeout=${GRUB_TIMEOUT}
set default=0

menuentry "SBoy28 (UEFI)" {
    multiboot /boot/kernel.elf
    boot
}
GRUBEOF

cat > "${INIT_CFG}" <<'INITEOF'
[init]
mount_config=/boot/mounts.conf
apps_manifest=/boot/apps.list
default_shell=/apps/OS
INITEOF

cat > "${MOUNT_CFG}" <<'MOUNTEOF'
[mounts]
/apps=ramfs,ro
/data=ramfs,rw
MOUNTEOF

cat > "${APPS_CFG}" <<'APPSEOF'
/apps/OS
APPSEOF

# Build both common UEFI fallback names.
grub-mkstandalone \
    -O x86_64-efi \
    -o "${EFI_BOOT_DIR}/BOOTX64.EFI" \
    "boot/grub/grub.cfg=${GRUB_CFG}"

grub-mkstandalone \
    -O i386-efi \
    -o "${EFI_BOOT_DIR}/BOOTIA32.EFI" \
    "boot/grub/grub.cfg=${GRUB_CFG}"

mkdir -p "$(dirname "${OUTPUT_ISO}")"
grub-mkrescue \
    --xorriso=xorriso \
    -- \
    -volid "${VOLUME_ID}" \
    -o "${OUTPUT_ISO}" \
    "${ISO_ROOT}" >/dev/null

echo "Created UEFI-bootable ISO: ${OUTPUT_ISO}"
if [[ ${KEEP_STAGING} -eq 1 ]]; then
    echo "Staging preserved at: ${ISO_ROOT}"
else
    rm -rf "${ISO_ROOT}"
fi
