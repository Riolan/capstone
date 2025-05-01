# AI Image Upload + Labeling System

This project provides a Python-based pipeline for uploading images and annotations to AWS S3 and inserting their metadata into a PostgreSQL database hosted on AWS RDS. It uses COCO-style JSON annotations to populate image classification and bounding box tables for training AI models.

## Database Contains
- 27000+ labeled images
- Original annotation files in COCO format for the images
- The sql schema for the tables 

## ✅ Prerequisites

- AWS account with:
  - S3 bucket created
  - IAM user with S3 upload privileges
  - PostgreSQL RDS instance accessible to the current machine
- Python 3.8+
- Local folder containing COCO-annotated image datasets

---

## ⚙️ Requirements

- Python 3.8+
- AWS S3 Bucket with appropriate IAM access
- AWS RDS PostgreSQL instance
- `pip install -r requirements.txt`

## 📥 Download This Project

You can download the entire ready-to-use project as a ZIP file:

👉 [Download from Google Drive](https://drive.google.com/file/d/1iCkLuGFSDhz5BeUACqLc_YVIVc-pFxgn/view?usp=sharing)

> Make sure to unzip the file and follow the setup instructions below.

## 📦 Setup Instructions
1. **Clone the project or download the zip file**
2. **Unzip file and create and activate a virtual environment**
3. **Install dependencies**
   ```bash
   pip install -r requirements.txt
   
4. **Create a .env file based on .env.example and populate with your AWS and DB credentials**
5. **Recreate the PostgreSQL Schema Run this SQL using any client (e.g., pgAdmin or psql CLI):**
    ```bash
   \i sql/table.sql
6. **Run the upload script**
    ```bash
   python upload.py <dataset_type> <annotation_json_path>
   Example
   python upload.py train dataset/annotations/train.json

---

## 📄 `.env.example`

    ```env
    # AWS Credentials
    AWS_ACCESS_KEY=your_aws_access_key
    AWS_SECRET_KEY=your_aws_secret_key
    S3_BUCKET=your_bucket_name
    S3_FOLDER=uploaded_images/
    
    # PostgreSQL RDS
    DB_HOST=your-db-endpoint.rds.amazonaws.com
    DB_NAME=your_db_name
    DB_USER=your_db_user
    DB_PASSWORD=your_db_password



