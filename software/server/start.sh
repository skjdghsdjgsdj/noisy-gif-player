#!/bin/bash

# Start the Flask application with Gunicorn for production use
# Timeout set to 600 seconds (10 minutes) to handle long video conversions

python3 -m venv venv
source venv/bin/activate
pip3 install -r requirements.txt
gunicorn --bind localhost:5000 --timeout 600 wsgi:app
deactivate
