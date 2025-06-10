from flask import Flask, request, send_file
import os
import zipfile
import subprocess
import io
import shutil
import atexit
import signal
from typing import List

app = Flask(__name__)
BASE_DIR = ".vcs"
COMMITS_DIR = os.path.join(BASE_DIR, "commits")
TMP_ZIP = "vcs_temp.zip"
METADATA_FILE = os.path.join(BASE_DIR, "vcs_meta.txt")
PID_FILE = os.path.join(BASE_DIR, "server.pid")

def get_all_ips() -> List[str]:
    """Get all non-loopback IPs reliably."""
    try:
        result = subprocess.run(['ip', 'addr'], capture_output=True, text=True)
        return [
            line.split()[1].split('/')[0]
            for line in result.stdout.split('\n')
            if 'inet ' in line and '127.0.0.1' not in line
        ]
    except Exception:
        return ["127.0.0.1"]

def create_repo_zip():
    """Create zip in memory."""
    memory_file = io.BytesIO()
    with zipfile.ZipFile(memory_file, 'w', zipfile.ZIP_DEFLATED) as zipf:
        if os.path.exists(COMMITS_DIR):
            for root, _, files in os.walk(COMMITS_DIR):
                for file in files:
                    file_path = os.path.join(root, file)
                    arcname = os.path.join(".vcs", os.path.relpath(file_path, BASE_DIR))
                    zipf.write(file_path, arcname)
        
        # Include metadata and PID files
        for meta_file in [METADATA_FILE]:
            if os.path.exists(meta_file):
                zipf.write(meta_file, os.path.join(".vcs", os.path.basename(meta_file)))
    
    memory_file.seek(0)
    return memory_file

def update_metadata(remote_url: str = ""):
    """Update metadata file with server info."""
    os.makedirs(BASE_DIR, exist_ok=True)
    
    # Get current metadata if exists
    metadata = {}
    if os.path.exists(METADATA_FILE):
        with open(METADATA_FILE, 'r') as f:
            for line in f:
                if ':' in line:
                    key, val = line.strip().split(':', 1)
                    metadata[key] = val
    
    # Update with current server info
    if not remote_url:  # For host server
        ips = get_all_ips()
        if ips:
            metadata['remote_url'] = f"{ips[0]}:9000"
    else:  # For client
        metadata['remote_url'] = remote_url
    
    # Write back to file
    with open(METADATA_FILE, 'w') as f:
        for key, val in metadata.items():
            f.write(f"{key}:{val}\n")

def cleanup():
    """Cleanup function for atexit."""
    if os.path.exists(PID_FILE):
        os.remove(PID_FILE)


@app.route('/clone')
def clone():
    try:
        if not os.path.exists(COMMITS_DIR):
            return "No repository found", 404
            
        update_metadata()  # Ensure metadata is current
        zip_buffer = create_repo_zip()
        return send_file(
            zip_buffer,
            as_attachment=True,
            download_name='repo.zip',
            mimetype='application/zip'
        )
    except Exception as e:
        return f"Server error: {str(e)}", 500

@app.route('/pull')
def pull():
    try:
        if not os.path.exists(COMMITS_DIR):
            return "No repository found", 404
            
        zip_buffer = create_repo_zip()
        return send_file(
            zip_buffer,
            as_attachment=True,
            download_name='pull.zip',
            mimetype='application/zip'
        )
    except Exception as e:
        return f"Server error: {str(e)}", 500

@app.route('/push', methods=['POST'])
def push():
    try:
        if 'file' not in request.files:
            return "No file uploaded", 400

        file = request.files['file']
        if file.filename == '':
            return "Empty file", 400

        # Save to temp location
        temp_dir = os.path.join(BASE_DIR, "temp_push")
        os.makedirs(temp_dir, exist_ok=True)
        zip_path = os.path.join(temp_dir, TMP_ZIP)
        file.save(zip_path)

        if not zipfile.is_zipfile(zip_path):
            shutil.rmtree(temp_dir)
            return "Invalid ZIP file", 400

        # Extract and validate
        temp_extract = os.path.join(temp_dir, "extracted")
        with zipfile.ZipFile(zip_path, 'r') as zipf:
            zipf.extractall(temp_extract)

        vcs_dir = os.path.join(temp_extract, ".vcs")
        if not os.path.exists(vcs_dir):
            shutil.rmtree(temp_dir)
            return "Invalid repository structure", 400

        # Update commits
        if os.path.exists(COMMITS_DIR):
            shutil.rmtree(COMMITS_DIR)
        shutil.copytree(
            os.path.join(vcs_dir, "commits"),
            COMMITS_DIR
        )

        # Preserve our server's metadata
        shutil.rmtree(temp_dir)
        return "Push successful", 200

    except Exception as e:
        if 'temp_dir' in locals() and os.path.exists(temp_dir):
            shutil.rmtree(temp_dir)
        return f"Push failed: {str(e)}", 500

def print_connection_info(port: int):
    ip_list = get_all_ips()
    print("\n Repository server ready!")
    print("═══════════════════════════════════════════════")
    print(f"Server PID: {os.getpid()}")
    print("Available endpoints:")
    for ip in ip_list:
        print(f"  http://{ip}:{port}/clone")
        print(f"  http://{ip}:{port}/pull")
        print(f"  POST http://{ip}:{port}/push")
    print("\nUse './vcs stop' to shutdown the server")
    print("═══════════════════════════════════════════════")

def write_server_info(port: int):
    """Write server info to a file."""
    ip_list = get_all_ips()
    if ip_list:
        server_address = f"{ip_list[0]}:{port}"
        with open(os.path.join(BASE_DIR, "server.info"), "w") as f:
            f.write(server_address)

if __name__ == '__main__':
    os.makedirs(BASE_DIR, exist_ok=True)
    os.makedirs(COMMITS_DIR, exist_ok=True)
    
    # Initialize server metadata
    update_metadata()
    # save_pid()
    
    port = 9000
    write_server_info(port)  # Write server.info file
    print_connection_info(port)
    
    # Register cleanup for SIGTERM
    signal.signal(signal.SIGTERM, lambda *args: (cleanup(), exit(0)))
    
    app.run(host='0.0.0.0', port=port, threaded=True)