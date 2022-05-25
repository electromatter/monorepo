#!/bin/bash

# Short script to create an archlinux image in ~10s

set -ue

PACSTRAP_PACKAGES=(base)
PACKAGES=(linux mkinitcpio grub vim sudo)

MOUNTS=(/dev /sys /proc /run /tmp /var/tmp /var/cache/pacman)

err() {
    printf "ERR: %s\n" "$*" >&2
}

is_root() {
    [ "$(id -u)" -eq 0 ]
}

mount_binds() {
    local root
    root="$1"

    for name in "${MOUNTS[@]}"; do
        mkdir -p "${root}/${name}"
        mount --make-rslave -o bind "${name}" "${root}${name}"
    done
}

main() {
    local root img nbd part tag password hostname tz

    if ! is_root; then
        err "Must be run as root."
        exit 1
    fi

    exec < /dev/null

    modprobe nbd

    root=target
    img=arch.img
    dev=/dev/nbd0
    size=10G
    password='$6$//9wipcCZiqXtwS5$eRJHkt292CdpOGJrhfN0CjzKLiOudKmWheEwj6g8UjCvtBmNOBtpP49LdB.v/cWDdPKSYSfgQiimlxO1eDspv1'  # asdf
    hostname=bunny
    tz=America/New_York

    {
        qemu-img create "${img}" "${size}"
        qemu-nbd -f raw --cache none -c "${dev}" "${img}"
        blkdiscard "${dev}"

        printf 'size=+, type=L\n' | sfdisk "${dev}"
        part="${dev}p1"

        mkfs.ext4 "${part}"
        tag="$(blkid --output export "${part}" | grep ^UUID)"

        mkdir -p "${root}"
        mount "${part}" "${root}"

        pacstrap -c "${root}" "${PACSTRAP_PACKAGES[@]}"

        mount_binds "${root}"

        chroot "${root}" \
            env \
                "tag=${tag}" \
                "dev=${dev}" \
                "password=${password}" \
                "hostname=${hostname}" \
                "tz=${tz}" \
                bash -c '
            set -ue

            sed -i -e "/^#en_US.UTF-8/ s/^#//" /etc/locale.gen
            locale-gen
            printf "LANG=en_US.UTF-8\n" > /etc/locale.conf
            export LANG=en_US.UTF-8

            printf "%s\n" "${tag} / ext4 defaults 0 0" > /etc/fstab

            ln -s "/usr/share/zoneinfo/${tz}" /etc/localtime

            printf "${hostname}\n" > /etc/hostname
            cat > /etc/systemd/network/default.network << EOF
[Match]
Name=en*

[Network]
DHCP=ipv4
EOF
            systemctl enable systemd-networkd

            groupadd sudo
            useradd --groups sudo -p "${password}" -m -s /bin/bash erai
            cat > /etc/sudoers << EOF
root ALL=(ALL) ALL
%sudo ALL=(ALL:ALL) NOPASSWD: ALL
@includedir /etc/sudoers.d
EOF

            cat > /etc/mkinitcpio.conf << EOF
MODULES=(virtio ext4)
BINARIES=()
FILES=()
HOOKS=(systemd block filesystems)
COMPRESSION=zstd
EOF

            pacman -S --noconfirm "$@"

            cat > /etc/default/grub << EOF
GRUB_DEFAULT=0
GRUB_TIMEOUT=0
GRUB_DISTRIBUTOR="Arch"
GRUB_CMDLINE_LINUX_DEFAULT="ro quiet"
GRUB_CMDLINE_LINUX=""
GRUB_PRELOAD_MODULES="part_msdos"
GRUB_TIMEOUT_STYLE=hidden
GRUB_TERMINAL_INPUT=console
GRUB_GFXMODE=auto
GRUB_GFXPAYLOAD_LINUX=keep
GRUB_DISABLE_OS_PROBER=true
EOF

            grub-install --target=i386-pc "${dev}"
            grub-mkconfig -o /boot/grub/grub.cfg
        ' -- "${PACKAGES[@]}"

        cp "${root}/boot/vmlinuz-linux" "${root}/boot/initramfs-linux.img" .

        find "${root}" -xdev -name '*.pacnew' -delete

        fstrim "${root}"

        umount -R "${root}"
        rmdir "${root}"
        qemu-nbd -d "${dev}"
    } || {
        err "Build failed."
        umount -R "${root}" || :
        rmdir "${root}" || :
        qemu-nbd -d "${dev}" || :
        return 1
    }
}

main "$@"
