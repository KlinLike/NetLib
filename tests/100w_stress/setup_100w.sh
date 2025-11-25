#!/bin/bash
# ==============================================================================
# NetLib 百万并发一键配置脚本（单文件版）
#
# 目的：为 Linux 在 100 万并发场景下配置所需的系统参数。
# 功能：
#   - 配置文件描述符与 systemd 限制（最高 1048576）
#   - 写入并应用核心 TCP/内核参数（如 somaxconn、port range、fastopen 等）
#   - 创建并启用 8GB 交换空间，配置永久挂载与 swappiness=10
#   - 在每一步打印关键参数的变更前后值，并最终统一验证
# 使用：
#   - 交互式：sudo tests/100w_stress/setup_100w.sh
#   - 无交互：sudo tests/100w_stress/setup_100w.sh auto
#   - 仅验证：sudo tests/100w_stress/setup_100w.sh verify
# 注意：需要 root 权限，部分设置在重启或重新登录后完全生效
# ==============================================================================

set -e

COLOR_GREEN="\033[0;32m"
COLOR_YELLOW="\033[1;33m"
COLOR_RED="\033[0;31m"
COLOR_BLUE="\033[0;34m"
COLOR_RESET="\033[0m"

info() {
    echo -e "${COLOR_BLUE}[INFO]${COLOR_RESET} $1"
}

success() {
    echo -e "${COLOR_GREEN}[✓]${COLOR_RESET} $1"
}

warning() {
    echo -e "${COLOR_YELLOW}[⚠]${COLOR_RESET} $1"
}

error() {
    echo -e "${COLOR_RED}[✗]${COLOR_RESET} $1"
}

prompt_yes_no() {
    local q="$1 [Y/n]: "
    read -r -p "$(echo -e "${COLOR_BLUE}[?]${COLOR_RESET} $q")" reply
    case "$reply" in
        [Nn]|[Nn][Oo]) return 1 ;;
        *) return 0 ;;
    esac
}

prompt_interactive() {
    if [ "${AUTO_MODE:-false}" = true ]; then
        return 1
    fi
    prompt_yes_no "$1"
}

# 检查是否为root用户
check_root() {
    if [ "$EUID" -ne 0 ]; then
        error "请使用 sudo 运行此脚本"
        exit 1
    fi
}

# 获取脚本目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 显示欢迎信息
show_welcome() {
    echo ""
    echo "================================================================"
    echo "   NetLib 百万并发一键配置脚本"
    echo "================================================================"
    echo ""
    info "本脚本将自动配置："
    echo "  1. 文件描述符限制"
    echo "  2. TCP 网络参数"
    echo "  3. 内存优化参数"
    echo "  4. 8GB 交换空间"
    echo ""
}

# 备份配置
backup_config() {
    local backup_dir="/tmp/netlib_backup_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir"
    if [ -f /etc/security/limits.conf ]; then cp /etc/security/limits.conf "$backup_dir/"; fi
    if [ -f /etc/sysctl.conf ]; then cp /etc/sysctl.conf "$backup_dir/"; fi
    info "配置已备份到: $backup_dir"
}

