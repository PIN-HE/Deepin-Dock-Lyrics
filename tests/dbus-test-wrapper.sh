#!/usr/bin/env bash
# 在无 D-Bus activation 的私有 session bus 上运行测试命令。
# 默认 dbus-run-session 仍会加载 /usr/share/dbus-1/session.conf 的
# <servicedir>，测试进程的 method call 会触发系统 lyrics-dockd 的
# D-Bus activation，污染测试（真实 daemon 抢占 org.deepin.LyricsDock1）。
# 本 wrapper 使用不含 servicedir 的配置，彻底隔离。
#
# Runs a test command on a private session bus without any D-Bus activation
# directory. dbus-run-session keeps the default <servicedir>, so a test's
# method call can still activate the real lyrics-dockd and steal the
# org.deepin.LyricsDock1 name. This wrapper removes that side effect.

set -u

config_file=$(mktemp /tmp/dbus-noact-XXXXXX.conf)
address_file=$(mktemp /tmp/dbus-noact-XXXXXX.addr)
daemon_pid=""
cleanup() {
    if [ -n "$daemon_pid" ]; then
        kill "$daemon_pid" 2>/dev/null
        wait "$daemon_pid" 2>/dev/null
    fi
    rm -f "$config_file" "$address_file"
}
trap cleanup EXIT

cat > "$config_file" <<'EOF'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF

dbus-daemon --config-file="$config_file" --print-address \
    > "$address_file" 2>/dev/null &
daemon_pid=$!

for _ in $(seq 100); do
    [ -s "$address_file" ] && break
    sleep 0.05
done
address=$(head -n 1 "$address_file")
if [ -z "$address" ]; then
    echo "dbus-test-wrapper: failed to start private dbus-daemon" >&2
    exit 1
fi

export DBUS_SESSION_BUS_ADDRESS="$address"
exec "$@"
