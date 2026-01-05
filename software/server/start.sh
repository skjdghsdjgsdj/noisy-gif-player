#!/bin/bash

# Start the Flask application with Gunicorn for production use
# Timeout set to 600 seconds (10 minutes) to handle long video conversions

gunicorn --bind 0.0.0.0:5000 --timeout 600 wsgi:app