# 文件描述符限制
setup_file_limits() {
    info "配置文件描述符限制..."
    local before_soft=$(ulimit -Sn)
    local before_hard=$(ulimit -Hn)
    local before_filemax=$(cat /proc/sys/fs/file-max 2>/dev/null || echo 'N/A')
    info "变更前: soft=$before_soft, hard=$before_hard, fs.file-max=$before_filemax"
    ulimit -n 1048576 2>/dev/null || ulimit -n 65535
    if grep -q "NetLib 百万并发配置" /etc/security/limits.conf 2>/dev/null; then
        info "文件描述符限制已配置，跳过"
    else
        cat >> /etc/security/limits.conf <<EOF

# NetLib 百万并发配置
* soft nofile 1048576
* hard nofile 1048576
root soft nofile 1048576
root hard nofile 1048576
EOF
        success "文件描述符限制已写入 /etc/security/limits.conf"
    fi
    if [ -d /etc/systemd ]; then
        mkdir -p /etc/systemd/system.conf.d
        if [ ! -f /etc/systemd/system.conf.d/limits.conf ] || ! grep -q "DefaultLimitNOFILE" /etc/systemd/system.conf.d/limits.conf 2>/dev/null; then
            cat > /etc/systemd/system.conf.d/limits.conf <<EOF
[Manager]
DefaultLimitNOFILE=1048576
EOF
            success "Systemd 限制已写入 /etc/systemd/system.conf.d/limits.conf"
        else
            local current_limit=$(grep -E "^DefaultLimitNOFILE=" /etc/systemd/system.conf.d/limits.conf | tail -n1 | cut -d'=' -f2)
            if [ "$current_limit" != "1048576" ]; then
                if prompt_interactive "检测到 DefaultLimitNOFILE=$current_limit，是否更新为 1048576"; then
                    sed -i -E 's/^DefaultLimitNOFILE=.*/DefaultLimitNOFILE=1048576/' /etc/systemd/system.conf.d/limits.conf
                    success "Systemd DefaultLimitNOFILE 已更新为 1048576"
                else
                    info "保留现有 DefaultLimitNOFILE=$current_limit"
                fi
            else
                info "Systemd 限制已配置，跳过"
            fi
        fi
    fi
    local after_soft=$(ulimit -Sn)
    local after_hard=$(ulimit -Hn)
    local after_filemax=$(cat /proc/sys/fs/file-max 2>/dev/null || echo 'N/A')
    success "变更后: soft=$after_soft, hard=$after_hard, fs.file-max=$after_filemax"
    success "文件描述符限制配置完成（永久生效）"

    if grep -q "nofile" /etc/security/limits.conf 2>/dev/null; then
        local line
        line=$(grep -E "^\*\s+soft\s+nofile\s+" /etc/security/limits.conf | tail -n1)
        if [ -n "$line" ]; then
            local val=$(echo "$line" | awk '{print $4}')
            if [ "$val" != "1048576" ]; then
                if prompt_interactive "检测到 * soft nofile=$val，是否更新为 1048576"; then
                    sed -i -E 's/^\*\s+soft\s+nofile\s+.*/\* soft nofile 1048576/' /etc/security/limits.conf
                    success "已更新 * soft nofile=1048576"
                else
                    info "保留现有 * soft nofile=$val"
                fi
            fi
        fi

        line=$(grep -E "^\*\s+hard\s+nofile\s+" /etc/security/limits.conf | tail -n1)
        if [ -n "$line" ]; then
            local val=$(echo "$line" | awk '{print $4}')
            if [ "$val" != "1048576" ]; then
                if prompt_interactive "检测到 * hard nofile=$val，是否更新为 1048576"; then
                    sed -i -E 's/^\*\s+hard\s+nofile\s+.*/\* hard nofile 1048576/' /etc/security/limits.conf
                    success "已更新 * hard nofile=1048576"
                else
                    info "保留现有 * hard nofile=$val"
                fi
            fi
        fi

        line=$(grep -E "^root\s+soft\s+nofile\s+" /etc/security/limits.conf | tail -n1)
        if [ -n "$line" ]; then
            local val=$(echo "$line" | awk '{print $4}')
            if [ "$val" != "1048576" ]; then
                if prompt_interactive "检测到 root soft nofile=$val，是否更新为 1048576"; then
                    sed -i -E 's/^root\s+soft\s+nofile\s+.*/root soft nofile 1048576/' /etc/security/limits.conf
                    success "已更新 root soft nofile=1048576"
                else
                    info "保留现有 root soft nofile=$val"
                fi
            fi
        fi

        line=$(grep -E "^root\s+hard\s+nofile\s+" /etc/security/limits.conf | tail -n1)
        if [ -n "$line" ]; then
            local val=$(echo "$line" | awk '{print $4}')
            if [ "$val" != "1048576" ]; then
                if prompt_interactive "检测到 root hard nofile=$val，是否更新为 1048576"; then
                    sed -i -E 's/^root\s+hard\s+nofile\s+.*/root hard nofile 1048576/' /etc/security/limits.conf
                    success "已更新 root hard nofile=1048576"
                else
                    info "保留现有 root hard nofile=$val"
                fi
            fi
        fi
    fi
}

setup_pam_limits() {
    info "配置 PAM 限制模块..."
    local pam_files="/etc/pam.d/common-session /etc/pam.d/common-session-noninteractive"
    for pam_file in $pam_files; do
        [ -f "$pam_file" ] || continue
        if ! grep -Eq '^[[:space:]]*session[[:space:]]+required[[:space:]]+pam_limits\.so' "$pam_file" 2>/dev/null; then
            if grep -Eq '^[[:space:]]*session[[:space:]]+required' "$pam_file" 2>/dev/null; then
                echo "session required pam_limits.so" >> "$pam_file"
                success "已添加 pam_limits.so 到 $pam_file"
            else
                warning "无法找到 session required 行，请手动检查 $pam_file"
            fi
        else
            info "pam_limits.so 已在 $pam_file 中配置"
        fi
    done
}

