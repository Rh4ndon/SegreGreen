#!/bin/bash

echo "🚀 Starting Segri Green Hotspot..."

# Stop systemd-resolved if running
systemctl stop systemd-resolved 2>/dev/null
systemctl disable systemd-resolved 2>/dev/null

# Kill any existing processes
pkill hostapd 2>/dev/null
pkill dnsmasq 2>/dev/null

# Wait a moment
sleep 2

# Assign IP address
ip addr add 192.168.4.1/24 dev wlxe0e1a994e580 2>/dev/null

# Start hostapd
hostapd /root/segri_green/config/hostapd.conf -B
echo "✅ hostapd started"

# Wait for hostapd to initialize
sleep 2

# Start dnsmasq
dnsmasq -C /root/segri_green/config/dnsmasq.conf
echo "✅ dnsmasq started"

# Apply iptables rules (if not already saved)
iptables -t nat -A POSTROUTING -o end0 -j MASQUERADE 2>/dev/null
iptables -A FORWARD -i end0 -o wlxe0e1a994e580 -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null
iptables -A FORWARD -i wlxe0e1a994e580 -o end0 -j ACCEPT 2>/dev/null

echo "✅ Firewall rules applied"
echo ""
echo "📡 WiFi Hotspot: Segri Green WiFi"
echo "🌐 Gateway: 192.168.4.1"
echo ""


