import os
import sys
import time
import socket
import serial

BOOT_FILE = '/tmp/postmarketOS-export/boot.img'
ROOTFS_FILE = '/tmp/pmos-rootfs.ext4'
DEVICE_TTY = '/dev/ttyACM0'
HOST_IP = '172.16.42.2'
PORT = 5555
CHUNK_SIZE = 4 * 1024 * 1024  # 4MB chunks

def stream_file(ser, filepath, target_partition, completion_marker):
    file_size = os.path.getsize(filepath)
    print(f"\n[*] Target: {target_partition} | Source: {filepath} ({file_size / (1024**2):.1f} MB)")
    
    # Start server socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST_IP, PORT))
    server.listen(1)
    
    # Send command to device
    cmd = f"nc {HOST_IP} {PORT} < /dev/null > {target_partition} && sync && echo '{completion_marker}'\n"
    ser.write(cmd.encode('utf-8'))
    
    conn, client_addr = server.accept()
    print(f"[+] Phone connected from {client_addr} for {target_partition}")
    
    t_start = time.time()
    sent = 0
    last_report = t_start
    
    with open(filepath, 'rb') as f:
        while True:
            chunk = f.read(CHUNK_SIZE)
            if not chunk:
                break
            conn.sendall(chunk)
            sent += len(chunk)
            now = time.time()
            if now - last_report >= 2.0 or sent == file_size:
                pct = (sent / file_size) * 100
                elapsed = now - t_start
                speed = (sent / (1024 * 1024)) / elapsed if elapsed > 0 else 0
                eta = (file_size - sent) / (speed * 1024 * 1024) if speed > 0 else 0
                print(f"    Progress: {sent / (1024**2):.1f}/{file_size / (1024**2):.1f} MB ({pct:5.1f}%) | Speed: {speed:5.1f} MB/s | ETA: {eta:4.0f}s")
                last_report = now
                
    conn.shutdown(socket.SHUT_WR)
    conn.close()
    server.close()
    
    total_time = time.time() - t_start
    avg_speed = (file_size / (1024 * 1024)) / total_time
    print(f"[+] Sent in {total_time:.1f}s (Average speed: {avg_speed:.1f} MB/s)")
    
    print("[*] Waiting for phone to sync...")
    deadline = time.time() + 90
    buf = ""
    done = False
    while time.time() < deadline:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            buf += chunk
            sys.stdout.write(chunk)
            sys.stdout.flush()
            if completion_marker in buf:
                done = True
                break
        time.sleep(0.3)
        
    if not done:
        print(f"\n[-] Timeout waiting for {completion_marker}!")
        sys.exit(1)
    print(f"\n[+] Successfully wrote to {target_partition}!")

# Main execution
print("=== NURA OS FLASHING PIPELINE ===")
ser = serial.Serial(DEVICE_TTY, 115200, timeout=2)
ser.write(b'\n\n')
time.sleep(0.5)
ser.read(ser.in_waiting or 1)

# 1. Flash boot.img to /dev/sdf18 (boot_b)
stream_file(ser, BOOT_FILE, '/dev/sdf18', 'FLASH_BOOT_COMPLETED')

# 2. Flash rootfs to /dev/sde26 (userdata)
stream_file(ser, ROOTFS_FILE, '/dev/sde26', 'FLASH_ROOTFS_COMPLETED')

# 3. Resize rootfs
print("\n[*] Running e2fsck and resize2fs on /dev/sde26...")
post_cmds = "e2fsck -f -y /dev/sde26 && resize2fs /dev/sde26 && sync && echo 'RESIZE_AND_SYNC_COMPLETED'\n"
ser.write(post_cmds.encode('utf-8'))

buf = ""
deadline = time.time() + 90
while time.time() < deadline:
    if ser.in_waiting:
        chunk = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        buf += chunk
        sys.stdout.write(chunk)
        sys.stdout.flush()
        if 'RESIZE_AND_SYNC_COMPLETED' in buf:
            break
    time.sleep(0.5)

print("\n[*] Triggering reboot into Nura OS...")
ser.write(b"sync; echo b > /proc/sysrq-trigger\n")
time.sleep(1)
ser.close()
print("[+] Reboot signal sent! Phone is booting into Nura OS.")