# 内核网络参数
setup_sysctl() {
    info "配置内核网络参数..."
    local before_somaxconn=$(cat /proc/sys/net/core/somaxconn 2>/dev/null || echo 'N/A')
    local before_syn_backlog=$(cat /proc/sys/net/ipv4/tcp_max_syn_backlog 2>/dev/null || echo 'N/A')
    local before_port_range=$(cat /proc/sys/net/ipv4/ip_local_port_range 2>/dev/null || echo 'N/A')
    local before_fastopen=$(cat /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null || echo 'N/A')
    local before_swappiness=$(cat /proc/sys/vm/swappiness 2>/dev/null || echo 'N/A')
    info "变更前: somaxconn=$before_somaxconn, syn_backlog=$before_syn_backlog, port_range=$before_port_range, fastopen=$before_fastopen, swappiness=$before_swappiness"
    if ! grep -q "NetLib 百万并发优化配置" /etc/sysctl.conf 2>/dev/null; then
        cat >> /etc/sysctl.conf <<EOF

# ====== NetLib 百万并发优化配置 ======

fs.file-max = 2097152
fs.nr_open = 2097152

net.core.somaxconn = 65535
net.core.netdev_max_backlog = 65536
net.ipv4.tcp_max_syn_backlog = 65536

net.ipv4.ip_local_port_range = 1024 65535

net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 10
net.ipv4.tcp_max_tw_buckets = 2000000

net.ipv4.tcp_mem = 786432 1048576 1572864
net.ipv4.tcp_rmem = 4096 4096 16777216
net.ipv4.tcp_wmem = 4096 4096 16777216
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216

net.netfilter.nf_conntrack_max = 2000000
net.nf_conntrack_max = 2000000

net.ipv4.tcp_fastopen = 3

net.ipv4.tcp_keepalive_time = 600
net.ipv4.tcp_keepalive_probes = 3
net.ipv4.tcp_keepalive_intvl = 15

net.ipv4.tcp_syncookies = 1

net.ipv4.tcp_max_orphans = 262144
net.ipv4.tcp_orphan_retries = 1

net.ipv4.tcp_retries2 = 8
net.ipv4.tcp_syn_retries = 3
net.ipv4.tcp_synack_retries = 3

net.ipv4.tcp_slow_start_after_idle = 0
net.ipv4.tcp_mtu_probing = 1

net.ipv4.ip_local_reserved_ports = 1024-2000

vm.overcommit_memory = 1
vm.swappiness = 10
EOF
        success "已写入 NetLib 优化标记"
    fi
    ensure_sysctl() {
        local key="$1"
        local desired="$2"
        if grep -Eq "^${key}\s*=" /etc/sysctl.conf 2>/dev/null; then
            local current=$(grep -E "^${key}\s*=" /etc/sysctl.conf | tail -n1 | sed -E "s/^${key}\s*=\s*//")
            if [ "$current" != "$desired" ]; then
                if prompt_interactive "检测到 ${key}=${current}，是否更新为 ${desired}"; then
                    sed -i -E "s/^${key}\s*=\s*.*/${key} = ${desired}/" /etc/sysctl.conf
                    success "已更新 ${key}=${desired}"
                else
                    info "保留现有 ${key}=${current}"
                fi
            fi
        else
            echo "${key} = ${desired}" >> /etc/sysctl.conf
            success "已写入 ${key}=${desired}"
        fi
    }

    ensure_sysctl fs.file-max 2097152
    ensure_sysctl fs.nr_open 2097152
    ensure_sysctl net.core.somaxconn 65535
    ensure_sysctl net.core.netdev_max_backlog 65536
    ensure_sysctl net.ipv4.tcp_max_syn_backlog 65536
    ensure_sysctl net.ipv4.ip_local_port_range "1024 65535"
    ensure_sysctl net.ipv4.tcp_tw_reuse 1
    ensure_sysctl net.ipv4.tcp_fin_timeout 10
    ensure_sysctl net.ipv4.tcp_max_tw_buckets 2000000
    ensure_sysctl net.ipv4.tcp_mem "786432 1048576 1572864"
    ensure_sysctl net.ipv4.tcp_rmem "4096 4096 16777216"
    ensure_sysctl net.ipv4.tcp_wmem "4096 4096 16777216"
    ensure_sysctl net.core.rmem_max 16777216
    ensure_sysctl net.core.wmem_max 16777216
    ensure_sysctl net.netfilter.nf_conntrack_max 2000000
    ensure_sysctl net.nf_conntrack_max 2000000
    ensure_sysctl net.ipv4.tcp_fastopen 3
    ensure_sysctl net.ipv4.tcp_keepalive_time 600
    ensure_sysctl net.ipv4.tcp_keepalive_probes 3
    ensure_sysctl net.ipv4.tcp_keepalive_intvl 15
    ensure_sysctl net.ipv4.tcp_syncookies 1
    ensure_sysctl net.ipv4.tcp_max_orphans 262144
    ensure_sysctl net.ipv4.tcp_orphan_retries 1
    ensure_sysctl net.ipv4.tcp_retries2 8
    ensure_sysctl net.ipv4.tcp_syn_retries 3
    ensure_sysctl net.ipv4.tcp_synack_retries 3
    ensure_sysctl net.ipv4.tcp_slow_start_after_idle 0
    ensure_sysctl net.ipv4.tcp_mtu_probing 1
    ensure_sysctl net.ipv4.ip_local_reserved_ports 1024-2000
    ensure_sysctl vm.overcommit_memory 1
    ensure_sysctl vm.swappiness 10
    info "应用内核参数到当前系统..."
    sysctl -p > /dev/null 2>&1 || true
    local after_somaxconn=$(cat /proc/sys/net/core/somaxconn 2>/dev/null || echo 'N/A')
    local after_syn_backlog=$(cat /proc/sys/net/ipv4/tcp_max_syn_backlog 2>/dev/null || echo 'N/A')
    local after_port_range=$(cat /proc/sys/net/ipv4/ip_local_port_range 2>/dev/null || echo 'N/A')
    local after_fastopen=$(cat /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null || echo 'N/A')
    local after_swappiness=$(cat /proc/sys/vm/swappiness 2>/dev/null || echo 'N/A')
    success "变更后: somaxconn=$after_somaxconn, syn_backlog=$after_syn_backlog, port_range=$after_port_range, fastopen=$after_fastopen, swappiness=$after_swappiness"
    success "内核网络参数配置完成（永久生效）"
}

