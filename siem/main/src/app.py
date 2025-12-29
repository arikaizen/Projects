from flask import Flask, render_template, request, jsonify
import os
import sys
import importlib.util
from pathlib import Path

app = Flask(__name__)

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


if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
