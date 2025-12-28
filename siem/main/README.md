# SIEM Web UI

A general-purpose Python web UI for the SIEM system built with Flask.

## Setup

1. Install dependencies:
```bash
pip install -r requirements.txt
```

2. Run the application:
```bash
cd src
python app.py
```

3. Open your browser and navigate to:
```
http://localhost:5000
```

## Features

- Dashboard with system status
- Data submission interface (JSON format)
- RESTful API endpoints
- Real-time status updates
- Responsive design

## API Endpoints

- `GET /` - Main dashboard
- `GET /api/status` - System status
- `GET /api/data` - Get data endpoint info
- `POST /api/data` - Submit data (JSON)

## Directory Structure

```
main/
├── src/
│   ├── app.py              # Main Flask application
│   ├── templates/          # HTML templates
│   │   └── index.html
│   └── static/             # Static assets
│       ├── css/
│       │   └── style.css
│       └── js/
│           └── main.js
├── requirements.txt        # Python dependencies
├── tst/                    # Tests
├── inc/                    # Includes
├── bin/                    # Binaries
└── config/                 # Configuration files
```