# 系统参数
setup_system() {
    info "步骤 1/2: 配置系统参数..."
    echo ""
    set +e
    backup_config
    setup_file_limits
    setup_pam_limits
    setup_sysctl
    local rc=$?
    set -e
    if [ $rc -eq 0 ]; then
        success "系统参数配置完成"
    else
        error "系统参数配置失败 (code $rc)"
        return $rc
    fi
    echo ""
}

# 交换空间检查
check_swap() {
    free -h
    if [ -f /proc/swaps ]; then cat /proc/swaps; fi
}

# 创建并启用交换空间
setup_swap_inner() {
    local SWAP_FILE="/swapfile"
    local SWAP_SIZE="8G"
    local before_swap=$(free -h | awk '/Swap/ {print $2}')
    info "开始创建 ${SWAP_SIZE} 交换空间... (变更前: Swap=$before_swap)"
    if [ -f "$SWAP_FILE" ] && swapon --show | grep -q "$SWAP_FILE"; then
        info "Swap 已启用，跳过创建"
        return 0
    fi
    if [ -f "$SWAP_FILE" ]; then swapoff "$SWAP_FILE" 2>/dev/null || true; fi
    local available_space=$(df / | tail -1 | awk '{print $4}')
    local required_space=$((8 * 1024 * 1024))
    if [ "$available_space" -lt "$required_space" ]; then
        error "磁盘空间不足"
        return 1
    fi
    if command -v fallocate > /dev/null; then
        fallocate -l ${SWAP_SIZE} "$SWAP_FILE"
    else
        dd if=/dev/zero of="$SWAP_FILE" bs=1M count=8192 status=progress
    fi
    chmod 600 "$SWAP_FILE"
    mkswap "$SWAP_FILE"
    swapon "$SWAP_FILE"
    local after_swap=$(free -h | awk '/Swap/ {print $2}')
    success "交换空间已启用 (变更后: Swap=$after_swap)"
}

