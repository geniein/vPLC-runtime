import socket
import time
import sys

def print_hex(label, data):
    print(f"{label}: {' '.join(f'{b:02X}' for b in data)}")

def test_assembly_mc():
    print("=== Starting Automotive Assembly Line Mitsubishi MC Test ===")
    
    server_ip = "127.0.0.1"
    server_port = 5011
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((server_ip, server_port))
        print(f"[Client] Connected to vPLC MC Server at {server_ip}:{server_port}")
    except Exception as e:
        print(f"[Error] Failed to connect: {e}")
        print("Please make sure vPLC is running with --assembly first!")
        sys.exit(1)

    # 1. Batch Read D0, D1 registers (D0=%MW0: Completed Cars, D1=%MW1: Speed)
    # Header subheader=5000, length=0C00
    req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0C\x00\x10\x00\x01\x04\x00\x00\x00\x00\x00\xA8\x02\x00"
    s.sendall(req)
    resp = s.recv(1024)
    if len(resp) >= 15:
        val0 = resp[11] | (resp[12] << 8) # Completed Cars
        val1 = resp[13] | (resp[14] << 8) # Conveyor Speed
        print(f"[Read] Completed Cars (D0): {val0}")
        print(f"[Read] Conveyor Speed (D1): {val1} mm/s (Default should be 200)")
    else:
        print("[Error] Failed to batch read D registers.")

    # 2. WRITE Conveyor Speed (D1) to 600 mm/s
    # Header subheader=5000, Cmd=0114 (Batch Write), Data=5802 (600)
    print("\n[Action] Writing Conveyor Speed (D1) = 600 mm/s through MC Protocol...")
    req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0E\x00\x10\x00\x01\x14\x00\x00\x01\x00\x00\xA8\x01\x00\x58\x02"
    s.sendall(req)
    resp = s.recv(1024)
    if len(resp) >= 11:
        end_code = resp[9] | (resp[10] << 8)
        if end_code == 0:
            print("[Success] Written Speed = 600 mm/s. LOOK AT THE TUI DASHBOARD! The conveyor roller speed will accelerate!")
        else:
            print(f"[Error] End Code: {end_code:04X}")

    # 3. Monitor real-time changes
    print("\n[Action] Monitoring real-time conveyor speed-up on MC for 5 seconds...")
    for i in range(10):
        time.sleep(0.5)
        # Read D0 and D1
        req = b"\x50\x00\x00\xFF\xFF\x03\x00\x0C\x00\x10\x00\x01\x04\x00\x00\x00\x00\x00\xA8\x02\x00"
        s.sendall(req)
        resp = s.recv(1024)
        if len(resp) >= 15:
            comp_cars = resp[11] | (resp[12] << 8)
            print(f"  -> [{i*0.5:.1f}s] Completed Cars: {comp_cars}")

    s.close()
    print("\n=== MC Server Automotive Simulation Test Completed Gracefully! ===")

if __name__ == "__main__":
    test_assembly_mc()
