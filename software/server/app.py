#!/usr/bin/env python3
import os
import subprocess
import tempfile
import uuid
import shutil
import sys
import atexit
from pathlib import Path
from typing import Tuple, Optional, Final
from flask import Flask, request, send_file, jsonify, render_template
from werkzeug.utils import secure_filename
from werkzeug.datastructures import FileStorage
import threading
import time

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 1024 * 1024 * 1024  # 1GB max file size

# iOS uses: .mov (with H.264 or HEVC), .m4v
# Android uses: .mp4, .3gp, .webm
# Common formats: mp4, webm, gif, m4v, mov, 3gp
ALLOWED_EXTENSIONS: Final[set[str]] = {'mp4', 'webm', 'gif', 'm4v', 'mov', '3gp'}
SCRIPT_PATH: Final[str] = os.path.abspath('./convert.sh')
JOBS_DIR: Final[str] = tempfile.mkdtemp(prefix='convert_jobs_')
JOB_TIMEOUT: Final[int] = 1800  # 30 minutes
CONVERSION_TIMEOUT: Final[int] = 300  # 5 minutes

# Store original filenames for downloads
job_filenames: dict[str, str] = {}

def allowed_file(filename: str) -> bool:
    """Security: Validate file extension against whitelist"""
    return '.' in filename and \
           filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

def is_valid_job_id(job_id: str) -> bool:
    """Security: Validate job ID format (UUIDs only)"""
    try:
        uuid.UUID(job_id, version=4)
        return True
    except ValueError:
        return False

def get_mime_type(file_type: str) -> str:
    """
    Return MIME type for file_type.
    Assumes file_type is pre-validated as 'gif' or 'wav'.
    """
    if file_type == 'gif':
        return 'image/gif'
    else:  # Must be 'wav'
        return 'audio/wav'

def cleanup_job_directory(job_dir: str) -> None:
    """Shared cleanup logic for job directories"""
    try:
        if os.path.exists(job_dir):
            shutil.rmtree(job_dir, ignore_errors=True)
    except Exception:
        pass

def schedule_cleanup(job_id: str, job_dir: str) -> None:
    """Schedule job directory cleanup after timeout"""
    def cleanup() -> None:
        time.sleep(JOB_TIMEOUT)
        cleanup_job_directory(job_dir)
        # Clean up filename mapping
        if job_id in job_filenames:
            del job_filenames[job_id]

    thread = threading.Thread(target=cleanup, daemon=True)
    thread.start()

def cleanup_jobs_dir_on_shutdown() -> None:
    """Clean up all job directories on application shutdown"""
    try:
        if os.path.exists(JOBS_DIR):
            for job_id in os.listdir(JOBS_DIR):
                job_path = os.path.join(JOBS_DIR, job_id)
                if os.path.isdir(job_path):
                    cleanup_job_directory(job_path)
            # Remove the root JOBS_DIR itself
            os.rmdir(JOBS_DIR)
    except Exception:
        pass

# Register shutdown cleanup
atexit.register(cleanup_jobs_dir_on_shutdown)

@app.after_request
def set_security_headers(response):
    response.headers['X-Content-Type-Options'] = 'nosniff'
    response.headers['X-Frame-Options'] = 'DENY'
    response.headers['Content-Security-Policy'] = "default-src 'self'; style-src 'unsafe-inline' 'self'; script-src 'unsafe-inline' 'self'"
    return response

