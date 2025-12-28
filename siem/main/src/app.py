from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

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

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
