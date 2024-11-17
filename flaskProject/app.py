from flask import Flask, request, jsonify
from werkzeug.security import generate_password_hash, check_password_hash
from dotenv import load_dotenv
from datetime import datetime, timezone
from firebase_admin import credentials, messaging
from flask_jwt_extended import JWTManager, create_access_token, jwt_required, get_jwt_identity

import os
import psycopg2
import firebase_admin

# Load environment variables from .env
load_dotenv()

app = Flask(__name__)
app.config["JWT_SECRET_KEY"] = os.getenv("JWT_KEY")
jwt = JWTManager(app)

# Initialize Firebase
cred = credentials.Certificate("nwFB.json")  # replace with your actual path
firebase_admin.initialize_app(cred)

# Fetch values from environment variables
def get_db_connection():
    try:
        conn = psycopg2.connect(
            host=os.getenv("DB_HOST"),
            port=os.getenv("DB_PORT"),
            database=os.getenv("DB_NAME"),
            user=os.getenv("DB_USER"),
            password=os.getenv("DB_PASSWORD")
        )
    except Exception as error:
        print(f"Connection failed: {error}")
    return conn

@app.route('/api/signup', methods=['POST'])
def signup():
    data = request.get_json()
    username = data['username']
    email = data['email']
    password = data['password']
    first_name = data.get('first_name', '').strip().capitalize()  # Retrieve first name from request
    last_name = data.get('last_name', '').strip().capitalize()  # Retrieve last name from request
    fcm_token = data.get('fcm_token')  # Retrieve the token from the request

    # Hash the password
    password_hash = generate_password_hash(password)

    # Current UTC time for created_at
    created_at = datetime.now(timezone.utc)

    # Connect to AWS PostgreSQL and insert the user
    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute(
        """
        INSERT INTO user_info (username, email, password_hash, first_name, last_name, created_at, fcm_token)
        VALUES (%s, %s, %s, %s, %s, %s, %s) RETURNING user_id
        """,
        (username, email, password_hash, first_name, last_name, created_at, fcm_token)
    )
    conn.commit()
    user_id = cur.fetchone()[0]
    cur.close()
    conn.close()

    return jsonify({"user_id": user_id, "message": "User created successfully!"}), 201

@app.route('/api/login', methods=['POST'])
def login():
    data = request.get_json()
    username = data.get('username')
    password = data.get('password')
    fcm_token = data.get('fcm_token')  # Retrieve FCM token from client

    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute("SELECT user_id, password_hash FROM user_info WHERE username = %s", (username,))
    user = cur.fetchone()

    if user and check_password_hash(user[1], password):
        user_id = user[0]

        # Update FCM token and last login
        cur.execute("UPDATE user_info SET fcm_token = %s, last_login = %s WHERE user_id = %s",
                    (fcm_token, datetime.now(timezone.utc), user_id))
        conn.commit()

        # Create JWT token
        access_token = create_access_token(identity=user_id)
        cur.close()
        conn.close()

        return jsonify(access_token=access_token, message="Login successful"), 200
    else:
        cur.close()
        conn.close()
        return jsonify({"error": "Invalid credentials"}), 401

@app.route('/api/send_notification', methods=['POST'])
@jwt_required() # Makes having an authentication token required
def send_notification():
    current_user = get_jwt_identity()  # Get user ID from JWT token

    # Retrieve user's FCM token from the database
    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute("SELECT fcm_token FROM user_info WHERE user_id = %s", (current_user,))
    fcm_token = cur.fetchone()[0]
    cur.close()
    conn.close()

    # Send push notification if FCM token is available
    if fcm_token:
        message = messaging.Message(
            notification=messaging.Notification(
                title="Notification Title",
                body="Notification Body"
            ),
            token=fcm_token
        )
        response = messaging.send(message)
        return jsonify({"message": "Notification sent!", "response": response}), 200
    else:
        return jsonify({"error": "FCM token not found for user"}), 404

@app.route('/api/check_username', methods=['POST'])
def check_username():
    data = request.get_json()
    username = data.get('username')

    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute("SELECT COUNT(*) FROM user_info WHERE username = %s", (username,))
    is_unique = cur.fetchone()[0] == 0

    if not is_unique:
        # Generate suggested usernames
        suggestions = [f"{username}{i}" for i in range(1, 4)]
        return jsonify({"is_unique": False, "suggestions": suggestions}), 200

    return jsonify({"is_unique": True}), 200


# Can modify to be any type of action that requires authentication.
# @app.route('/api/protected_route', methods=['GET'])
# @jwt_required()
# def protected():
#     current_user = get_jwt_identity()  # Get the user ID from the token
#     return jsonify(logged_in_as=current_user), 200


# User Settings
# @app.route('/api/user_settings', methods=['GET'])
# @jwt_required()
# def user_settings():
#     current_user = get_jwt_identity()  # Get the user ID from the token
#
#     # Query the database for user settings
#     conn = get_db_connection()
#     cur = conn.cursor()
#     cur.execute("SELECT theme, notifications, language FROM user_settings WHERE user_id = %s", (current_user,))
#     settings = cur.fetchone()
#     cur.close()
#     conn.close()
#
#     if settings:
#         settings_data = {
#             "theme": settings[0],
#             "notifications": settings[1],
#             "language": settings[2]
#         }
#         return jsonify(settings_data), 200
#     else:
#         return jsonify({"error": "User settings not found"}), 404


if __name__ == '__main__':
    app.run()