@app.errorhandler(413)
def request_entity_too_large(error):
    return jsonify({'error': 'File too large. Maximum size is 1GB'}), 413

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/upload', methods=['POST'])
def upload_file():
    # Validate file exists in request
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400

    file: FileStorage = request.files['file']

    # Validate filename
    if file.filename == '':
        return jsonify({'error': 'No file selected'}), 400

    # Security: Check file extension against whitelist
    if not allowed_file(file.filename):
        return jsonify({'error': f'File type not allowed. Allowed types: {", ".join(sorted(ALLOWED_EXTENSIONS))}'}), 400

    # Get include_sound parameter (default True for backwards compatibility)
    include_sound: bool = request.form.get('include_sound', 'true').lower() == 'true'

    job_dir: Optional[str] = None

    def cleanup_and_error(message: str, code: int = 500):
        """Helper to clean up job directory and return error"""
        if job_dir:
            cleanup_job_directory(job_dir)
        return jsonify({'error': message}), code

    try:
        # Generate unique job ID
        job_id: str = str(uuid.uuid4())

        # Store original filename (without extension) for download naming
        original_filename: str = secure_filename(file.filename)
        base_name_no_ext: str = os.path.splitext(original_filename)[0]
        job_filenames[job_id] = base_name_no_ext

        # Create secure temporary directory for this job
        job_dir = os.path.join(JOBS_DIR, job_id)
        os.makedirs(job_dir, mode=0o700)

        # Security: Use secure_filename to prevent directory traversal
        file_ext: str = original_filename.rsplit('.', 1)[1].lower()

        # Use standard library for secure temporary filename
        with tempfile.NamedTemporaryFile(
            mode='wb',
            suffix=f'.{file_ext}',
            dir=job_dir,
            delete=False
        ) as temp_file:
            input_path: str = temp_file.name
            file.save(input_path)

        # Extract base name right after file creation
        base_name: str = os.path.splitext(os.path.basename(input_path))[0]

        # Security: Use subprocess with list arguments (NOT shell=True)
        # Use absolute path to script since cwd is changed
        result = subprocess.run(
            [SCRIPT_PATH, input_path],
            cwd=job_dir,
            capture_output=True,
            text=True,
            timeout=CONVERSION_TIMEOUT
        )

        if result.returncode != 0:
            error_msg: str = result.stderr or 'Conversion failed'
            return cleanup_and_error(f'Conversion error: {error_msg}')

        # Clean up input file immediately after successful conversion
        try:
            if os.path.exists(input_path):
                os.remove(input_path)
        except OSError:
            pass

        # Use exact paths instead of searching
        gif_path: str = os.path.join(job_dir, f'{base_name}.gif')
        wav_path: str = os.path.join(job_dir, f'{base_name}.wav')

        # Verify GIF exists (always required)
        if not os.path.exists(gif_path):
            return cleanup_and_error('GIF output file not generated')

        # Verify GIF is not empty
        if os.path.getsize(gif_path) == 0:
            return cleanup_and_error('Generated GIF is empty or corrupted')

        # If sound not requested, delete WAV if it exists
        if not include_sound and os.path.exists(wav_path):
            os.remove(wav_path)

        # If sound requested, verify WAV exists
        if include_sound and not os.path.exists(wav_path):
            return cleanup_and_error('WAV output file not generated')

        # Verify WAV is not empty (if sound was requested)
        if include_sound and os.path.getsize(wav_path) == 0:
            return cleanup_and_error('Generated WAV is empty or corrupted')

        # Schedule cleanup
        schedule_cleanup(job_id, job_dir)

        return jsonify({
            'job_id': job_id,
            'message': 'Conversion successful',
            'has_sound': include_sound
        }), 200

    except subprocess.TimeoutExpired:
        return cleanup_and_error('Conversion timeout exceeded')
    except Exception as e:
        return cleanup_and_error(f'Server error: {str(e)}')

@app.route('/download/<job_id>/<file_type>')
def download_file(job_id: str, file_type: str):
    # Security: Validate job_id is a proper UUID
    if not is_valid_job_id(job_id):
        return jsonify({'error': 'Invalid job ID'}), 400

    # Security: Validate file_type is only 'gif' or 'wav'
    if file_type not in ['gif', 'wav']:
        return jsonify({'error': 'Invalid file type'}), 400

    # Security: Construct path safely using os.path.join
    job_dir: str = os.path.join(JOBS_DIR, job_id)

    # Security: Verify the directory exists and is within JOBS_DIR
    if not os.path.exists(job_dir):
        return jsonify({'error': 'Job not found'}), 404

    try:
        common = os.path.commonpath([os.path.abspath(job_dir), os.path.abspath(JOBS_DIR)])
        if common != os.path.abspath(JOBS_DIR):
            return jsonify({'error': 'Job not found'}), 404
    except ValueError:
        # Raised when paths are on different drives (Windows) or invalid
        return jsonify({'error': 'Job not found'}), 404

    # Find any file with the requested extension (there should only be one per type)
    files_in_dir: list[str] = [f for f in os.listdir(job_dir) if f.endswith(f'.{file_type}')]

    if not files_in_dir:
        return jsonify({'error': 'File not found'}), 404

    if len(files_in_dir) > 1:
        print(f'WARNING: Multiple {file_type} files found in job {job_id}', file=sys.stderr)

    file_path: str = os.path.join(job_dir, files_in_dir[0])

    # Verify file exists
    if not os.path.exists(file_path):
        return jsonify({'error': 'File not found'}), 404

    mime_type: str = get_mime_type(file_type)

    # Get original filename for download
    original_base: str = job_filenames.get(job_id, 'converted')
    download_name: str = f'{original_base}.{file_type}'

    return send_file(
        file_path,
        mimetype=mime_type,
        as_attachment=True,
        download_name=download_name
    )

if __name__ == '__main__':
    # Ensure convert.sh exists and is executable
    if not os.path.exists(SCRIPT_PATH):
        print(f"Error: {SCRIPT_PATH} not found", file=sys.stderr)
        exit(1)

    if not os.access(SCRIPT_PATH, os.X_OK):
        print(f"Error: {SCRIPT_PATH} is not executable. Run: chmod +x {SCRIPT_PATH}", file=sys.stderr)
        exit(1)

    # Run Flask app
    app.run(debug=False, host='127.0.0.1', port=5000)
