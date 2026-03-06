#!/usr/bin/env python3
"""
Segri Green Token Server
- Reads tokens from ESP32 via serial
- Blocks internet until valid token entered
- Serves login page at http://192.168.4.1
"""

import serial
import json
import time
import threading
import subprocess
import os
import re
from flask import Flask, request, render_template_string, redirect
from datetime import datetime, timedelta

# Configuration
SERIAL_PORT = "/dev/ttyUSB0"
BAUD_RATE = 115200
TOKEN_FILE = "/root/segri_green/tokens.json"
HOTSPOT_INTERFACE = "wlxe0e1a994e580"  # Your WiFi interface
#WAN_INTERFACE = "end0"  # Your internet interface
#WAN_INTERFACE = "enx00e04c362d27"
WAN_INTERFACE = "enx025851313666"
LEASES_FILE = "/var/lib/misc/dnsmasq.leases"

app = Flask(__name__)

# HTML login page
LOGIN_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <title>Segri Green WiFi</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #2ecc71, #27ae60);
            color: white;
            min-height: 100vh;
        }
        .container {
            max-width: 500px;
            margin: 0 auto;
            background: rgba(0,0,0,0.3);
            border-radius: 15px;
            padding: 25px;
            backdrop-filter: blur(5px);
        }
        h1 { text-align: center; font-size: 28px; }
        .logo { text-align: center; font-size: 60px; margin: 20px 0; }
        .form-group { margin-bottom: 20px; }
        input[type="text"] {
            width: 100%;
            padding: 15px;
            border: none;
            border-radius: 8px;
            font-size: 18px;
            box-sizing: border-box;
        }
        button {
            width: 100%;
            padding: 15px;
            background: #f1c40f;
            border: none;
            border-radius: 8px;
            font-size: 20px;
            font-weight: bold;
            cursor: pointer;
        }
        button:hover { background: #f39c12; }
        .info-box {
            background: rgba(0,0,0,0.3);
            padding: 15px;
            border-radius: 8px;
            margin-top: 20px;
        }
        .error {
            color: #e74c3c;
            background: rgba(0,0,0,0.3);
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .success {
            color: #2ecc71;
            background: rgba(0,0,0,0.3);
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .footer {
            text-align: center;
            margin-top: 30px;
            font-size: 12px;
            opacity: 0.8;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">♻️</div>
        <h1>Segri Green WiFi</h1>
        
        {% if message %}
            <div class="{{ 'success' if success else 'error' }}">{{ message }}</div>
        {% endif %}
        
        {% if not success %}
        <form method="POST">
            <div class="form-group">
                <input type="text" name="token" 
                       placeholder="Enter your token" 
                       required autofocus>
            </div>
            <button type="submit">Activate Internet</button>
        </form>
        {% endif %}
        
        <div class="info-box">
            <h3>How it works:</h3>
            <ol>
                <li>Drop recyclable trash at the station</li>
                <li>Get a token on the display</li>
                <li>Enter token above for internet access</li>
            </ol>
            <p><strong>Time rates:</strong></p>
            <ul>
                <li>Paper/Paper Cup/Cardboard: 15 min</li>
                <li>Styrofoam/Plastic: 20 min</li>
                <li>Plastic Bottle: 30 min</li>
            </ul>
        </div>
        
        <div class="footer">
            Drop trash • Earn WiFi • Recycle
        </div>
    </div>
</body>
</html>
"""

class TokenManager:
    def __init__(self):
        self.tokens = {}
        self.active_macs = {}  # mac -> {token, expiry}
        self.load_tokens()
    
    def load_tokens(self):
        try:
            if os.path.exists(TOKEN_FILE):
                with open(TOKEN_FILE, 'r') as f:
                    data = json.load(f)
                    self.tokens = data.get('tokens', {})
                    self.active_macs = data.get('active_macs', {})
                print(f"✅ Loaded {len(self.tokens)} tokens and {len(self.active_macs)} active MACs")
        except:
            self.tokens = {}
            self.active_macs = {}
    
    def save_tokens(self):
        try:
            data = {
                'tokens': self.tokens,
                'active_macs': self.active_macs
            }
            with open(TOKEN_FILE, 'w') as f:
                json.dump(data, f, indent=2)
        except:
            pass
    
    def add_token(self, token, trash_type, minutes):
        expiry = (datetime.now() + timedelta(minutes=minutes)).isoformat()
        self.tokens[token] = {
            'trash_type': trash_type,
            'minutes': minutes,
            'expiry': expiry,
            'used': False,
            'mac': None
        }
        self.save_tokens()
        print(f"✅ New token: {token} ({trash_type}, {minutes} min)")
    
    def check_mac_access(self, mac):
        """Check if MAC already has active internet"""
        if mac in self.active_macs:
            expiry = datetime.fromisoformat(self.active_macs[mac]['expiry'])
            if datetime.now() < expiry:
                return True, self.active_macs[mac]['token']
            else:
                # Remove expired MAC
                del self.active_macs[mac]
                self.save_tokens()
        return False, None
    
    def validate_token(self, token, mac):
        # First check if MAC already has active access
        has_access, active_token = self.check_mac_access(mac)
        if has_access:
            return True, self.tokens[active_token], active_token, "already_active"
        
        # Check if token exists
        if token not in self.tokens:
            return False, None, None, "invalid"
        
        data = self.tokens[token]
        
        # Check if token already used by another MAC
        if data.get('used', False) and data.get('mac') != mac:
            return False, None, None, "already_used"
        
        # Check if expired
        expiry = datetime.fromisoformat(data['expiry'])
        if datetime.now() > expiry:
            return False, None, None, "expired"
        
        return True, data, token, "valid"
    
    def activate_token(self, token, mac):
        if token in self.tokens:
            # Mark token as used
            self.tokens[token]['used'] = True
            self.tokens[token]['mac'] = mac
            
            # Add to active MACs
            self.active_macs[mac] = {
                'token': token,
                'expiry': self.tokens[token]['expiry']
            }
            
            self.save_tokens()
            return True
        return False
    
class SerialReader(threading.Thread):
    def __init__(self, token_manager):
        threading.Thread.__init__(self)
        self.token_manager = token_manager
        self.running = True
        self.serial = None
    
    def run(self):
        try:
            print(f"🔌 Attempting to connect to {SERIAL_PORT}...")
            print(f"   File exists: {os.path.exists(SERIAL_PORT)}")
            print(f"   Readable: {os.access(SERIAL_PORT, os.R_OK)}")
            print(f"   Writable: {os.access(SERIAL_PORT, os.W_OK)}")
            
            self.serial = serial.Serial(
                SERIAL_PORT, 
                BAUD_RATE, 
                timeout=1,
                dsrdtr=False,
                rtscts=False,
                exclusive=False
            )
            
            if self.serial.is_open:
                print(f"✅ Connected to ESP32 on {SERIAL_PORT}")
                print(f"   Settings: {self.serial.get_settings()}")
            else:
                print(f"❌ Port opened but not connected?")

            
            while self.running:
                try:
                    if self.serial.in_waiting:
                        line = self.serial.readline().decode('utf-8').strip()
                        if line and line.startswith("TOKEN:"):
                            parts = line.split(":")
                            if len(parts) == 4:
                                token = parts[1]
                                trash_type = parts[2]
                                minutes = int(parts[3])
                                self.token_manager.add_token(token, trash_type, minutes)
                                print(f"📨 Received token: {token}")
                except Exception as e:
                    print(f"Serial read error: {e}")
                time.sleep(0.1)
        except Exception as e:
            print(f"❌ Serial error: {e}")
    
    def stop(self):
        self.running = False
        if self.serial:
            self.serial.close()

def allow_internet(mac, token_data):
    """Allow internet access for a MAC address"""
    expiry = datetime.fromisoformat(token_data['expiry'])
    minutes_left = int((expiry - datetime.now()).total_seconds() / 60)
    seconds_left = int((expiry - datetime.now()).total_seconds())
    
    print(f"⏱️  {mac} has {minutes_left} minutes ({seconds_left} seconds) of internet")
    
    # Remove any existing rule for this MAC
    subprocess.run([
        "iptables", "-D", "FORWARD", "-i", HOTSPOT_INTERFACE,
        "-o", WAN_INTERFACE, "-m", "mac", "--mac-source", mac,
        "-j", "ACCEPT"
    ], check=False, stderr=subprocess.DEVNULL)
    
    # Add new rule at position 1 (before DROP)
    result = subprocess.run([
        "iptables", "-I", "FORWARD", "1", "-i", HOTSPOT_INTERFACE,
        "-o", WAN_INTERFACE, "-m", "mac", "--mac-source", mac,
        "-j", "ACCEPT"
    ], capture_output=True, text=True)
    
    if result.returncode == 0:
        print(f"🌐 Internet allowed for {mac} until {expiry.strftime('%H:%M:%S')}")
    else:
        print(f"❌ Failed to add iptables rule: {result.stderr}")
    
    # Schedule removal and disconnection
        # Schedule removal and disconnection
    def remove_access():
        # Wait until expiry
        wait_seconds = max(1, seconds_left)
        print(f"⏰ Timer set for {mac}: removing internet in {wait_seconds} seconds")
        time.sleep(wait_seconds)
        
        # Remove iptables ACCEPT rule
        subprocess.run([
            "iptables", "-D", "FORWARD", "-i", HOTSPOT_INTERFACE,
            "-o", WAN_INTERFACE, "-m", "mac", "--mac-source", mac,
            "-j", "ACCEPT"
        ], check=False)
        
        # Add a DROP rule for this MAC (optional, but keeps them blocked)
        subprocess.run([
            "iptables", "-I", "FORWARD", "2", "-i", HOTSPOT_INTERFACE,
            "-o", WAN_INTERFACE, "-m", "mac", "--mac-source", mac,
            "-j", "DROP"
        ], check=False)
        
        print(f"🔒 Internet blocked for {mac} - token expired")
        print(f"📱 Phone can still connect to WiFi but has no internet")
        
        # Remove from active MACs
        if mac in token_manager.active_macs:
            del token_manager.active_macs[mac]
            token_manager.save_tokens()
        
        print(f"✅ Internet removed for {mac} - phone remains connected to WiFi")
    
    threading.Thread(target=remove_access, daemon=True).start()
    
def cleanup_expired_sessions():
    """Check for expired sessions and remove internet access"""
    print("🧹 Cleaning up expired sessions...")
    now = datetime.now()
    expired_macs = []
    
    for mac, data in token_manager.active_macs.items():
        expiry = datetime.fromisoformat(data['expiry'])
        if now > expiry:
            expired_macs.append(mac)
            print(f"  Found expired: {mac}")
            
            # Remove iptables ACCEPT rule
            subprocess.run([
                "iptables", "-D", "FORWARD", "-i", HOTSPOT_INTERFACE,
                "-o", WAN_INTERFACE, "-m", "mac", "--mac-source", mac,
                "-j", "ACCEPT"
            ], check=False, stderr=subprocess.DEVNULL)
            
            # Add DROP rule to ensure they stay blocked
            subprocess.run([
                "iptables", "-I", "FORWARD", "2", "-i", HOTSPOT_INTERFACE,
                "-o", WAN_INTERFACE, "-m", "mac", "--mac-source", mac,
                "-j", "DROP"
            ], check=False, stderr=subprocess.DEVNULL)
    
    # Remove from active_macs
    for mac in expired_macs:
        del token_manager.active_macs[mac]
    
    if expired_macs:
        token_manager.save_tokens()
        print(f"✅ Cleaned up {len(expired_macs)} expired sessions")
    
def get_phone_mac(ip):
    """Get MAC address of phone from leases file"""
    try:
        if os.path.exists(LEASES_FILE):
            with open(LEASES_FILE, 'r') as f:
                for line in f:
                    if ip in line:
                        parts = line.strip().split()
                        if len(parts) >= 3:
                            return parts[1]  # MAC is second field
    except:
        pass
    return None
# Flask routes
@app.route('/', methods=['GET', 'POST'])
def index():
    # Get client IP and MAC
    client_ip = request.remote_addr
    client_mac = get_phone_mac(client_ip)
    
    if not client_mac:
        return render_template_string(LOGIN_PAGE, 
            message="Could not identify your device. Please reconnect to WiFi.",
            success=False)
    
    # Check if MAC already has active internet
    has_access, active_token = token_manager.check_mac_access(client_mac)
    if has_access:
        token_data = token_manager.tokens[active_token]
        expiry = datetime.fromisoformat(token_data['expiry'])
        minutes_left = int((expiry - datetime.now()).total_seconds() / 60)
        return render_template_string(LOGIN_PAGE,
            message=f"✅ Already connected! {minutes_left} minutes remaining.",
            success=True)
    
    if request.method == 'POST':
        token = request.form.get('token', '').strip().upper()
        
        # Validate token
        valid, data, used_token, reason = token_manager.validate_token(token, client_mac)
        
        if valid:
            # Activate token for this MAC
            token_manager.activate_token(used_token, client_mac)
            
            # Allow internet for this MAC
            allow_internet(client_mac, data)
            
            expiry = datetime.fromisoformat(data['expiry'])
            minutes_left = int((expiry - datetime.now()).total_seconds() / 60)
            
            return render_template_string(LOGIN_PAGE,
                message=f"✅ Internet activated! {minutes_left} minutes remaining.",
                success=True)
        else:
            messages = {
                "invalid": "❌ Invalid token",
                "already_used": "❌ Token already used by another device",
                "expired": "❌ Token expired"
            }
            return render_template_string(LOGIN_PAGE,
                message=messages.get(reason, "❌ Error"),
                success=False)
    
    return render_template_string(LOGIN_PAGE)

# Main
if __name__ == "__main__":
    print("=" * 50)
    print("Segri Green Token Server")
    print("=" * 50)
    
    # Initialize token manager
    token_manager = TokenManager()
    
    # Clean up any expired sessions
    cleanup_expired_sessions() 
    
    # Start serial reader
    serial_reader = SerialReader(token_manager)
    serial_reader.daemon = True
    serial_reader.start()
    
    print("🚀 Starting web server on port 80...")
    print("🌐 http://192.168.4.1")
    print("Press Ctrl+C to stop\n")
    
    try:
        app.run(host='0.0.0.0', port=80, debug=False, threaded=True)
    except KeyboardInterrupt:
        print("\n🛑 Shutting down...")
        serial_reader.stop()