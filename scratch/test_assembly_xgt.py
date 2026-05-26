import socket
import time
import sys

def print_hex(label, data):
    print(f"{label}: {' '.join(f'{b:02X}' for b in data)}")

def test_assembly_xgt():
    print("=== Starting Automotive Assembly Line LS Electric XGT Test ===")
    
    server_ip = "127.0.0.1"
    server_port = 2004
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((server_ip, server_port))
        print(f"[Client] Connected to vPLC XGT Server at {server_ip}:{server_port}")
    except Exception as e:
        print(f"[Error] Failed to connect: {e}")
        print("Please make sure vPLC is running with --assembly first!")
        sys.exit(1)

    # 1. Read Word Register %MW1 (Conveyor Speed)
    # DataType=0200 (Word), VarName="%MW1"
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x01\x00\x10\x00"
    body = b"\x00\x00\x54\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW1"
    req = header + body
    
    s.sendall(req)
    resp = s.recv(1024)
    if len(resp) >= 34:
        val = resp[32] | (resp[33] << 8)
        print(f"[Read] Conveyor Speed (%MW1): {val} mm/s (Default should be 200)")
    else:
        print("[Error] Failed to read %MW1.")

    # 2. WRITE Conveyor Speed (%MW1) to 450 mm/s
    # DataType=0200 (Word), WriteDataLen=0200, WriteData=C201 (450)
    print("\n[Action] Writing Conveyor Speed (%MW1) = 450 mm/s through XGT Protocol...")
    header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x02\x00\x14\x00"
    body = b"\x00\x00\x58\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW1\x02\x00\xC2\x01"
    req = header + body
    
    s.sendall(req)
    resp = s.recv(1024)
    if len(resp) >= 30:
        status = resp[26] | (resp[27] << 8)
        if status == 0:
            print("[Success] Written Speed = 450 mm/s. LOOK AT THE TUI DASHBOARD! The conveyor roller speed will accelerate!")
        else:
            print(f"[Error] Error status returned: {status:04X}")

    # 3. Monitor real-time completed cars
    print("\n[Action] Monitoring real-time completed cars count on XGT for 5 seconds...")
    for i in range(10):
        time.sleep(0.5)
        # Read Completed Cars (%MW0)
        header = b"LSIS-XGT\x00\x00\x00\x00\xA0\x33\x03\x00\x10\x00"
        body = b"\x00\x00\x54\x00\x02\x00\x00\x00\x01\x00\x04\x00%MW0"
        req = header + body
        s.sendall(req)
        resp = s.recv(1024)
        if len(resp) >= 34:
            comp = resp[32] | (resp[33] << 8)
            print(f"  -> [{i*0.5:.1f}s] Completed Cars: {comp}")

    s.close()
    print("\n=== XGT Server Automotive Simulation Test Completed Gracefully! ===")

if __name__ == "__main__":
    test_assembly_xgt()
