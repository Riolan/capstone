# 🧠 AI Training Image Manager

This is a full-stack web application for managing and labeling AI training datasets. It enables users to upload image folders with COCO annotations, visualize stored images, view metadata (including classifications and bounding boxes), and delete data—all through a user-friendly interface.

---

## 🚀 Features

- Upload a batch of labeled images + annotation JSON to AWS S3 and PostgreSQL
- Paginated image viewer by dataset type (`train`, `valid`, `test`)
- View image metadata, classifications, and bounding boxes
- Delete images from both S3 and the database
- Built with Flask (backend) and React (frontend)

---

## 📥 Download This Project

You can download the entire ready-to-use project as a ZIP file:

👉 [Download from Google Drive](https://drive.google.com/file/d/1ZkDoL6KYrryD7PSETYVZN8-8nmLLzJsy/view?usp=sharing)

---

## 🛠 Tech Stack

- **Frontend**: React (JS)
- **Backend**: Flask (Python), Flask-CORS
- **Database**: PostgreSQL (hosted on AWS RDS)
- **Storage**: AWS S3
- **Image Processing**: Pillow
- **Environment Configuration**: `python-dotenv`

---

## ⚙️ Setup Instructions

### 1. Backend (Flask)

#### 🔧 Install dependencies

    ```bash
    pip install -r requirements.txt
### 2. Copy .env.example to .env and fill in your actual secrets:

### 3. ▶️ Run Flask API
    ```bash
    python app.py

### 1. Frontend (React)
**Requirements**
- Node.js 16+
- React + Axios

### 🔧 Setup
    ```bash
    cd frontend
    npm install
    npm start