# 永久挂载与swappiness
setup_swap_permanent() {
    local SWAP_FILE="/swapfile"
    if ! grep -q "$SWAP_FILE" /etc/fstab 2>/dev/null; then
        cp /etc/fstab /etc/fstab.backup.$(date +%Y%m%d_%H%M%S)
        echo "$SWAP_FILE none swap sw 0 0" >> /etc/fstab
        success "已添加到 /etc/fstab"
    fi
    local target_swappiness=10
    local before_swappiness=$(cat /proc/sys/vm/swappiness 2>/dev/null || echo 'N/A')
    sysctl -w vm.swappiness=$target_swappiness >/dev/null 2>&1 || true
    if ! grep -q "vm.swappiness" /etc/sysctl.conf; then
        echo "vm.swappiness = $target_swappiness" >> /etc/sysctl.conf
    fi
    local after_swappiness=$(cat /proc/sys/vm/swappiness 2>/dev/null || echo 'N/A')
    info "Swappiness: $before_swappiness -> $after_swappiness"
}

# 交换空间
setup_swap() {
    info "步骤 2/2: 配置交换空间..."
    echo ""
    set +e
    setup_swap_inner || {
        error "交换空间创建失败"
        set -e
        return 1
    }
    setup_swap_permanent
    set -e
    success "交换空间配置完成"
    echo ""
}

# 验证所有配置
verify_all() {
    echo ""
    info "验证配置..."
    echo ""
    
    local errors=0
    
    # 验证文件描述符
    local fd_limit=$(ulimit -n)
    if [ "$fd_limit" -ge 100000 ]; then
        success "文件描述符: $fd_limit"
    else
        error "文件描述符过低: $fd_limit"
        ((errors++))
    fi
    
    # 验证TCP参数
    local somaxconn=$(cat /proc/sys/net/core/somaxconn)
    if [ "$somaxconn" -ge 10000 ]; then
        success "TCP队列: $somaxconn"
    else
        warning "TCP队列较低: $somaxconn"
    fi
    
    # 验证交换空间
    local swap_size=$(free -h | grep Swap | awk '{print $2}')
    if [ "$swap_size" != "0B" ]; then
        success "交换空间: $swap_size"
    else
        warning "交换空间未配置"
    fi
    
    echo ""
    
    if [ $errors -eq 0 ]; then
        success "所有配置验证通过！"
        return 0
    else
        error "发现 $errors 个配置问题"
        return 1
    fi
}

# 显示完成信息
show_complete() {
    echo ""
    echo "================================================================"
    success "配置完成！"
    echo "================================================================"
    echo ""
    warning "重要提示："
    echo "  1. 配置已永久生效，重启后自动加载"
    echo "  2. 建议重启系统使所有设置完全生效："
    echo "     sudo reboot"
    echo ""
    info "验证命令："
    echo "  ulimit -n       # 查看文件描述符"
    echo "  free -h         # 查看内存和交换空间"
    echo ""
}

# 显示使用说明
show_usage() {
    cat <<EOF
用法: sudo $0 [命令]

命令:
    (无参数)    交互式配置（逐步确认）
    auto        无交互一键配置（系统参数 + 交换空间）
    verify      验证当前配置
    help        显示此帮助信息

示例:
    sudo $0              # 交互式配置
    sudo $0 auto         # 无交互一键配置
    sudo $0 verify       # 验证配置

说明:
    本脚本为单文件实现，顺序交互式设置系统参数与交换空间，
    并在每步后显示成功/失败与当前值，结束时统一验证。

EOF
}

# 主函数
main() {
    case "${1:-interactive}" in
        interactive)
            AUTO_MODE=false
            check_root
            show_welcome
            info "检查当前配置"
            verify_all || true
            echo ""
            if prompt_yes_no "是否继续配置系统参数"; then
                setup_system || exit 1
            else
                warning "已跳过系统参数配置"
            fi
            echo ""
            if prompt_yes_no "是否继续配置交换空间"; then
                setup_swap || exit 1
            else
                warning "已跳过交换空间配置"
            fi
            verify_all
            show_complete
            ;;
        auto)
            AUTO_MODE=true
            check_root
            show_welcome
            setup_system
            setup_swap
            verify_all
            show_complete
            ;;
        verify)
            check_root
            verify_all
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            error "未知命令: $1"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"
