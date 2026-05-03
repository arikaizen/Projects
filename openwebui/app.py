import os
import json
import uuid
import sqlite3
import requests
from datetime import datetime
from flask import Flask, request, jsonify, render_template, Response, stream_with_context, send_from_directory
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

DB_PATH = os.path.join(os.path.dirname(__file__), 'data', 'webui.db')
UPLOAD_FOLDER = os.path.join(os.path.dirname(__file__), 'data', 'uploads')
os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
os.makedirs(UPLOAD_FOLDER, exist_ok=True)


def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = get_db()
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS conversations (
        id TEXT PRIMARY KEY,
        title TEXT NOT NULL,
        model TEXT,
        system_prompt TEXT DEFAULT '',
        created_at TEXT,
        updated_at TEXT
    )''')
    c.execute('''CREATE TABLE IF NOT EXISTS messages (
        id TEXT PRIMARY KEY,
        conversation_id TEXT,
        role TEXT,
        content TEXT,
        images TEXT DEFAULT '[]',
        created_at TEXT,
        FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE
    )''')
    c.execute('''CREATE TABLE IF NOT EXISTS settings (
        key TEXT PRIMARY KEY,
        value TEXT
    )''')
    defaults = {
        'ollama_url': 'http://localhost:11434',
        'openai_api_key': '',
        'openai_base_url': 'https://api.openai.com/v1',
        'anthropic_api_key': '',
        'default_model': '',
        'theme': 'dark',
        'temperature': '0.7',
        'top_p': '0.9',
        'max_tokens': '4096',
    }
    for key, value in defaults.items():
        c.execute('INSERT OR IGNORE INTO settings (key, value) VALUES (?, ?)', (key, value))
    conn.commit()
    conn.close()


init_db()


def get_settings():
    conn = get_db()
    rows = conn.execute('SELECT key, value FROM settings').fetchall()
    conn.close()
    return {row['key']: row['value'] for row in rows}


def detect_provider(model):
    m = model.lower()
    if 'claude' in m:
        return 'anthropic'
    if any(x in m for x in ['gpt-', 'o1-', 'o3-', 'o4-', 'text-davinci']):
        return 'openai'
    return 'ollama'


# ---------- Models ----------

@app.route('/api/models', methods=['GET'])
def list_models():
    settings = get_settings()
    models = []

    # Ollama
    try:
        resp = requests.get(
            f"{settings['ollama_url'].rstrip('/')}/api/tags", timeout=3
        )
        if resp.ok:
            for m in resp.json().get('models', []):
                models.append({
                    'id': m['name'],
                    'name': m['name'],
                    'provider': 'ollama',
                    'size': m.get('size', 0),
                    'details': m.get('details', {}),
                })
    except Exception:
        pass

    # OpenAI
    if settings.get('openai_api_key'):
        try:
            base = settings.get('openai_base_url', 'https://api.openai.com/v1').rstrip('/')
            headers = {'Authorization': f"Bearer {settings['openai_api_key']}"}
            resp = requests.get(f"{base}/models", headers=headers, timeout=5)
            if resp.ok:
                for m in resp.json().get('data', []):
                    mid = m['id']
                    if any(x in mid for x in ['gpt', 'o1', 'o3', 'o4']):
                        models.append({'id': mid, 'name': mid, 'provider': 'openai'})
        except Exception:
            pass

    # Anthropic (static list)
    if settings.get('anthropic_api_key'):
        for mid in [
            'claude-opus-4-7',
            'claude-sonnet-4-6',
            'claude-haiku-4-5-20251001',
            'claude-3-5-sonnet-20241022',
            'claude-3-5-haiku-20241022',
        ]:
            models.append({'id': mid, 'name': mid, 'provider': 'anthropic'})

    return jsonify({'models': models})


# ---------- Chat (streaming) ----------

@app.route('/api/chat', methods=['POST'])
def chat():
    data = request.json
    model = data.get('model', '')
    messages = data.get('messages', [])
    system_prompt = data.get('system_prompt', '')
    provider = data.get('provider') or detect_provider(model)
    settings = get_settings()

    def generate_ollama():
        url = f"{settings['ollama_url'].rstrip('/')}/api/chat"
        payload = {
            'model': model,
            'messages': messages,
            'stream': True,
            'options': {
                'temperature': float(settings.get('temperature', 0.7)),
                'top_p': float(settings.get('top_p', 0.9)),
            },
        }
        if system_prompt:
            payload['system'] = system_prompt
        try:
            with requests.post(url, json=payload, stream=True, timeout=120) as resp:
                full = ''
                for line in resp.iter_lines():
                    if line:
                        chunk = json.loads(line)
                        content = chunk.get('message', {}).get('content', '')
                        if content:
                            full += content
                            yield f"data: {json.dumps({'content': content, 'done': False})}\n\n"
                        if chunk.get('done'):
                            yield f"data: {json.dumps({'content': '', 'done': True, 'full_content': full})}\n\n"
        except Exception as e:
            yield f"data: {json.dumps({'error': str(e), 'done': True})}\n\n"

    def generate_openai():
        base = settings.get('openai_base_url', 'https://api.openai.com/v1').rstrip('/')
        headers = {
            'Authorization': f"Bearer {settings['openai_api_key']}",
            'Content-Type': 'application/json',
        }
        msgs = []
        if system_prompt:
            msgs.append({'role': 'system', 'content': system_prompt})
        msgs.extend(messages)
        payload = {
            'model': model,
            'messages': msgs,
            'stream': True,
            'temperature': float(settings.get('temperature', 0.7)),
            'max_tokens': int(settings.get('max_tokens', 4096)),
        }
        try:
            with requests.post(f"{base}/chat/completions", headers=headers, json=payload, stream=True, timeout=120) as resp:
                full = ''
                for line in resp.iter_lines():
                    if not line:
                        continue
                    text = line.decode('utf-8') if isinstance(line, bytes) else line
                    if text.startswith('data: '):
                        text = text[6:]
                    if text == '[DONE]':
                        yield f"data: {json.dumps({'content': '', 'done': True, 'full_content': full})}\n\n"
                        break
                    try:
                        chunk = json.loads(text)
                        content = chunk.get('choices', [{}])[0].get('delta', {}).get('content', '')
                        if content:
                            full += content
                            yield f"data: {json.dumps({'content': content, 'done': False})}\n\n"
                    except json.JSONDecodeError:
                        pass
        except Exception as e:
            yield f"data: {json.dumps({'error': str(e), 'done': True})}\n\n"

    def generate_anthropic():
        headers = {
            'x-api-key': settings['anthropic_api_key'],
            'anthropic-version': '2023-06-01',
            'content-type': 'application/json',
        }
        msgs = [m for m in messages if m.get('role') != 'system']
        payload = {
            'model': model,
            'max_tokens': int(settings.get('max_tokens', 4096)),
            'messages': msgs,
            'stream': True,
        }
        if system_prompt:
            payload['system'] = system_prompt
        try:
            with requests.post('https://api.anthropic.com/v1/messages', headers=headers, json=payload, stream=True, timeout=120) as resp:
                full = ''
                for line in resp.iter_lines():
                    if not line:
                        continue
                    text = line.decode('utf-8') if isinstance(line, bytes) else line
                    if text.startswith('data: '):
                        text = text[6:]
                    try:
                        event = json.loads(text)
                        if event.get('type') == 'content_block_delta':
                            content = event.get('delta', {}).get('text', '')
                            if content:
                                full += content
                                yield f"data: {json.dumps({'content': content, 'done': False})}\n\n"
                        elif event.get('type') == 'message_stop':
                            yield f"data: {json.dumps({'content': '', 'done': True, 'full_content': full})}\n\n"
                    except json.JSONDecodeError:
                        pass
        except Exception as e:
            yield f"data: {json.dumps({'error': str(e), 'done': True})}\n\n"

    generators = {
        'anthropic': generate_anthropic,
        'openai': generate_openai,
    }
    generator = generators.get(provider, generate_ollama)()

    return Response(
        stream_with_context(generator),
        mimetype='text/event-stream',
        headers={'Cache-Control': 'no-cache', 'X-Accel-Buffering': 'no'},
    )


# ---------- Conversations ----------

@app.route('/api/conversations', methods=['GET'])
def list_conversations():
    conn = get_db()
    rows = conn.execute('SELECT * FROM conversations ORDER BY updated_at DESC').fetchall()
    conn.close()
    return jsonify([dict(r) for r in rows])


@app.route('/api/conversations', methods=['POST'])
def create_conversation():
    data = request.json or {}
    conn = get_db()
    cid = str(uuid.uuid4())
    now = datetime.utcnow().isoformat()
    conn.execute(
        'INSERT INTO conversations (id, title, model, system_prompt, created_at, updated_at) VALUES (?,?,?,?,?,?)',
        (cid, data.get('title', 'New Chat'), data.get('model', ''), data.get('system_prompt', ''), now, now),
    )
    conn.commit()
    row = conn.execute('SELECT * FROM conversations WHERE id = ?', (cid,)).fetchone()
    conn.close()
    return jsonify(dict(row)), 201


@app.route('/api/conversations/<cid>', methods=['GET'])
def get_conversation(cid):
    conn = get_db()
    conv = conn.execute('SELECT * FROM conversations WHERE id = ?', (cid,)).fetchone()
    if not conv:
        return jsonify({'error': 'Not found'}), 404
    msgs = conn.execute(
        'SELECT * FROM messages WHERE conversation_id = ? ORDER BY created_at ASC', (cid,)
    ).fetchall()
    conn.close()
    result = dict(conv)
    result['messages'] = [dict(m) for m in msgs]
    return jsonify(result)


@app.route('/api/conversations/<cid>', methods=['PUT'])
def update_conversation(cid):
    data = request.json or {}
    conn = get_db()
    now = datetime.utcnow().isoformat()
    fields, values = [], []
    for f in ('title', 'model', 'system_prompt'):
        if f in data:
            fields.append(f'{f} = ?')
            values.append(data[f])
    if fields:
        fields.append('updated_at = ?')
        values += [now, cid]
        conn.execute(f"UPDATE conversations SET {', '.join(fields)} WHERE id = ?", values)
        conn.commit()
    row = conn.execute('SELECT * FROM conversations WHERE id = ?', (cid,)).fetchone()
    conn.close()
    return jsonify(dict(row))


@app.route('/api/conversations/<cid>', methods=['DELETE'])
def delete_conversation(cid):
    conn = get_db()
    conn.execute('DELETE FROM messages WHERE conversation_id = ?', (cid,))
    conn.execute('DELETE FROM conversations WHERE id = ?', (cid,))
    conn.commit()
    conn.close()
    return '', 204


@app.route('/api/conversations/<cid>/messages', methods=['POST'])
def add_message(cid):
    data = request.json or {}
    conn = get_db()
    mid = str(uuid.uuid4())
    now = datetime.utcnow().isoformat()
    conn.execute(
        'INSERT INTO messages (id, conversation_id, role, content, images, created_at) VALUES (?,?,?,?,?,?)',
        (mid, cid, data['role'], data['content'], json.dumps(data.get('images', [])), now),
    )
    conn.execute('UPDATE conversations SET updated_at = ? WHERE id = ?', (now, cid))
    conn.commit()
    row = conn.execute('SELECT * FROM messages WHERE id = ?', (mid,)).fetchone()
    conn.close()
    return jsonify(dict(row)), 201


# ---------- Settings ----------

@app.route('/api/settings', methods=['GET'])
def get_settings_route():
    s = get_settings()
    if s.get('openai_api_key'):
        s['openai_api_key_masked'] = '••••' + s['openai_api_key'][-4:]
    if s.get('anthropic_api_key'):
        s['anthropic_api_key_masked'] = '••••' + s['anthropic_api_key'][-4:]
    return jsonify(s)


@app.route('/api/settings', methods=['PUT'])
def update_settings():
    data = request.json or {}
    conn = get_db()
    for key, value in data.items():
        conn.execute('INSERT OR REPLACE INTO settings (key, value) VALUES (?,?)', (key, str(value)))
    conn.commit()
    conn.close()
    return jsonify({'status': 'ok'})


# ---------- File upload ----------

@app.route('/api/upload', methods=['POST'])
def upload_file():
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400
    f = request.files['file']
    filename = f"{uuid.uuid4()}_{f.filename}"
    f.save(os.path.join(UPLOAD_FOLDER, filename))
    return jsonify({'filename': filename, 'url': f'/uploads/{filename}'})


@app.route('/uploads/<filename>')
def serve_upload(filename):
    return send_from_directory(UPLOAD_FOLDER, filename)


# ---------- Generate title ----------

@app.route('/api/generate-title', methods=['POST'])
def generate_title():
    data = request.json or {}
    first_message = data.get('message', '')
    words = first_message.strip().split()
    title = ' '.join(words[:8]) + ('...' if len(words) > 8 else '')
    return jsonify({'title': title or 'New Chat'})


# ---------- Main ----------

@app.route('/')
def index():
    return render_template('index.html')


if __name__ == '__main__':
    print("OpenWebUI running at http://localhost:3000")
    app.run(debug=True, host='0.0.0.0', port=3000, threaded=True)
