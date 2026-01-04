from flask import Flask, render_template, request, jsonify
import os
import sys
import importlib.util
from pathlib import Path
import socket
import threading
import json
from datetime import datetime

app = Flask(__name__)

# Log storage (in-memory for now)
received_logs = []
MAX_LOGS = 10000  # Keep last 10000 logs

# App discovery and loading
APPS_DIR = Path(__file__).parent.parent / 'apps'
loaded_apps = {}


def discover_apps():
    """
    Discover and load apps from the apps folder.

    Returns:
        dict: Dictionary of loaded apps {app_id: app_instance}
    """
    apps = {}

    if not APPS_DIR.exists():
        print(f"Apps directory not found: {APPS_DIR}")
        return apps

    # Add apps directory to Python path
    sys.path.insert(0, str(APPS_DIR.parent))

    # Scan for Python files in apps directory
    for app_file in APPS_DIR.glob('*.py'):
        if app_file.name.startswith('_'):
            continue

        try:
            # Import the module
            module_name = app_file.stem
            spec = importlib.util.spec_from_file_location(
                f"apps.{module_name}",
                app_file
            )

            if spec and spec.loader:
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)

                # Look for an 'app' instance in the module
                if hasattr(module, 'app'):
                    app_instance = module.app
                    apps[app_instance.id] = app_instance
                    print(f"✓ Loaded app: {app_instance.name} ({app_instance.id})")

        except Exception as e:
            print(f"✗ Failed to load app {app_file.name}: {e}")

    return apps


# Load apps on startup
loaded_apps = discover_apps()
print(f"\nTotal apps loaded: {len(loaded_apps)}\n")


@app.route('/')
def home():
    """Main dashboard page"""
    return render_template('index.html')

@app.route('/api/status')
def status():
    """API endpoint for system status"""
    return jsonify({
        'status': 'running',
        'message': 'SIEM system operational'
    })

@app.route('/api/data', methods=['GET', 'POST'])
def data():
    """API endpoint for data operations"""
    if request.method == 'POST':
        data = request.get_json()
        return jsonify({
            'status': 'success',
            'received': data
        })
    return jsonify({
        'status': 'ready',
        'message': 'Send POST request with data'
    })


@app.route('/api/apps')
def list_apps():
    """API endpoint to list available apps"""
    apps_list = []
    for app_id, app_instance in loaded_apps.items():
        apps_list.append({
            'id': app_instance.id,
            'name': app_instance.name,
            'description': app_instance.description,
            'icon': app_instance.icon
        })
    return jsonify({'apps': apps_list})


@app.route('/api/apps/<app_id>')
def get_app(app_id):
    """API endpoint to get app content"""
    if app_id not in loaded_apps:
        return jsonify({'error': 'App not found'}), 404

    app_instance = loaded_apps[app_id]
    content = app_instance.render()

    return jsonify({
        'id': app_instance.id,
        'name': app_instance.name,
        'content': content
    })


@app.route('/api/logs')
def get_logs():
    """API endpoint to retrieve received logs"""
    limit = request.args.get('limit', 100, type=int)
    search_query = request.args.get('q', '', type=str)

    logs = received_logs[-limit:]  # Get last N logs

    # Filter by search query if provided
    if search_query:
        logs = [log for log in logs if search_query.lower() in json.dumps(log).lower()]

    return jsonify({
        'total': len(received_logs),
        'returned': len(logs),
        'logs': logs
    })


def handle_forwarder_connection(client_socket, client_address):
    """Handle incoming connection from a log forwarder"""
    print(f"[Log Receiver] New connection from {client_address}")

    buffer = ""
    try:
        while True:
            data = client_socket.recv(4096)
            if not data:
                break

            buffer += data.decode('utf-8', errors='ignore')

            # Process complete lines (JSON objects)
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                line = line.strip()

                if line:
                    try:
                        log_entry = json.loads(line)
                        log_entry['received_at'] = datetime.now().isoformat()
                        log_entry['source'] = client_address[0]

                        received_logs.append(log_entry)

                        # Maintain max log limit
                        if len(received_logs) > MAX_LOGS:
                            received_logs.pop(0)

                        print(f"[Log Receiver] Received log: {line[:100]}...")
                    except json.JSONDecodeError as e:
                        print(f"[Log Receiver] Invalid JSON: {e}")

    except Exception as e:
        print(f"[Log Receiver] Error handling connection: {e}")
    finally:
        client_socket.close()
        print(f"[Log Receiver] Connection closed from {client_address}")


def start_log_receiver(port=8089):
    """Start TCP server to receive logs from forwarders"""
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server_socket.bind(('0.0.0.0', port))
        server_socket.listen(5)
        print(f"[Log Receiver] Listening on port {port} for incoming logs...")

        while True:
            client_socket, client_address = server_socket.accept()
            client_thread = threading.Thread(
                target=handle_forwarder_connection,
                args=(client_socket, client_address),
                daemon=True
            )
            client_thread.start()

    except Exception as e:
        print(f"[Log Receiver] Error: {e}")
    finally:
        server_socket.close()


if __name__ == '__main__':
    # Start log receiver in background thread
    log_receiver_thread = threading.Thread(target=start_log_receiver, args=(8089,), daemon=True)
    log_receiver_thread.start()

    print("\n" + "="*50)
    print("SIEM Server Starting")
    print("="*50)
    print(f"Web Interface: http://0.0.0.0:5000")
    print(f"Log Receiver: port 8089")
    print(f"Apps Loaded: {len(loaded_apps)}")
    print("="*50 + "\n")

    app.run(debug=True, host='0.0.0.0', port=5000, use_reloader=False)
